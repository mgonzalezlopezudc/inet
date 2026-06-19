//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"

#include <algorithm>
#include <map>
#include <set>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
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
        firstResponseTime = simTime();
        lastResponseTime = firstResponseTime + commonDuration + timeout + phyRxStartDelay;
    }

    virtual void setFrameToReceive(Packet *frame) override
    {
        auto tag = frame->findTag<physicallayer::Ieee80211HeMuRxTag>();
        auto header = findDataHeader(frame);
        if (tag == nullptr || header == nullptr || tag->getPpduFormat() != physicallayer::HE_TRIGGER_BASED_UPLINK ||
                tag->getTriggerId() != triggerId || simTime() < firstResponseTime || simTime() > lastResponseTime) {
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
                    delete frame;
                    return;
                }
                if (allocation.randomAccess && !ruMatches) {
                    delete frame;
                    return;
                }
                break;
            }
        if (aid == 0 || receivedAids.count(aid) != 0) {
            delete frame;
            return;
        }
        receivedAids.insert(aid);
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
    apAddress(apAddress)
{
    triggerId = coordinator->allocateTriggerId();
}

Packet *HeUlMuTxOpFs::buildTriggerPacket() const
{
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
    auto responseDuration = modeSet->getSifsTime() + schedule.commonDuration;
    header->setDurationField(responseDuration + modeSet->getSifsTime());
    bool extendedControls = schedule.guardInterval != physicallayer::HE_GI_3_2_US ||
            schedule.coding != physicallayer::HE_CODING_BCC || schedule.packetExtensionDurationUs != 0 ||
            schedule.puncturedSubchannelMask != 0;
    header->setChunkLength(B((extendedControls ? 30 : 26) + 12 * schedule.allocations.size()));
    auto packet = new Packet(triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ?
            "HE-BSRP-Trigger" : "HE-Basic-Trigger", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

void HeUlMuTxOpFs::processResponses(FrameSequenceContext *context)
{
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
            // The collection step already checked that this is a usable RA
            // allocation.  Only then may an unscheduled sender gain a record.
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
}

Packet *HeUlMuTxOpFs::buildMultiStaBlockAckPacket() const
{
    auto header = makeShared<Ieee80211MultiStaBlockAck>();
    header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    header->setTransmitterAddress(apAddress);
    header->setDurationField(SIMTIME_ZERO);
    header->setRecordsArraySize(ackRecords.size());
    for (size_t i = 0; i < ackRecords.size(); i++)
        header->setRecords(i, ackRecords[i]);
    header->setChunkLength(b(152 + 112 * ackRecords.size()));
    auto packet = new Packet("HE-Multi-STA-BlockAck", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

void HeUlMuTxOpFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    step = 0;
    coordinator->noteTriggerSent(triggerType);
}

IFrameSequenceStep *HeUlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            return new TransmitStep(buildTriggerPacket(), context->getIfs(), true);
        case 1:
            return new HeUlReceiveCollectionStep(triggerId, callback, schedule.allocations,
                    modeSet->getSifsTime() + schedule.commonDuration + modeSet->getSlotTime(),
                    schedule.commonDuration, modeSet->getPhyRxStartDelay());
        case 2:
            return new TransmitStep(buildMultiStaBlockAckPacket(), modeSet->getSifsTime(), true);
        case 3:
            return nullptr;
        default:
            throw cRuntimeError("Invalid HE UL MU frame sequence step");
    }
}

bool HeUlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    if (step == 1)
        processResponses(context);
    step++;
    return true;
}

std::string HeUlMuTxOpFs::getHistory() const
{
    switch (step) {
        case 0: return "HE-TRIGGER";
        case 1: return "HE-TB-COLLECT";
        case 2: return "MULTI-STA-BA";
        default: return "HE-UL-MU";
    }
}

} // namespace ieee80211
} // namespace inet
