//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"

#include <algorithm>
#include <map>
#include <set>

// HE UL MU TXOP frame sequence.
//
// Implements the AP side of a Trigger-based UL OFDMA exchange:
//   1. Transmit a Basic or BSRP Trigger frame (IEEE 802.11-2024 26.5.2,
//      with Trigger frame fields from 9.3.1.22).
//   2. Collect simultaneous HE TB PPDU responses on the assigned RUs
//      (26.5.2.3 and 27.3.11.12).
//   3. Send a Multi-STA BlockAck acknowledging all received MPDUs
//      (9.3.1.8 and 26.4.2).
//
// Implementation notes:
//   - The response collection window is strict: it accepts only HE-TB frames
//     whose Trigger ID and RU index match the outstanding Trigger.  Late
//     responses are discarded, which is conservative with respect to the
//     standard timing rules.
//   - Per-MPDU receive status is taken from the PHY-layer MPDU receive
//     indication tag when available; delimiters without such a tag are assumed
//     successful if parseable.  This is an abstraction of the per-MPDU FCS
//     checking defined in Clause 27.3.13.

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mac/framesequence/GenericFrameSequences.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

Ieee80211MultiStaBlockAckRecord buildHeUlMultiStaBlockAckRecord(
        uint16_t aid, uint8_t tid,
        const std::vector<physicallayer::Ieee80211MpduReceiveResult>& outcomes)
{
    Ieee80211MultiStaBlockAckRecord record;
    record.aid = aid;
    record.tid = tid;
    record.responseReceived = true;
    bool startingSequenceKnown = false;
    for (const auto& outcome : outcomes) {
        bool structurallyValid = outcome.status == physicallayer::MPDU_SUCCESS ||
                outcome.status == physicallayer::MPDU_FCS_ERROR;
        if (!structurallyValid || outcome.sequenceNumber < 0 || outcome.tid != tid)
            continue;
        if (!startingSequenceKnown) {
            record.startingSequenceNumber = outcome.sequenceNumber;
            startingSequenceKnown = true;
        }
        int offset = (outcome.sequenceNumber - record.startingSequenceNumber + 4096) % 4096;
        if (outcome.status == physicallayer::MPDU_SUCCESS && offset < 64)
            record.bitmap |= UINT64_C(1) << offset;
    }
    return record;
}

namespace {

static uint8_t accessCategoryToAci(AccessCategory ac)
{
    switch (ac) {
        case AC_BE: return 0;
        case AC_BK: return 1;
        case AC_VI: return 2;
        case AC_VO: return 3;
        default: throw cRuntimeError("Invalid access category for Basic Trigger");
    }
}

class HeUlReceiveCollectionStep : public ReceiveCollectionStep
{
  protected:
    uint32_t triggerId;
    HeHcf *callback;
    std::vector<IIeee80211HeUlScheduler::RuAllocation> allocations;
    bool nfrp = false;
    uint16_t nfrpStartingAid = 0;
    int nfrpToneSetsPerSpatialStream = 0;
    int nfrpScheduledStaCount = 0;
    std::set<uint16_t> receivedAids;
    std::set<int> receivedRandomAccessRus;
    simtime_t firstResponseTime;
    simtime_t lastResponseTime;

  public:
    HeUlReceiveCollectionStep(uint32_t triggerId, HeHcf *callback,
            const IIeee80211HeUlScheduler::Schedule& schedule,
            IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
            simtime_t timeout, simtime_t commonDuration, simtime_t phyRxStartDelay) :
        ReceiveCollectionStep(timeout), triggerId(triggerId), callback(callback), allocations(schedule.allocations),
        nfrp(triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER),
        nfrpStartingAid(schedule.nfrpStartingAid)
    {
        ASSERT(callback != nullptr);
        ASSERT(nfrp || !allocations.empty());
        ASSERT(commonDuration > SIMTIME_ZERO);
        if (nfrp) {
            nfrpToneSetsPerSpatialStream = IIeee80211HeUlScheduler::getNfrpToneSetsPerSpatialStream(
                    schedule.channelBandwidth);
            nfrpScheduledStaCount = IIeee80211HeUlScheduler::getNfrpScheduledStaCount(
                    schedule.channelBandwidth, schedule.nfrpMultiplexingFlag);
        }
        firstResponseTime = simTime();
        lastResponseTime = firstResponseTime + commonDuration + timeout + phyRxStartDelay;
    }

    virtual bool acceptsHeaderlessFrame(const Packet *frame) const override
    {
        auto tag = frame == nullptr ? nullptr : frame->findTag<physicallayer::Ieee80211HeMuRxTag>();
        return nfrp && frame != nullptr && frame->getDataLength() == b(0) && tag != nullptr &&
                tag->getPpduFormat() == physicallayer::HE_TRIGGER_BASED_UPLINK &&
                tag->getAllocationsArraySize() == 1 && tag->getAllocations(0).ndpFeedbackReport;
    }

    virtual void setFrameToReceive(Packet *frame) override
    {
        auto tag = frame->findTag<physicallayer::Ieee80211HeMuRxTag>();
        if (tag == nullptr || tag->getPpduFormat() != physicallayer::HE_TRIGGER_BASED_UPLINK ||
                tag->getTriggerId() != triggerId || simTime() < firstResponseTime || simTime() > lastResponseTime) {
            // This collection window is intentionally strict: accepting a late
            // or foreign HE-TB PPDU could acknowledge a different Trigger.
            EV_INFO << "Discarding HE UL response outside Trigger " << triggerId
                     << " collection window\n";
            delete frame;
            return;
        }
        uint16_t aid = 0;
        if (nfrp) {
            if (frame->getDataLength() != b(0) || tag->getAllocationsArraySize() != 1) {
                EV_INFO << "Discarding malformed NFRP feedback response for Trigger " << triggerId << "\n";
                delete frame;
                return;
            }
            const auto& report = tag->getAllocations(0);
            if (!report.ndpFeedbackReport || report.ndpFeedbackStatus > 1 ||
                    report.ndpRuToneSetIndex < 1 ||
                    report.ndpRuToneSetIndex > nfrpToneSetsPerSpatialStream ||
                    report.ndpStartingStsNumber > 1) {
                EV_INFO << "Discarding invalid NFRP report metadata for Trigger " << triggerId << "\n";
                delete frame;
                return;
            }
            const int offset = report.ndpStartingStsNumber * nfrpToneSetsPerSpatialStream +
                    report.ndpRuToneSetIndex - 1;
            if (offset >= nfrpScheduledStaCount || nfrpStartingAid + offset > 4095) {
                EV_INFO << "Discarding out-of-range NFRP report for Trigger " << triggerId << "\n";
                delete frame;
                return;
            }
            aid = nfrpStartingAid + offset;
            EV_INFO << "Collected NFRP feedback report: trigger=" << triggerId
                    << ", aid=" << aid << ", status=" << (int)report.ndpFeedbackStatus
                    << ", toneSet=" << (int)report.ndpRuToneSetIndex
                    << ", startingSts=" << (int)report.ndpStartingStsNumber << "\n";
        }
        bool randomAccess = false;
        if (!nfrp) {
            if (tag->getAllocationsArraySize() != 1 ||
                    tag->getAllocations(0).ndpFeedbackReport) {
                EV_INFO << "Discarding HE UL response without one canonical TB allocation\n";
                delete frame;
                return;
            }
            const auto& canonical = tag->getAllocations(0);
            for (const auto& allocation : allocations) {
                if (tag->getRuIndex() != allocation.ru.index ||
                        canonical.ruIndex != allocation.ru.index)
                    continue;
                if (!allocation.randomAccess &&
                        canonical.staId == allocation.associationId)
                    aid = allocation.associationId;
                else if (allocation.randomAccess)
                    randomAccess = true;
                if (aid != 0 || randomAccess)
                    break;
            }
        }
        if ((!nfrp && aid == 0 && !randomAccess) ||
                (aid != 0 && receivedAids.count(aid) != 0) ||
                (randomAccess && receivedRandomAccessRus.count(tag->getRuIndex()) != 0)) {
            EV_INFO << "Discarding " << (aid == 0 ? "unallocated" : "duplicate")
                     << " HE UL response for Trigger " << triggerId << "\n";
            delete frame;
            return;
        }
        if (aid != 0)
            receivedAids.insert(aid);
        if (randomAccess)
            receivedRandomAccessRus.insert(tag->getRuIndex());
        EV_INFO << "Collected HE UL response: trigger=" << triggerId
                 << ", aid=" << (randomAccess ? 0 : aid) << ", RU=" << tag->getRuIndex() << "\n";
        ReceiveCollectionStep::setFrameToReceive(frame);
    }
};

} // namespace

HeUlMuTxOpFs::HeUlMuTxOpFs(HeUlCoordinator *coordinator, HeHcf *callback,
        const HeUlMuPlan& plan,
        physicallayer::Ieee80211ModeSet *modeSet,
        const MacAddress& apAddress) :
    coordinator(coordinator),
    callback(callback),
    plan(plan),
    schedule(this->plan.getSchedule()),
    triggerType(this->plan.getTriggerType()),
    modeSet(modeSet),
    apAddress(apAddress),
    // G.5 HE sequences
    // he-ul-mu-sequence =
    //   ( Basic Trigger ) |
    //   ( Basic Trigger + a-mpdu + mu-user-respond + a-mpdu-end )
    //   1{Data[+HTC] + QoS + (no-ack | block-ack) + a-mpdu} + a-mpdu-end |
    //   MU-BAR Trigger BlockAck;
    // Implemented AP-side exchange:
    //   ( Basic Trigger | BSRP Trigger ) HE-TB-PPDU Multi-STA-BlockAck;
    // The Trigger and HE TB PPDU exchange follows the G.5 UL MU sequence.  The
    // following Multi-STA BlockAck is the AP response defined by 26.4.4.5.
    sequence(new SequentialFs({new StepFs("HE-TRIGGER",
                                          [this](StepFs *, FrameSequenceContext *context) {
                                              return new TransmitStep(buildTriggerPacket(), context->getIfs(), true);
                                          }),
                               new StepFs("HE-TB-PPDU",
                                          [this](StepFs *, FrameSequenceContext *context) {
                                              return this->buildReceiveCollectionStep();
                                          },
                                          [this](StepFs *, FrameSequenceContext *context) {
                                              processResponses(context);
                                              return true;
                                          }),
                               new OptionalFs(new StepFs("MULTI-STA-BA",
                                          [this](StepFs *, FrameSequenceContext *context) {
                                              return new TransmitStep(buildMultiStaBlockAckPacket(), this->modeSet->getSifsTime(), true);
                                          }), [this](OptionalFs *, FrameSequenceContext *) {
                                              return this->triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER;
                                          })}))
{
    ASSERT(coordinator != nullptr);
    ASSERT(callback != nullptr);
    ASSERT(modeSet != nullptr);
    triggerId = coordinator->allocateTriggerId();
}

IReceiveStep *HeUlMuTxOpFs::buildReceiveCollectionStep() const
{
    return new HeUlReceiveCollectionStep(triggerId, callback, schedule, triggerType,
            modeSet->getSifsTime() + schedule.commonDuration + modeSet->getSlotTime(),
            schedule.commonDuration, modeSet->getPhyRxStartDelay());
}

Packet *HeUlMuTxOpFs::buildTriggerPacket() const
{
    // IEEE 802.11-2024 9.3.1.22 defines the Trigger Common/User Info fields:
    // Trigger Type, UL Length/common duration, RU Allocation, UL HE-MCS, coding,
    // and UL Target Receive Power.  26.5.2.2 allows the AP to solicit one or
    // more HE TB PPDU responses by addressing User Info fields by AID or RA-RU.
    ASSERT(triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER || !schedule.allocations.empty());
    ASSERT(schedule.commonDuration > SIMTIME_ZERO);
    auto header = makeShared<Ieee80211TriggerFrame>();
    header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    header->setTransmitterAddress(apAddress);
    header->setTriggerType(triggerType);
    header->setTriggerId(triggerId);
    header->setUlLength(schedule.ulLength);
    header->setCommonDuration(schedule.commonDuration);
    header->setCommonDurationExact(schedule.commonDurationExact);
    header->setChannelBandwidthMhz(std::lround(schedule.channelBandwidth.get() / 1e6));
    header->setNoSignalExtension(schedule.noSignalExtension);
    header->setGuardInterval(schedule.guardInterval);
    header->setLtfType(schedule.ltfType);
    header->setCoding(schedule.coding);
    header->setLdpcExtraSymbolSegment(schedule.ldpcExtraSymbolSegment);
    header->setPreFecPaddingFactor(schedule.preFecPaddingFactor);
    header->setPeDisambiguity(schedule.peDisambiguity);
    header->setNumberOfHeLtfSymbols(schedule.numberOfHeLtfSymbols);
    header->setApTxPowerDbm(schedule.apTxPowerDbm);
    header->setPacketExtensionDurationUs(schedule.packetExtensionDurationUs);
    header->setPuncturedSubchannelMask(schedule.puncturedSubchannelMask);
    header->setNfrpStartingAid(schedule.nfrpStartingAid);
    header->setNfrpFeedbackType(schedule.nfrpFeedbackType);
    header->setNfrpTargetRssiDbm(schedule.nfrpTargetRssiDbm);
    header->setNfrpUseMaximumTransmitPower(schedule.nfrpUseMaximumTransmitPower);
    header->setNfrpMultiplexingFlag(schedule.nfrpMultiplexingFlag);
    header->setUsersArraySize(triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ? 0 : schedule.allocations.size());
    for (size_t i = 0; i < schedule.allocations.size(); i++) {
        const auto& allocation = schedule.allocations[i];
        Ieee80211HeTriggerUserInfo user;
        user.aid = allocation.associationId;
        user.ruIndex = allocation.ru.index;
        user.ruToneSize = allocation.ru.toneSize;
        user.ruToneOffset = allocation.ru.toneOffset;
        user.mcs = allocation.mcs;
        user.coding = schedule.coding;
        user.numberOfSpatialStreams = allocation.numberOfSpatialStreams;
        user.streamStartIndex = allocation.streamStartIndex;
        user.muMimo = allocation.muMimo;
        user.tid = allocation.tid;
        user.tidAggregationLimit = 1; // single-TID HE TB A-MPDU model
        user.preferredAc = accessCategoryToAci(allocation.accessCategory);
        user.targetRssiDbm = allocation.targetRssiDbm;
        user.useMaximumTransmitPower = allocation.useMaximumTransmitPower;
        user.randomAccess = allocation.randomAccess;
        if (triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER)
            header->setUsers(i, user);
    }
    // 9.3.1.22 says the Trigger Duration field follows 9.2.5.  Here it covers
    // the SIFS-delayed HE TB response and the AP's following SIFS response.
    auto responseDuration = modeSet->getSifsTime() + schedule.commonDuration;
    // Keep non-participating STAs' NAV set through the AP response. Without
    // the Multi-STA BlockAck airtime, their NAV expired exactly when the AP
    // began transmitting the BlockAck and an EDCA frame could arrive while
    // the frame-sequence state was still in its transmit step.
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER)
        header->setDurationField(responseDuration);
    else {
        auto maxBlockAckLength = B(18 + 12 * schedule.allocations.size() + 4);
        auto blockAckDuration = modeSet->getSlowestMandatoryMode()->getDuration(maxBlockAckLength);
        header->setDurationField(responseDuration + modeSet->getSifsTime() + blockAckDuration);
    }
    const int userInfoSize = triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? 5 : 6;
    header->setChunkLength(triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ?
            B(30) : B(24 + userInfoSize * schedule.allocations.size()));
    auto packet = new Packet(triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? "HE-BSRP-Trigger" :
            triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ? "HE-NFRP-Trigger" : "HE-Basic-Trigger", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

void HeUlMuTxOpFs::processResponses(FrameSequenceContext *context)
{
    // IEEE 802.11-2024 26.5.2.3 requires HE TB responses to use the Trigger's
    // RU, MCS, coding, GI, and duration parameters.  The simulated Trigger ID is
    // not a standard field; it is a model correlation key layered on top of the
    // standard RU/AID matching rules.
    ASSERT(context != nullptr);
    ackRecords.clear();
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        auto collection = check_and_cast<ReceiveCollectionStep *>(context->getLastStep());
        EV_INFO << "HE UL NFRP response processing: reports="
                << collection->getReceivedFrames().size() << ", no acknowledgment scheduled\n";
        return;
    }
    for (const auto& allocation : schedule.allocations) {
        if (allocation.randomAccess)
            continue;
        Ieee80211MultiStaBlockAckRecord record;
        record.aid = allocation.associationId;
        record.tid = allocation.tid;
        record.responseReceived = false;
        ackRecords.push_back(record);
    }

    auto collection = check_and_cast<ReceiveCollectionStep *>(context->getLastStep());
    std::map<uint16_t, std::vector<physicallayer::Ieee80211MpduReceiveResult>> receivedOutcomes;
    std::map<uint16_t, uint8_t> receivedTids;
    std::set<uint16_t> responders;
    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
            Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    for (auto packet : collection->getReceivedFrames()) {
        auto rxTag = packet->findTag<physicallayer::Ieee80211HeMuRxTag>();
        if (rxTag == nullptr || rxTag->getAllocationsArraySize() != 1)
            continue;
        const auto& canonical = rxTag->getAllocations(0);
        const IIeee80211HeUlScheduler::RuAllocation *matchedAllocation = nullptr;
        for (const auto& allocation : schedule.allocations)
            if (allocation.ru.index == canonical.ruIndex &&
                    (allocation.randomAccess || allocation.associationId == canonical.staId)) {
                matchedAllocation = &allocation;
                break;
            }
        if (matchedAllocation == nullptr)
            continue;

        uint16_t aid = matchedAllocation->randomAccess ? 0 : matchedAllocation->associationId;
        uint8_t tid = matchedAllocation->tid;
        bool responseIdentityKnown = aid != 0;
        std::vector<std::pair<Packet *, physicallayer::Ieee80211MpduReceiveResult>> decodedMpdus;

        if (dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront()) != nullptr) {
            EV_WARN << "Ignoring data-bearing HE TB response without an A-MPDU delimiter\n";
            continue;
        }

        auto parser = packet->dup();
        auto receiveInd = packet->findTag<physicallayer::Ieee80211MpduReceiveInd>();
        unsigned int resultIndex = 0;
        while (parser->getDataLength() > b(0) &&
                dynamicPtrCast<const Ieee80211MpduSubframeHeader>(parser->peekAtFront()) != nullptr) {
            auto delimiter = parser->popAtFront<Ieee80211MpduSubframeHeader>(b(-1), parsingFlags);
            B length(delimiter->getLength());
            if (length == B(0))
                continue;
            physicallayer::Ieee80211MpduReceiveResult outcome;
            outcome.length = length;
            outcome.status = delimiter->isIncorrect() ?
                    physicallayer::MPDU_DELIMITER_ERROR : physicallayer::MPDU_NOT_EVALUATED;
            if (receiveInd != nullptr && resultIndex < receiveInd->getResultsArraySize())
                outcome = receiveInd->getResults(resultIndex);
            if (length > parser->getDataLength())
                outcome.status = physicallayer::MPDU_PAYLOAD_ERROR;
            else {
                auto mpdu = new Packet(parser->getName());
                mpdu->insertAtBack(parser->popAtFront(length, parsingFlags));
                auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                        mpdu->peekAtFront<Ieee80211MacHeader>());
                if (header == nullptr && (outcome.status == physicallayer::MPDU_SUCCESS ||
                        outcome.status == physicallayer::MPDU_FCS_ERROR))
                    outcome.status = physicallayer::MPDU_HEADER_ERROR;
                decodedMpdus.emplace_back(mpdu, outcome);
            }
            int padding = (4 - (B(4) + length).get<B>() % 4) % 4;
            if (padding != 0 && parser->getDataLength() >= B(padding))
                parser->popAtFront(B(padding), parsingFlags);
            resultIndex++;
        }
        delete parser;

        // Scheduled allocation identity is supplied by the Trigger/RXVECTOR
        // even when every MPDU fails its FCS. An AID12=0 random-access RU has
        // no such identity: resolve it only from a successful associated MPDU.
        bool identityConflict = false;
        for (const auto& decoded : decodedMpdus) {
            auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                    decoded.first->peekAtFront<Ieee80211MacHeader>());
            if (decoded.second.status != physicallayer::MPDU_SUCCESS || header == nullptr)
                continue;
            auto decodedAid = callback->getAssociationId(header->getTransmitterAddress());
            if (decodedAid == 0) {
                identityConflict = true;
                break;
            }
            if (matchedAllocation->randomAccess && !responseIdentityKnown) {
                aid = decodedAid;
                tid = header->getTid();
                responseIdentityKnown = true;
            }
            else if (responseIdentityKnown &&
                    (decodedAid != aid || header->getTid() != tid)) {
                identityConflict = true;
                break;
            }
        }
        if (responseIdentityKnown && !identityConflict) {
            responders.insert(aid);
            receivedTids[aid] = tid;
            for (auto& decoded : decodedMpdus) {
                auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                        decoded.first->peekAtFront<Ieee80211MacHeader>());
                auto outcome = decoded.second;
                if (outcome.tid == tid)
                    receivedOutcomes[aid].push_back(outcome);
                if (outcome.status == physicallayer::MPDU_SUCCESS && header != nullptr &&
                        callback->getAssociationId(header->getTransmitterAddress()) == aid)
                    callback->processTriggeredUlFrame(decoded.first->dup(), header, aid);
            }
        }
        for (auto& decoded : decodedMpdus)
            delete decoded.first;
    }

    for (auto& record : ackRecords)
        if (responders.count(record.aid) != 0)
            record = buildHeUlMultiStaBlockAckRecord(record.aid, record.tid,
                    receivedOutcomes[record.aid]);
    for (auto aid : responders) {
        auto record = std::find_if(ackRecords.begin(), ackRecords.end(),
                [aid] (const auto& value) { return value.aid == aid; });
        if (record == ackRecords.end())
            ackRecords.push_back(buildHeUlMultiStaBlockAckRecord(aid,
                    receivedTids[aid], receivedOutcomes[aid]));
    }
    EV_INFO << "HE UL response processing: received=" << responders.size()
             << ", block-ack records=" << ackRecords.size() << "\n";
}

Packet *HeUlMuTxOpFs::buildMultiStaBlockAckPacket() const
{
    // IEEE 802.11-2024 26.4.4.5 says an AP receiving HE TB PPDUs from more
    // than one STA may respond with a Multi-STA BlockAck carried in an SU PPDU.
    // 26.4.2 defines the per-AID/TID acknowledgment context; 9.3.1.8 defines
    // the BlockAck frame format.
    auto header = makeShared<Ieee80211MultiStaBlockAck>();
    header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    header->setTransmitterAddress(apAddress);
    header->setDurationField(SIMTIME_ZERO);
    header->setRecordsArraySize(ackRecords.size());
    for (size_t i = 0; i < ackRecords.size(); i++)
        header->setRecords(i, ackRecords[i]);
    // IEEE Std 802.11-2024 Figures 9-58..9-60 encode each associated STA's
    // block-ack context as a 12-octet Per AID TID Info subfield: AID/TID,
    // Starting Sequence Control, and an 8-octet bitmap.
    header->setChunkLength(B(18 + 12 * ackRecords.size()));
    auto packet = new Packet("HE-Multi-STA-BlockAck", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    packet->addTag<physicallayer::Ieee80211HeTriggerCorrelationTag>()->
            setTriggerId(triggerId);
    return packet;
}

void HeUlMuTxOpFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    ASSERT(context != nullptr);
    ASSERT(sequence != nullptr);
    step = 0;
    sequence->startSequence(context, firstStep);
    EV_INFO << "Starting HE UL " << (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? "BSRP" : "Basic")
             << " Trigger " << triggerId << " with " << schedule.allocations.size() << " RU allocations\n";
    if (triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER)
        coordinator->commitSchedule(schedule);
    coordinator->noteTriggerSent(triggerType);
}

IFrameSequenceStep *HeUlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    ASSERT(sequence != nullptr);
    return sequence->prepareStep(context);
}

bool HeUlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    ASSERT(sequence != nullptr);
    step++;
    return sequence->completeStep(context);
}

std::string HeUlMuTxOpFs::getHistory() const
{
    return step == -1 ? "HE-UL-MU" : sequence->getHistory();
}

} // namespace ieee80211
} // namespace inet
