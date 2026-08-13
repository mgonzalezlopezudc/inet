// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_HEFRAMEDECORATIONPOLICY_H
#define __INET_HEFRAMEDECORATIONPOLICY_H

#include <functional>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

/** Stateless, exactly-once outgoing HE QoS-frame decoration policy. */
class INET_API HeFrameDecorationPolicy
{
  public:
    struct Request {
        bool associated = false;
        bool sendOperatingModeIndication = false;
        bool operatingModeControlSupported = false;
        uint8_t operatingModeChannelWidth = 0;
        uint8_t operatingModeRxNss = 1;
        bool operatingModeUlMuDisable = false;
    };

    using BufferStatusProvider = std::function<uint32_t(const MacAddress&, Tid, AccessCategory)>;
    using AccessCategoryProvider = std::function<AccessCategory(Tid)>;

    bool decorate(Packet *packet, const Request& request,
            const AccessCategoryProvider& mapTidToAccessCategory,
            const BufferStatusProvider& getBufferStatus) const;
};

} // namespace ieee80211
} // namespace inet

#endif
