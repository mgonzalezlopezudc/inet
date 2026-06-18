//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.h"

#include <algorithm>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"

namespace inet {
namespace ieee80211 {

Define_Module(HeUlSchedulerBacklogBased);

IIeee80211HeUlScheduler::Schedule HeUlSchedulerBacklogBased::schedule(const ScheduleContext& context)
{
    Schedule result;
    auto candidates = context.candidates;
    std::stable_sort(candidates.begin(), candidates.end(), [] (const auto& left, const auto& right) {
        if (left.anchor != right.anchor)
            return left.anchor;
        if (left.lastService != right.lastService)
            return left.lastService < right.lastService;
        return left.getSelectedBacklogBytes() > right.getSelectedBacklogBytes();
    });

    int maxRus = physicallayer::getHeMaxRuCount(context.channelBandwidth);
    int raCount = computeRandomAccessRuCount(context, maxRus);
    int scheduledCount = std::min({(int)candidates.size(), maxMuStations,
            std::max(0, maxRus - raCount)});
    std::vector<std::pair<int, int>> requested;
    for (int i = 0; i < scheduledCount; i++) {
        int64_t bytes = candidates[i].getSelectedBacklogBytes();
        int toneSize = bytes > 12000 ? 242 : bytes > 6000 ? 106 : bytes > 2000 ? 52 : 26;
        requested.push_back({toneSize, i});
    }
    for (int i = 0; i < raCount; i++)
        requested.push_back({26, scheduledCount + i});
    std::vector<physicallayer::Ieee80211HeRu> rus;
    while (!requested.empty()) {
        std::stable_sort(requested.begin(), requested.end(),
                [] (const auto& left, const auto& right) { return left.first > right.first; });
        std::vector<int> requestedSizes;
        for (const auto& request : requested)
            requestedSizes.push_back(request.first);
        if (physicallayer::allocateHeRus(context.channelCenterFrequency,
                context.channelBandwidth, requestedSizes, rus))
            break;

        // Preserve the requested 26-tone random-access capacity. First shrink
        // the largest scheduled RU; if all scheduled RUs are already minimal,
        // drop a scheduled user instead of silently discarding UORA RUs.
        auto scheduledRequest = std::find_if(requested.begin(), requested.end(),
                [scheduledCount] (const auto& request) {
                    return request.second < scheduledCount && request.first > 26;
                });
        if (scheduledRequest != requested.end())
            scheduledRequest->first = scheduledRequest->first > 106 ? 106 :
                    scheduledRequest->first > 52 ? 52 : 26;
        else {
            auto lastScheduledRequest = std::find_if(requested.rbegin(), requested.rend(),
                    [scheduledCount] (const auto& request) {
                        return request.second < scheduledCount;
                    });
            if (lastScheduledRequest == requested.rend())
                break;
            requested.erase(std::next(lastScheduledRequest).base());
        }
    }

    int targetRssiDbm = computeTargetRssiDbm(context);
    for (size_t i = 0; i < rus.size(); i++) {
        int originalIndex = requested[i].second;
        RuAllocation allocation;
        allocation.ru = rus[i];
        allocation.mcs = defaultMcs;
        allocation.targetRssiDbm = targetRssiDbm;
        if (originalIndex < scheduledCount) {
            const auto& candidate = candidates[originalIndex];
            allocation.staAddress = candidate.staAddress;
            allocation.associationId = candidate.associationId;
            allocation.tid = candidate.selectedTid;
            allocation.accessCategory = candidate.selectedAccessCategory;
            allocation.estimatedDuration = physicallayer::estimateHeMuUserDuration(
                    B(std::max<int64_t>(1, candidate.getSelectedBacklogBytes())),
                    allocation.ru.toneSize, allocation.mcs);
        }
        else
            allocation.randomAccess = true;
        result.allocations.push_back(allocation);
    }
    result.commonDuration = computeCommonDuration(context, result.allocations);
    return result;
}

} // namespace ieee80211
} // namespace inet
