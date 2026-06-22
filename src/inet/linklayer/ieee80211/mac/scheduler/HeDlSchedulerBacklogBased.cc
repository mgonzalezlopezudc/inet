//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerBacklogBased.h"

#include <algorithm>
#include <cmath>

namespace inet {
namespace ieee80211 {

Define_Module(HeDlSchedulerBacklogBased);

void HeDlSchedulerBacklogBased::initialize(int stage)
{
    HeDlSchedulerBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        deltaPlMax = par("deltaPlMax");
}

std::vector<IIeee80211HeDlScheduler::RuAllocation>
HeDlSchedulerBacklogBased::schedule(const ScheduleContext& context)
{
    ASSERT(!std::isnan(context.channelCenterFrequency.get()) && context.channelCenterFrequency > Hz(0));
    ASSERT(!std::isnan(context.channelBandwidth.get()) && context.channelBandwidth > Hz(0));
    auto anchorIt = std::find_if(context.candidates.begin(), context.candidates.end(),
            [] (const CandidateInfo& candidate) { return candidate.anchor; });
    if (anchorIt == context.candidates.end() && !context.candidates.empty())
        anchorIt = context.candidates.begin();
    if (anchorIt == context.candidates.end()) {
        EV_DEBUG << "HeDlSchedulerBacklogBased::schedule: no candidates available\n";
        return {};
    }
    CandidateInfo anchor = *anchorIt;
    anchor.anchor = true;
    std::vector<CandidateInfo> selected = {anchor};
    int rejectedByPathLoss = 0;
    for (const auto& candidate : context.candidates) {
        if (candidate.staAddress == anchor.staAddress)
            continue;
        if (!anchor.hasFreshPathLoss || !candidate.hasFreshPathLoss ||
                std::abs(candidate.pathLossDb - anchor.pathLossDb) <= deltaPlMax) {
            selected.push_back(candidate);
        }
        else {
            rejectedByPathLoss++;
        }
    }
#ifndef NDEBUG
    EV_DEBUG << "HeDlSchedulerBacklogBased::schedule: anchor = " << anchor.staAddress
             << ", pathLoss = " << (anchor.hasFreshPathLoss ? std::to_string(anchor.pathLossDb) : "N/A")
             << ", deltaPlMax = " << deltaPlMax
             << ", kept = " << selected.size() << ", rejected by path-loss spread = "
             << rejectedByPathLoss << "\n";
#endif
    std::sort(selected.begin() + 1, selected.end(), [&] (const CandidateInfo& a, const CandidateInfo& b) {
        if (anchor.hasFreshPathLoss && a.hasFreshPathLoss && b.hasFreshPathLoss) {
            double aDelta = std::abs(a.pathLossDb - anchor.pathLossDb);
            double bDelta = std::abs(b.pathLossDb - anchor.pathLossDb);
            if (aDelta != bDelta)
                return aDelta < bDelta;
        }
        if (a.backlogBytes != b.backlogBytes)
            return a.backlogBytes > b.backlogBytes;
        if (a.holDelay != b.holDelay)
            return a.holDelay > b.holDelay;
        return a.staAddress < b.staAddress;
    });
    int limit = maxMuStations < 0 ? physicallayer::getHeMaxRuCount(context.channelBandwidth) :
            std::min(maxMuStations, physicallayer::getHeMaxRuCount(context.channelBandwidth));
    if ((int)selected.size() > limit) {
#ifndef NDEBUG
        EV_DEBUG << "HeDlSchedulerBacklogBased::schedule: truncating candidate list from "
                 << selected.size() << " to " << limit << "\n";
#endif
        selected.resize(limit);
    }
    std::vector<int> requests;
    std::vector<int64_t> payloadBytes;
    for (const auto& candidate : selected)
        requests.push_back(requestRuForBytes(candidate.backlogBytes, context.channelBandwidth));
    for (const auto& candidate : selected)
        payloadBytes.push_back(candidate.backlogBytes);
    return fitRequestedRus(context, selected, requests, payloadBytes);
}

} // namespace ieee80211
} // namespace inet
