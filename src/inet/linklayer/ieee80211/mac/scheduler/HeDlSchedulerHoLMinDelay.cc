//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerHoLMinDelay.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

Define_Module(HeDlSchedulerHoLMinDelay);

std::vector<IIeee80211HeDlScheduler::RuAllocation>
HeDlSchedulerHoLMinDelay::schedule(const ScheduleContext& context)
{
    ASSERT(!std::isnan(context.channelCenterFrequency.get()) && context.channelCenterFrequency > Hz(0));
    ASSERT(!std::isnan(context.channelBandwidth.get()) && context.channelBandwidth > Hz(0));
    std::vector<CandidateInfo> selected = context.candidates;
    std::sort(selected.begin(), selected.end(), [] (const CandidateInfo& a, const CandidateInfo& b) {
        if (a.anchor != b.anchor)
            return a.anchor;
        if (a.holDelay != b.holDelay)
            return a.holDelay > b.holDelay;
        if (a.holPacketBytes != b.holPacketBytes)
            return a.holPacketBytes > b.holPacketBytes;
        if (a.backlogBytes != b.backlogBytes)
            return a.backlogBytes > b.backlogBytes;
        return a.staAddress < b.staAddress;
    });
    int limit = maxMuStations < 0 ? physicallayer::getHeMaxRuCount(context.channelBandwidth) :
            std::min(maxMuStations, physicallayer::getHeMaxRuCount(context.channelBandwidth));
    if ((int)selected.size() > limit) {
        EV_DEBUG << "HeDlSchedulerHoLMinDelay::schedule: truncating candidate list from "
                 << selected.size() << " to " << limit << "\n";
        selected.resize(limit);
    }
    EV_INFO << "HeDlSchedulerHoLMinDelay::schedule: scheduling " << selected.size()
            << " STAs by head-of-line delay\n";
    std::vector<int> requests;
    std::vector<int64_t> payloadBytes;
    for (const auto& candidate : selected)
        requests.push_back(requestRuForBytes(candidate.holPacketBytes, context.channelBandwidth));
    for (const auto& candidate : selected)
        payloadBytes.push_back(candidate.holPacketBytes);
    return fitRequestedRus(context, selected, requests, payloadBytes);
}

} // namespace ieee80211
} // namespace inet
