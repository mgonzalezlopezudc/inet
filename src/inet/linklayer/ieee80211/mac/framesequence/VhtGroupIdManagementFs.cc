//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/VhtGroupIdManagementFs.h"

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtGroupIdManager.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"

namespace inet {
namespace ieee80211 {

VhtGroupIdManagementFs::VhtGroupIdManagementFs(Ieee80211Mib *mib,
        IVhtGroupIdManager *groupIdManager, const MacAddress& peer,
        uint8_t groupId, uint8_t userPosition,
        uint64_t associationGeneration, Hz channelWidth) :
    mib(mib), groupIdManager(groupIdManager), peer(peer), groupId(groupId),
    userPosition(userPosition), associationGeneration(associationGeneration),
    channelWidth(channelWidth)
{
    ASSERT(mib != nullptr && groupIdManager != nullptr);
    ASSERT(!peer.isMulticast() && !peer.isUnspecified());
    ASSERT(groupId > 0 && groupId < 63 && userPosition < 4);
    ASSERT(associationGeneration > 0 && channelWidth == MHz(20));
}

void VhtGroupIdManagementFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    step = 0;
    groupIdManager->beginPending(peer, groupId, userPosition,
            associationGeneration, channelWidth);
}

IFrameSequenceStep *VhtGroupIdManagementFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            return new TransmitStep(buildActionFrame(), SIMTIME_ZERO, true);
        case 1: {
            auto transmit = check_and_cast<TransmitStep *>(context->getLastStep());
            auto packet = transmit->getFrameToTransmit();
            auto header = packet->peekAtFront<Ieee80211MgmtHeader>();
            return new ReceiveStep(context->getAckTimeout(packet, header),
                    IReceiveStep::TimeoutHandling::COMPLETE_STEP,
                    [this](Packet *packet, FrameSequenceContext *context) {
                        return isExpectedAck(packet, context);
                    },
                    IReceiveStep::UnexpectedResponseHandling::IGNORE_RESPONSE,
                    IFrameSequenceStep::Completion::EXPIRED);
        }
        case 2:
            return nullptr;
        default:
            throw cRuntimeError("Invalid VHT Group ID Management step");
    }
}

bool VhtGroupIdManagementFs::completeStep(FrameSequenceContext *context)
{
    if (step == 1) {
        auto receive = check_and_cast<IReceiveStep *>(context->getLastStep());
        if (receive->getReceivedFrame() != nullptr &&
                isExpectedAck(receive->getReceivedFrame(), context))
            groupIdManager->acknowledge(peer, groupId,
                    associationGeneration, channelWidth);
        else
            groupIdManager->invalidatePeer(peer);
    }
    step++;
    return true;
}

Packet *VhtGroupIdManagementFs::buildActionFrame() const
{
    auto header = makeShared<Ieee80211ActionFrame>();
    header->setType(ST_ACTION);
    header->setCategory(21);
    header->setReceiverAddress(peer);
    header->setTransmitterAddress(mib->address);
    header->setAddress3(mib->bssData.bssid);
    header->setSequenceNumber(SequenceNumberCyclic(0));
    header->setChunkLength(B(24));

    auto body = makeShared<Ieee80211VhtGroupIdManagement>();
    VhtGroupIdManager::setMembership(*body, groupId, userPosition);

    auto packet = new Packet("VHT-Group-ID-Management");
    packet->insertAtBack(header);
    packet->insertAtBack(body);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

bool VhtGroupIdManagementFs::isExpectedAck(Packet *packet,
        FrameSequenceContext *context) const
{
    if (packet == nullptr)
        return false;
    auto header = packet->peekAtFront<Ieee80211MacHeader>();
    return context->isForUs(header) && header->getType() == ST_ACK;
}

} // namespace ieee80211
} // namespace inet
