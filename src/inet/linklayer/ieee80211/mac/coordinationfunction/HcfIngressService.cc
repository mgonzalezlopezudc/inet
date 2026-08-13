//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfIngressService.h"

#include "inet/common/packet/Packet.h"

namespace inet {
namespace ieee80211 {

void HcfIngressService::checkActions(const Actions& actions)
{
    if (actions.ownership == nullptr || !actions.packetClaimed ||
            !actions.classifyDataFrame || !actions.frameClassified ||
            !actions.tagMacSapServiceDataUnit || !actions.resolvePerStaQueue ||
            !actions.getSharedQueue || !actions.ensureOriginalEnqueueTime ||
            !actions.hasFrameToTransmit || !actions.hasChannelOwner || !actions.isSequenceRunning ||
            !actions.channelAccessRequested || !actions.requestChannelAccess)
        throw cRuntimeError("Incomplete HCF ingress actions");
}

void HcfIngressService::checkAccessCategory(AccessCategory accessCategory)
{
    if (accessCategory < AC_BK || accessCategory >= AC_NUMCATEGORIES)
        throw cRuntimeError("Invalid HCF ingress access category %d", accessCategory);
}

HcfIngressService::Result HcfIngressService::processUpperFrame(Packet *packet,
        const Ptr<const Ieee80211DataOrMgmtHeader>& header,
        const Actions& actions)
{
    if (packet == nullptr || header == nullptr)
        throw cRuntimeError("HCF ingress requires a packet and an IEEE 802.11 data or management header");
    if (activePacket != nullptr)
        throw cRuntimeError(activePacket == packet ?
                "HCF ingress is already processing this packet" :
                "HCF ingress cannot process a foreign packet while another packet is active");
    checkActions(actions);
    auto packetHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
    if (packetHeader.get() != header.get())
        throw cRuntimeError("HCF ingress header does not belong to the submitted packet");

    activePacket = packet;
    bool claimed = false;
    bool enqueueCommitted = false;
    try {
        actions.ownership->claimPacket(packet);
        claimed = true;
        actions.packetClaimed();

        Result result;
        Ptr<const Ieee80211DataHeader> dataHeader;
        if (dynamicPtrCast<const Ieee80211MgmtHeader>(header))
            result.accessCategory = AC_VO;
        else if ((dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) != nullptr)
            result.accessCategory = actions.classifyDataFrame(dataHeader);
        else
            throw cRuntimeError("Unsupported HCF upper frame type");
        checkAccessCategory(result.accessCategory);
        actions.frameClassified(result.accessCategory);

        if (dataHeader != nullptr)
            actions.tagMacSapServiceDataUnit(packet, dataHeader);

        if (dataHeader != nullptr &&
                !header->getReceiverAddress().isMulticast() &&
                !header->getReceiverAddress().isBroadcast()) {
            result.queue = actions.resolvePerStaQueue(
                    header->getReceiverAddress(), result.accessCategory);
            actions.ensureOriginalEnqueueTime(packet);
            if (result.queue != nullptr)
                result.queueSelection = QueueSelection::PER_STA;
        }
        if (result.queue == nullptr) {
            result.queue = actions.getSharedQueue(result.accessCategory);
            result.queueSelection = QueueSelection::SHARED;
        }
        if (result.queue == nullptr)
            throw cRuntimeError("HCF ingress queue resolution returned no queue");

        // A successful enqueue is the ownership commit point. No eligibility
        // or channel-access action is evaluated if this call throws.
        actions.ownership->enqueuePacket(result.queue, packet);
        enqueueCommitted = true;
        if (actions.hasFrameToTransmit(result.accessCategory) &&
                !actions.hasChannelOwner() && !actions.isSequenceRunning()) {
            actions.channelAccessRequested();
            actions.requestChannelAccess(result.accessCategory);
            result.channelAccessRequested = true;
        }
        activePacket = nullptr;
        return result;
    }
    catch (...) {
        if (claimed && !enqueueCommitted)
            actions.ownership->returnClaimedPacketToCaller(packet);
        activePacket = nullptr;
        throw;
    }
}

} // namespace ieee80211
} // namespace inet
