//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerEqualSizedRUs.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"

namespace inet {
namespace ieee80211 {

Define_Module(HeUlSchedulerEqualSizedRUs);

IIeee80211HeUlScheduler::Schedule HeUlSchedulerEqualSizedRUs::schedule(const ScheduleContext& context)
{
    Schedule result;
    int availableRus = physicallayer::getHeMaxRuCount(context.channelBandwidth);
    ASSERT(availableRus >= 0);
    int raCount = computeRandomAccessRuCount(context, availableRus);
    int scheduledCount = std::min({(int)context.candidates.size(), maxMuStations,
            std::max(0, availableRus - raCount)});
    if (scheduledCount == 0)
        raCount = std::min(raCount, availableRus);
    auto layout = physicallayer::getHeEqualRuLayout(context.channelCenterFrequency,
            context.channelBandwidth, availableRus);
    ASSERT((int)layout.size() == availableRus);
    int targetRssiDbm = computeTargetRssiDbm(context);
    for (int i = 0; i < scheduledCount; i++) {
        const auto& candidate = context.candidates[i];
        RuAllocation allocation;
        allocation.staAddress = candidate.staAddress;
        allocation.associationId = candidate.associationId;
        allocation.tid = candidate.selectedTid;
        allocation.accessCategory = candidate.selectedAccessCategory;
        allocation.ru = layout[i];
        allocation.mcs = defaultMcs;
        allocation.targetRssiDbm = targetRssiDbm;
        allocation.estimatedDuration = physicallayer::estimateHeMuUserDuration(
                B(std::max<int64_t>(1, candidate.getSelectedBacklogBytes())),
                allocation.ru.toneSize, allocation.mcs);
        result.allocations.push_back(allocation);
    }
    for (int i = 0; i < raCount; i++) {
        RuAllocation allocation;
        allocation.randomAccess = true;
        allocation.associationId = 0;
        allocation.ru = layout[scheduledCount + i];
        allocation.mcs = defaultMcs;
        allocation.targetRssiDbm = targetRssiDbm;
        result.allocations.push_back(allocation);
    }
    result.commonDuration = computeCommonDuration(context, result.allocations);
    EV_INFO << "HE UL equal-RU schedule: scheduled=" << scheduledCount
             << ", randomAccess=" << raCount
             << ", total=" << result.allocations.size() << "\n";
    return result;
}

} // namespace ieee80211
} // namespace inet
