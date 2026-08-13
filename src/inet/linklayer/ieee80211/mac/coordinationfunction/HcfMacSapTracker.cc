//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfMacSapTracker.h"

#include <algorithm>
#include <set>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211MacSapServiceTag_m.h"

namespace inet {
namespace ieee80211 {

HcfMacSapTracker::HcfMacSapTracker(uint64_t nextServiceDataUnitId) :
    nextServiceDataUnitId(nextServiceDataUnitId)
{
}

void HcfMacSapTracker::addBufferedTrafficServiceBytes(uint32_t& total, uint64_t amount)
{
    total += static_cast<uint32_t>(std::min<uint64_t>(
            amount, MAX_FINITE_BUFFER_STATUS_BYTES - total));
}

uint64_t HcfMacSapTracker::allocateServiceDataUnitId()
{
    // Zero is reserved for invalid/unassigned metadata. Exhausting the
    // nonzero ID space fails deterministically instead of reusing an ID that
    // may still identify queued or in-progress traffic.
    if (nextServiceDataUnitId == 0)
        throw cRuntimeError("MAC-SAP service data unit ID space exhausted");
    return nextServiceDataUnitId++;
}

void HcfMacSapTracker::tagMacSapServiceDataUnit(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header)
{
    auto serviceDataUnitBytes = packet->getDataLength() -
            header->getChunkLength() - B(4);
    if (serviceDataUnitBytes <= B(0))
        return;
    auto tag = packet->addRegionTag<Ieee80211MacSapServiceTag>(
            packet->getFrontOffset() + header->getChunkLength(),
            serviceDataUnitBytes);
    tag->setServiceDataUnitId(allocateServiceDataUnitId());
    tag->setServiceDataUnitBytes(serviceDataUnitBytes);
}

uint32_t HcfMacSapTracker::calculateBufferedTrafficServiceBytes(
        const MacAddress& peer, int tid, const PacketVector& pendingPackets,
        const PacketVector& inProgressPackets,
        const PacketVector& additionalPackets) const
{
    std::set<uint64_t> accountedServiceDataUnits;
    std::set<const Packet *> accountedLegacyPackets;
    uint32_t serviceBytes = 0;
    auto accountPacket = [&] (const Packet *packet) {
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront());
        if (header == nullptr || header->getType() != ST_DATA_WITH_QOS ||
                header->getReceiverAddress() != peer ||
                (tid >= 0 && header->getTid() != tid))
            return;

        // IEEE Std 802.11-2024 clauses 9.2.4.5.6, 9.2.4.7.4, and 26.5.5:
        // buffer status counts MAC-SAP MSDU/A-MSDU service octets, including
        // queued in-progress traffic, but excludes MPDU/A-MPDU and PHY overhead.
        bool hasServiceDataUnitTag = false;
        packet->mapAllRegionTags<Ieee80211MacSapServiceTag>(
                packet->getFrontOffset(), packet->getDataLength(),
                [&] (b, b, const Ptr<const Ieee80211MacSapServiceTag>& tag) {
                    hasServiceDataUnitTag = true;
                    auto id = tag->getServiceDataUnitId();
                    if (id == 0)
                        throw cRuntimeError("MAC-SAP service data unit ID must be nonzero");
                    if (accountedServiceDataUnits.insert(id).second) {
                        auto bytes = tag->getServiceDataUnitBytes().get<B>();
                        if (bytes < 0)
                            throw cRuntimeError("MAC-SAP service data unit byte count must not be negative");
                        addBufferedTrafficServiceBytes(serviceBytes, bytes);
                    }
                });
        if (hasServiceDataUnitTag)
            return;

        // Compatibility for complete legacy/unit-test packets that predate
        // MAC-SAP provenance. A fragment body is never interpreted as a
        // complete MSDU or A-MSDU.
        if (!accountedLegacyPackets.insert(packet).second)
            return;
        if (header->getFragmentNumber() != 0 || header->getMoreFragments())
            return;
        if (header->getAMsduPresent()) {
            b offset = header->getChunkLength();
            b bodyEnd = packet->getDataLength() - B(4);
            while (offset < bodyEnd) {
                auto subframeHeader = packet->peekAt<Ieee80211MsduSubframeHeader>(offset);
                auto subframeLength = B(subframeHeader->getLength());
                auto subframeEnd = offset + subframeHeader->getChunkLength() + subframeLength;
                if (subframeLength < B(0) || subframeEnd > bodyEnd)
                    throw cRuntimeError("Invalid complete A-MSDU subframe length");
                addBufferedTrafficServiceBytes(serviceBytes, subframeLength.get<B>());
                offset = subframeEnd;
                if (offset < bodyEnd)
                    offset += B((4 - (subframeHeader->getChunkLength() +
                            subframeLength).get<B>() % 4) % 4);
            }
            if (offset != bodyEnd)
                throw cRuntimeError("Invalid complete A-MSDU padding length");
        }
        else {
            auto payloadBytes = (packet->getDataLength() -
                    header->getChunkLength() - B(4)).get<B>();
            if (payloadBytes > 0)
                addBufferedTrafficServiceBytes(serviceBytes, payloadBytes);
        }
    };

    for (auto packet : pendingPackets)
        accountPacket(packet);
    for (auto packet : inProgressPackets)
        accountPacket(packet);
    for (auto packet : additionalPackets)
        accountPacket(packet);
    return serviceBytes;
}

} // namespace ieee80211
} // namespace inet
