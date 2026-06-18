//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"

#include <algorithm>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"

namespace inet {
namespace ieee80211 {

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
    header->setChunkLength(B(26 + 12 * schedule.allocations.size()));
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
    for (auto packet : collection->getReceivedFrames()) {
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr)
            continue;
        auto aid = callback->getAssociationId(header->getTransmitterAddress());
        auto record = std::find_if(ackRecords.begin(), ackRecords.end(),
                [aid] (const auto& value) { return value.aid == aid; });
        if (record == ackRecords.end()) {
            Ieee80211MultiStaBlockAckRecord value;
            value.aid = aid;
            value.tid = header->getTid();
            ackRecords.push_back(value);
            record = std::prev(ackRecords.end());
        }
        record->responseReceived = true;
        if (header->getType() != ST_QOS_NULL) {
            record->startingSequenceNumber = header->getSequenceNumber().get();
            record->bitmap = 1;
        }
        callback->processTriggeredUlFrame(packet->dup(), header, aid);
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
            return new ReceiveCollectionStep(modeSet->getSifsTime() + schedule.commonDuration +
                    modeSet->getSlotTime());
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
