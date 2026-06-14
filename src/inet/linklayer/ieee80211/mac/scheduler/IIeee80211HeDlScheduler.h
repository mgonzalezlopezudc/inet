//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEE80211HEDLSCHEDULER_H
#define __INET_IIEE80211HEDLSCHEDULER_H

#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * Abstract interface for IEEE 802.11ax Downlink OFDMA resource unit schedulers.
 * Given a list of candidate STAs and the channel parameters, returns a per-STA
 * RU assignment.
 */
class INET_API IIeee80211HeDlScheduler
{
  public:
    struct RuAllocation {
        MacAddress staAddress;
        physicallayer::Ieee80211HeRu ru;
    };

    virtual ~IIeee80211HeDlScheduler() {}

    /**
     * Schedule RU allocations for the given candidate STAs.
     *
     * @param candidates       Ordered list of destination MAC addresses.
     * @param channelCenterFrequency  Center frequency of the primary channel.
     * @param channelBandwidth        Bandwidth of the primary channel.
     * @return  One RuAllocation per selected STA. May return fewer entries
     *          than candidates if maxMuStations is exceeded.
     */
    virtual std::vector<RuAllocation> schedule(
            const std::vector<MacAddress>& candidates,
            Hz channelCenterFrequency,
            Hz channelBandwidth) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_IIEE80211HEDLSCHEDULER_H
