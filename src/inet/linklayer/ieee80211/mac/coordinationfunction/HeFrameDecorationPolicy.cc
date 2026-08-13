// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeFrameDecorationPolicy.h"

namespace inet {
namespace ieee80211 {

bool HeFrameDecorationPolicy::decorate(Packet *packet, const Request& request,
        const AccessCategoryProvider& mapTidToAccessCategory,
        const BufferStatusProvider& getBufferStatus) const
{
    if (!request.associated)
        return false;
    auto header = dynamicPtrCast<const Ieee80211DataHeader>(
            packet->peekAtFront<Ieee80211MacHeader>());
    if (header == nullptr || header->getType() != ST_DATA_WITH_QOS)
        return false;
    // IEEE Std 802.11-2024, 9.2.4.5.11 and 9.2.4.6a: an OMI takes
    // precedence over the one-shot BSR carried by this modeled control field.
    auto writableHeader = packet->removeAtFront<Ieee80211DataHeader>();
    const bool alreadyDecorated = writableHeader->getBufferStatusPresent() ||
            writableHeader->getOperatingModePresent();
    const bool useOperatingMode = request.sendOperatingModeIndication &&
            request.operatingModeControlSupported;
    if (alreadyDecorated && (!useOperatingMode || writableHeader->getOperatingModePresent())) {
        packet->insertAtFront(writableHeader);
        return false;
    }
    if (!alreadyDecorated)
        writableHeader->setChunkLength(writableHeader->getChunkLength() + B(4));
    writableHeader->setOrder(true);
    auto tid = writableHeader->getTid();
    if (!mapTidToAccessCategory)
        throw cRuntimeError("HE frame decoration requires the authoritative TID-to-AC mapping");
    auto accessCategory = mapTidToAccessCategory(tid);
    if (useOperatingMode) {
        writableHeader->setBufferStatusPresent(false);
        writableHeader->setOperatingModePresent(true);
        writableHeader->setOperatingModeChannelWidth(request.operatingModeChannelWidth);
        writableHeader->setOperatingModeRxNss(request.operatingModeRxNss);
        writableHeader->setOperatingModeUlMuDisable(request.operatingModeUlMuDisable);
    }
    else {
        writableHeader->setBufferStatusPresent(true);
        writableHeader->setBufferStatusTid(tid);
        writableHeader->setBufferStatusAc(accessCategory);
        writableHeader->setBufferStatusQueueSize(getBufferStatus(
                writableHeader->getReceiverAddress(), tid, accessCategory));
    }
    packet->insertAtFront(writableHeader);
    return true;
}

} // namespace ieee80211
} // namespace inet
