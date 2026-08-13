//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFMACSAPTRACKER_H
#define __INET_HCFMACSAPTRACKER_H

#include <cstdint>
#include <limits>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

class Packet;

namespace ieee80211 {

/**
 * Assigns MAC-SAP service-data-unit identities and accounts their service
 * bytes without owning any Packet or queue.
 *
 * Region tags added by this service are owned by their Packet. Packet lists
 * passed to the accounting operation are call-scoped, non-owning views.
 */
class INET_API HcfMacSapTracker
{
  public:
    using PacketVector = std::vector<const Packet *>;

    static constexpr uint32_t MAX_FINITE_BUFFER_STATUS_BYTES =
            std::numeric_limits<uint32_t>::max() - 1;

  private:
    uint64_t nextServiceDataUnitId = 1;

  protected:
    explicit HcfMacSapTracker(uint64_t nextServiceDataUnitId);

  public:
    HcfMacSapTracker() = default;
    HcfMacSapTracker(const HcfMacSapTracker&) = delete;
    HcfMacSapTracker& operator=(const HcfMacSapTracker&) = delete;
    HcfMacSapTracker(HcfMacSapTracker&&) = delete;
    HcfMacSapTracker& operator=(HcfMacSapTracker&&) = delete;

    static void addBufferedTrafficServiceBytes(uint32_t& total, uint64_t amount);
    uint64_t allocateServiceDataUnitId();
    void tagMacSapServiceDataUnit(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header);

    uint32_t calculateBufferedTrafficServiceBytes(const MacAddress& peer, int tid,
            const PacketVector& pendingPackets,
            const PacketVector& inProgressPackets = {},
            const PacketVector& additionalPackets = {}) const;
};

} // namespace ieee80211
} // namespace inet

#endif
