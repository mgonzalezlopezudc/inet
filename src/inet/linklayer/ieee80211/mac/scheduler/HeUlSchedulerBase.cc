//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBase.h"

#include <algorithm>
#include <cmath>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"

namespace inet {
namespace ieee80211 {

void HeUlSchedulerBase::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        maxMuStations = par("maxMuStations");
        minRandomAccessRus = par("minRandomAccessRus");
        maxRandomAccessRus = par("maxRandomAccessRus");
        defaultMcs = par("defaultMcs");
    }
}

int HeUlSchedulerBase::computeRandomAccessRuCount(const ScheduleContext& context, int availableRus) const
{
    int demand = context.estimatedRandomAccessContenders;
    int feedback = (int)std::round(context.recentRandomAccessCollisionRate * 2 -
            context.recentRandomAccessIdleRate * 2);
    int requested = std::clamp(demand + feedback, minRandomAccessRus, maxRandomAccessRus);
    return std::clamp(requested, 0, availableRus);
}

int HeUlSchedulerBase::computeTargetRssiDbm(const ScheduleContext& context) const
{
    return (int)std::round(context.apSensitivityDbm + context.targetRssiMarginDb);
}

simtime_t HeUlSchedulerBase::computeCommonDuration(const ScheduleContext& context,
        const std::vector<RuAllocation>& allocations) const
{
    simtime_t duration = context.requestedDuration;
    if (duration <= SIMTIME_ZERO) {
        duration = SIMTIME_ZERO;
        for (const auto& allocation : allocations)
            duration = std::max(duration, allocation.estimatedDuration);
    }
    if (context.txopLimit > SIMTIME_ZERO)
        duration = std::min(duration, context.txopLimit);
    return duration;
}

} // namespace ieee80211
} // namespace inet
