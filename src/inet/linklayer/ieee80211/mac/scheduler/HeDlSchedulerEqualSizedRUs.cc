//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.h"

#include <algorithm>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(HeDlSchedulerEqualSizedRUs);

void HeDlSchedulerEqualSizedRUs::initialize(int stage)
{
    HeDlSchedulerBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        maxMuStations = par("maxMuStations");
        schedulingFunction = par("schedulingFunction").stdstringValue();
        if (schedulingFunction != "fBW" && schedulingFunction != "fHoL")
            throw cRuntimeError("Unknown equal-sized HE RU scheduling function: %s", schedulingFunction.c_str());
    }
}

std::vector<IIeee80211HeDlScheduler::RuAllocation>
HeDlSchedulerEqualSizedRUs::schedule(const ScheduleContext& context)
{
    if (context.candidates.empty())
        return {};
    auto selectedCandidates = context.candidates;
    std::sort(selectedCandidates.begin(), selectedCandidates.end(), defaultCandidateLess);
    auto validCounts = getHeEqualRuCounts(context.channelBandwidth);
    int candidateLimit = maxMuStations < 0 ? getHeMaxRuCount(context.channelBandwidth) : maxMuStations;
    int candidates = std::min((int)selectedCandidates.size(), candidateLimit);
    if (candidates <= 0)
        return {};
    int ruCount = validCounts.front();
    if (schedulingFunction == "fBW") {
        for (int count : validCounts)
            if (count <= candidates)
                ruCount = count;
    }
    else {
        ruCount = validCounts.back();
        for (int count : validCounts) {
            if (count >= candidates) {
                ruCount = count;
                break;
            }
        }
    }
    int numSelected = std::min(candidates, ruCount);
    auto rus = getHeEqualRuLayout(context.channelCenterFrequency, context.channelBandwidth, ruCount);

    std::vector<RuAllocation> result;
    result.reserve(numSelected);
    for (int i = 0; i < numSelected; ++i) {
        RuAllocation alloc;
        alloc.staAddress = selectedCandidates[i].staAddress;
        alloc.ru = rus[i];
        alloc.estimatedSnrDb = estimateSnrDb(context, selectedCandidates[i], alloc.ru);
        alloc.mcs = selectMcs(alloc.estimatedSnrDb, selectedCandidates[i].hasFreshPathLoss);
        alloc.estimatedDuration = estimateDuration(
                std::max<int64_t>(selectedCandidates[i].holPacketBytes, 1), alloc.ru.toneSize, alloc.mcs);
        result.push_back(alloc);
    }
    return result;
}

} // namespace ieee80211
} // namespace inet
