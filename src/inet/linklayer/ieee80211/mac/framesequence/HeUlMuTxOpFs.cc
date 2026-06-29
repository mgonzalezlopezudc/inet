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

namespace {

class HeUlReceiveCollectionStep : public ReceiveCollectionStep
{
  protected:
    uint32_t triggerId;
    HeHcf *callback;
    std::vector<IIeee80211HeUlScheduler::RuAllocation> allocations;
    std::set<uint16_t> receivedAids;
    simtime_t firstResponseTime;
    simtime_t lastResponseTime;

    const Ieee80211DataHeader *findDataHeader(const Packet *packet) const
    {
        constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
        if (auto header = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront<Ieee80211MacHeader>()))
            return header.get();
        if (dynamicPtrCast<const Ieee80211MpduSubframeHeader>(packet->peekAtFront()) == nullptr)
            return nullptr;
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekDataAt(B(4), B(-1), parsingFlags));
        return header == nullptr ? nullptr : header.get();
    }

  public:
    HeUlReceiveCollectionStep(uint32_t triggerId, HeHcf *callback,
            const std::vector<IIeee80211HeUlScheduler::RuAllocation>& allocations,
            simtime_t timeout, simtime_t commonDuration, simtime_t phyRxStartDelay) :
        ReceiveCollectionStep(timeout), triggerId(triggerId), callback(callback), allocations(allocations)
    {
        ASSERT(callback != nullptr);
        ASSERT(!allocations.empty());
        ASSERT(commonDuration > SIMTIME_ZERO);
        firstResponseTime = simTime();
        lastResponseTime = firstResponseTime + commonDuration + timeout + phyRxStartDelay;
    }

    virtual void setFrameToReceive(Packet *frame) override
    {
        auto tag = frame->findTag<physicallayer::Ieee80211HeMuRxTag>();
        auto header = findDataHeader(frame);
        if (tag == nullptr || header == nullptr || tag->getPpduFormat() != physicallayer::HE_TRIGGER_BASED_UPLINK ||
                tag->getTriggerId() != triggerId || simTime() < firstResponseTime || simTime() > lastResponseTime) {
            // This collection window is intentionally strict: accepting a late
            // or foreign HE-TB PPDU could acknowledge a different Trigger.
            EV_INFO << "Discarding HE UL response outside Trigger " << triggerId
                     << " collection window\n";
            delete frame;
            return;
        }
        uint16_t aid = 0;
        for (const auto& allocation : allocations)
            if ((!allocation.randomAccess && allocation.staAddress == header->getTransmitterAddress()) ||
                    (allocation.randomAccess && tag->getRuIndex() == allocation.ru.index)) {
                aid = allocation.associationId;
                if (allocation.randomAccess)
                    aid = callback->getAssociationId(header->getTransmitterAddress());
                bool ruMatches = tag->getRuIndex() == allocation.ru.index;
                if (!allocation.randomAccess && !ruMatches) {
                    EV_INFO << "Discarding scheduled HE UL response with unexpected RU "
                             << tag->getRuIndex() << " for AID " << aid << "\n";
                    delete frame;
                    return;
                }
                if (allocation.randomAccess && !ruMatches) {
                    EV_INFO << "Discarding random-access HE UL response with unexpected RU "
                             << tag->getRuIndex() << "\n";
                    delete frame;
                    return;
                }
                break;
            }
        if (aid == 0 || receivedAids.count(aid) != 0) {
            EV_INFO << "Discarding " << (aid == 0 ? "unallocated" : "duplicate")
                     << " HE UL response for Trigger " << triggerId << "\n";
            delete frame;
            return;
        }
        receivedAids.insert(aid);
        EV_INFO << "Collected HE UL response: trigger=" << triggerId
                 << ", aid=" << aid << ", RU=" << tag->getRuIndex() << "\n";
        ReceiveCollectionStep::setFrameToReceive(frame);
    }
};

} // namespace

HeUlMuTxOpFs::HeUlMuTxOpFs(HeUlCoordinator *coordinator, HeHcf *callback,
        const IIeee80211HeUlScheduler::Schedule& schedule,
        IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
        physicallayer::Ieee80211ModeSet *modeSet,
        const MacAddress& apAddress) :
    coordinator(coordinator),
    callback(callback),
    schedule(schedule),
    triggerType(triggerType),
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
                                              return new HeUlReceiveCollectionStep(this->triggerId, this->callback, this->schedule.allocations,
                                                      this->modeSet->getSifsTime() + this->schedule.commonDuration + this->modeSet->getSlotTime(),
                                                      this->schedule.commonDuration, this->modeSet->getPhyRxStartDelay());
                                          },
                                          [this](StepFs *, FrameSequenceContext *context) {
                                              processResponses(context);
                                              return true;
                                          }),
                               new StepFs("MULTI-STA-BA",
                                          [this](StepFs *, FrameSequenceContext *context) {
                                              return new TransmitStep(buildMultiStaBlockAckPacket(), this->modeSet->getSifsTime(), true);
                                          })}))
{
    ASSERT(coordinator != nullptr);
    ASSERT(callback != nullptr);
    ASSERT(modeSet != nullptr);
    ASSERT(!schedule.allocations.empty());
    ASSERT(schedule.commonDuration > SIMTIME_ZERO);
    ASSERT(triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER ||
            triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER);
    triggerId = coordinator->allocateTriggerId();
}

Packet *HeUlMuTxOpFs::buildTriggerPacket() const
{
    // IEEE 802.11-2024 9.3.1.22 defines the Trigger Common/User Info fields:
    // Trigger Type, UL Length/common duration, RU Allocation, UL HE-MCS, coding,
    // and UL Target Receive Power.  26.5.2.2 allows the AP to solicit one or
    // more HE TB PPDU responses by addressing User Info fields by AID or RA-RU.
    ASSERT(!schedule.allocations.empty());
    ASSERT(schedule.commonDuration > SIMTIME_ZERO);
    auto header = makeShared<Ieee80211TriggerFrame>();
    header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    header->setTransmitterAddress(apAddress);
    header->setTriggerType(triggerType);
    header->setTriggerId(triggerId);
    header->setCommonDuration(schedule.commonDuration);
    header->setGuardInterval(schedule.guardInterval);
    header->setCoding(schedule.coding);
    header->setPacketExtensionDurationUs(schedule.packetExtensionDurationUs);
    header->setPuncturedSubchannelMask(schedule.puncturedSubchannelMask);
    header->setUsersArraySize(schedule.allocations.size());
    for (size_t i = 0; i < schedule.allocations.size(); i++) {
        const auto& allocation = schedule.allocations[i];
        Ieee80211HeTriggerUserInfo user;
        user.aid = allocation.associationId;
        user.ruIndex = allocation.ru.index;
        user.ruToneSize = allocation.ru.toneSize;
        user.ruToneOffset = allocation.ru.toneOffset;
        user.mcs = allocation.mcs;
        user.tid = allocation.tid;
        user.targetRssiDbm = allocation.targetRssiDbm;
        user.randomAccess = allocation.randomAccess;
        header->setUsers(i, user);
    }
    // 9.3.1.22 says the Trigger Duration field follows 9.2.5.  Here it covers
    // the SIFS-delayed HE TB response and the AP's following SIFS response.
    auto responseDuration = modeSet->getSifsTime() + schedule.commonDuration;
    header->setDurationField(responseDuration + modeSet->getSifsTime());
    int userInfoSize = (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) ? 5 : 6;
    header->setChunkLength(B(24 + userInfoSize * schedule.allocations.size()));
    auto packet = new Packet(triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ?
            "HE-BSRP-Trigger" : "HE-Basic-Trigger", header);
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
    std::map<uint16_t, std::vector<int>> successfulSequences;
    std::set<uint16_t> responders;
    auto processMpdu = [&] (Packet *mpdu, physicallayer::Ieee80211MpduReceiveStatus status) {
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(mpdu->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || status != physicallayer::MPDU_SUCCESS)
            return;
        auto aid = callback->getAssociationId(header->getTransmitterAddress());
        if (aid == 0)
            return;
        auto record = std::find_if(ackRecords.begin(), ackRecords.end(),
                [aid] (const auto& value) { return value.aid == aid; });
        if (record == ackRecords.end()) {
            // 9.3.1.22 Table 9-52 allows AID12=0 RA-RUs for associated STAs.
            // The collection step already checked that this response selected a
            // usable RA allocation; only then may an unscheduled sender gain a
            // Multi-STA BA record.
            Ieee80211MultiStaBlockAckRecord value;
            value.aid = aid;
            value.tid = header->getTid();
            ackRecords.push_back(value);
            record = std::prev(ackRecords.end());
        }
        if (record->tid != header->getTid())
            return;
        responders.insert(aid);
        record->responseReceived = true;
        if (header->getType() != ST_QOS_NULL) {
            successfulSequences[aid].push_back(header->getSequenceNumber().get());
            callback->processTriggeredUlFrame(mpdu->dup(), header, aid);
        }
        else
            callback->processTriggeredUlFrame(mpdu->dup(), header, aid);
    };

    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
            Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    for (auto packet : collection->getReceivedFrames()) {
        if (dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront<Ieee80211MacHeader>()) != nullptr) {
            processMpdu(packet, physicallayer::MPDU_SUCCESS);
            continue;
        }
        auto parser = packet->dup();
        auto receiveInd = packet->findTag<physicallayer::Ieee80211MpduReceiveInd>();
        unsigned int resultIndex = 0;
        while (parser->getDataLength() > b(0) &&
                dynamicPtrCast<const Ieee80211MpduSubframeHeader>(parser->peekAtFront()) != nullptr) {
            auto delimiter = parser->popAtFront<Ieee80211MpduSubframeHeader>(B(-1), parsingFlags);
            auto status = delimiter->isIncorrect() ? physicallayer::MPDU_DELIMITER_ERROR : physicallayer::MPDU_SUCCESS;
            if (receiveInd != nullptr && resultIndex < receiveInd->getResultsArraySize())
                status = receiveInd->getResults(resultIndex).status;
            B length(delimiter->getLength());
            if (length > parser->getDataLength())
                status = physicallayer::MPDU_PAYLOAD_ERROR;
            else {
                auto mpdu = new Packet(parser->getName());
                mpdu->insertAtBack(parser->popAtFront(length, parsingFlags));
                processMpdu(mpdu, status);
                delete mpdu;
            }
            int padding = (4 - (B(4) + length).get<B>() % 4) % 4;
            if (padding != 0 && parser->getDataLength() >= B(padding))
                parser->popAtFront(B(padding), parsingFlags);
            resultIndex++;
        }
        delete parser;
    }
    for (auto& record : ackRecords) {
        auto it = successfulSequences.find(record.aid);
        if (it == successfulSequences.end() || it->second.empty())
            continue;
        record.startingSequenceNumber = it->second.front();
        for (int sequenceNumber : it->second) {
            int offset = (sequenceNumber - record.startingSequenceNumber + 4096) % 4096;
            if (offset < 64)
                record.bitmap |= UINT64_C(1) << offset;
        }
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
