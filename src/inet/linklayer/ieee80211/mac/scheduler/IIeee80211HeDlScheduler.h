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
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/queueing/contract/IPacketQueue.h"

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
    struct CandidateInfo {
        MacAddress staAddress;
        AccessCategory accessCategory = AC_BE;
        bool anchor = false;
        int64_t backlogBytes = 0;
        int64_t holPacketBytes = 0;
        simtime_t holEnqueueTime = SIMTIME_ZERO;
        simtime_t holDelay = SIMTIME_ZERO;
        double pathLossDb = NaN;
        bool hasFreshPathLoss = false;
        queueing::IPacketQueue *sourceQueue = nullptr;
    };

    struct ScheduleContext {
        std::vector<CandidateInfo> candidates;
        MacAddress anchorSta;
        Hz channelCenterFrequency = Hz(NaN);
        Hz channelBandwidth = Hz(NaN);
        simtime_t txopLimit = SIMTIME_ZERO;
        W totalTransmitPower = W(NaN);
        double noiseFigureDb = 0;
    };

    struct RuAllocation {
        MacAddress staAddress;
        physicallayer::Ieee80211HeRu ru;
        int mcs = 0;
        double estimatedSnrDb = NaN;
        simtime_t estimatedDuration = SIMTIME_ZERO;
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
    virtual std::vector<RuAllocation> schedule(const std::vector<MacAddress>& candidates,
            Hz channelCenterFrequency, Hz channelBandwidth) = 0;

    virtual std::vector<RuAllocation> schedule(const ScheduleContext& context)
    {
        std::vector<MacAddress> candidates;
        for (const auto& candidate : context.candidates)
            candidates.push_back(candidate.staAddress);
        return schedule(candidates, context.channelCenterFrequency, context.channelBandwidth);
    }
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_IIEE80211HEDLSCHEDULER_H
