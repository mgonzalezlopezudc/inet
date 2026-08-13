//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerEqualSizedRUs.h"

#include <algorithm>

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

    // The coordinator projects operating-mode state into the immutable context.
    std::vector<CandidateInfo> candidates;
    for (const auto& candidate : context.candidates) {
        const bool freshKnownZero = candidate.hasTypedBacklogEstimates &&
                candidate.hasFreshReport && !candidate.isUnknownProbe() &&
                candidate.getSelectedBacklogBytes() == 0;
        if (!candidate.ulMuDisabled && !freshKnownZero)
            candidates.push_back(candidate);
    }

    int scheduledCount = std::min({(int)candidates.size(), maxMuStations,
            std::max(0, availableRus - raCount)});
    if (scheduledCount == 0)
        raCount = std::min(raCount, availableRus);
    auto layout = physicallayer::getHeEqualRuLayout(context.channelCenterFrequency,
            context.channelBandwidth, availableRus);
    ASSERT((int)layout.size() == availableRus);
    int targetRssiDbm = computeTargetRssiDbm(context);
    for (int i = 0; i < scheduledCount; i++) {
        const auto& candidate = candidates[i];
        RuAllocation allocation;
        allocation.staAddress = candidate.staAddress;
        allocation.associationId = candidate.associationId;
        allocation.tid = candidate.selectedTid;
        allocation.accessCategory = candidate.selectedAccessCategory;
        allocation.ru = layout[i];
        int maximumNss = 1;
        if (candidate.hasNegotiatedHeCapabilities &&
                candidate.negotiatedHeCapabilities.localRxPeerTx.valid)
            maximumNss = getMaxNss(candidate.negotiatedHeCapabilities.localRxPeerTx.mcsNss);
        maximumNss = std::clamp(maximumNss, 1,
                candidate.coding == physicallayer::HE_CODING_BCC ? 4 : 8);
        allocation.numberOfSpatialStreams = maximumNss;
        allocation.coding = candidate.coding;
        allocation.mcs = selectMcs(context, candidate, allocation.ru, maximumNss);
        allocation.targetRssiDbm = targetRssiDbm;
        allocation.estimatedDuration = physicallayer::estimateHeMuUserDuration(
                B(std::max<int64_t>(1, candidate.getSelectedBacklogBytes())),
                allocation.ru.toneSize, allocation.mcs, allocation.numberOfSpatialStreams,
                false);
        result.allocations.push_back(allocation);
    }
    for (int i = 0; i < raCount; i++) {
        RuAllocation allocation;
        allocation.randomAccess = true;
        allocation.randomAccessTarget = getRandomAccessTarget();
        allocation.associationId = 0;
        allocation.ru = layout[scheduledCount + i];
        allocation.mcs = defaultMcs;
        allocation.targetRssiDbm = targetRssiDbm;
        allocation.estimatedDuration = context.requestedDuration;
        result.allocations.push_back(allocation);
    }
    result.commonDuration = computeCommonDuration(context, result.allocations);
    result.decisionReason = "equal-sized trigger";
    for (auto& allocation : result.allocations)
        allocation.estimatedDuration = result.commonDuration;
    EV_INFO << "HE UL equal-RU schedule: scheduled=" << scheduledCount
             << ", randomAccess=" << raCount
             << ", total=" << result.allocations.size() << "\n";
    return result;
}

} // namespace ieee80211
} // namespace inet
