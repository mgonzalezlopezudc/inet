//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBase.h"

#include <sstream>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"

// Base class for HE UL OFDMA schedulers.
//
// Common helpers for UL scheduler implementations:
//   - Computing the number of random-access RUs for UORA (Clause 26.5.4).
//   - Selecting the target RSSI for triggered STAs (Clause 26.5.2.3).
//   - Computing the common HE-TB PPDU duration (Clause 27.3.11.12).
//
// Implementation notes:
//   - computeRandomAccessRuCount() uses a heuristic feedback formula based on
//     estimated contenders, collision rate and idle rate.  IEEE 802.11-2024 does
//     not specify how many RA-RUs an AP should advertise.
//   - computeTargetRssiDbm() is a simple sensitivity-plus-margin approximation
//     of the Target RSSI field defined in Clause 26.5.2.3; it does not implement
//     the full TPC/TMI handling described in the standard.

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
        const char *heRateControlModule = par("heRateControlModule");
        heRateControl = *heRateControlModule == '\0' ? nullptr :
                dynamic_cast<IIeee80211HeRateControl *>(getModuleByPath(heRateControlModule));
        if (*heRateControlModule != '\0' && heRateControl == nullptr)
            throw cRuntimeError("heRateControlModule '%s' is not an IIeee80211HeRateControl", heRateControlModule);
        ASSERT(maxMuStations >= 0);
        ASSERT(minRandomAccessRus >= 0);
        ASSERT(minRandomAccessRus <= maxRandomAccessRus);
        ASSERT(defaultMcs >= 0 && defaultMcs <= 11);
        WATCH(lastCandidateCount);
        WATCH(lastScheduledUserCount);
        WATCH(lastRandomAccessRuCount);
        WATCH(lastTargetRssiDbm);
        WATCH(lastCommonDuration);
        WATCH_EXPR("lastChannelBandwidth", lastChannelBandwidth.str());
        WATCH(lastSchedulingReason);
        WATCH_VECTOR(lastCandidates);
        WATCH_VECTOR(lastRuAllocations);
        WATCH_EXPR("lastScheduleSummary", getLastScheduleSummary());
    }
}

int HeUlSchedulerBase::computeRandomAccessRuCount(const ScheduleContext& context, int availableRus) const
{
    ASSERT(availableRus >= 0);
    ASSERT(context.estimatedRandomAccessContenders >= 0);
    ASSERT(context.recentRandomAccessCollisionRate >= 0 && context.recentRandomAccessCollisionRate <= 1);
    ASSERT(context.recentRandomAccessIdleRate >= 0 && context.recentRandomAccessIdleRate <= 1);
    // Heuristic: increase RA-RUs when collisions dominate, decrease when many
    // RA-RUs are idle.  The standard leaves the number of advertised RA-RUs to
    // the AP's discretion (Clause 26.5.4); this is one possible policy.
    int demand = context.estimatedRandomAccessContenders;
    int feedback = (int)std::round(context.recentRandomAccessCollisionRate * 2 -
            context.recentRandomAccessIdleRate * 2);
    int requested = std::clamp(demand + feedback, minRandomAccessRus, maxRandomAccessRus);
    return std::clamp(requested, 0, availableRus);
}

int HeUlSchedulerBase::computeTargetRssiDbm(const ScheduleContext& context) const
{
    // Approximation of the Target RSSI subfield in the Trigger frame user info
    // (Clause 26.5.2.3).  The standard allows the AP to request a specific
    // received RSSI at the AP; here we approximate it as sensitivity + margin.
    return (int)std::round(context.apSensitivityDbm + context.targetRssiMarginDb);
}

int HeUlSchedulerBase::selectMcs(const ScheduleContext& context, const CandidateInfo& candidate,
        const physicallayer::Ieee80211HeRu& ru) const
{
    if (heRateControl == nullptr || candidate.associationId == 0)
        return defaultMcs;
    IIeee80211HeRateControl::Constraints constraints;
    constraints.maxMcs = 9; // keep HE-TB robust unless LDPC/user constraints explicitly widen later
    if (candidate.hasFreshPathLoss)
        heRateControl->reportHeRxSnir(candidate.staAddress,
                context.targetRssiMarginDb + context.apSensitivityDbm - context.apSensitivityDbm);
    auto selection = heRateControl->selectHeMode(candidate.staAddress, context.channelBandwidth,
            ru.toneSize, physicallayer::HE_TRIGGER_BASED_UPLINK, 1, constraints);
    return selection.mode == nullptr ? defaultMcs : selection.mcs;
}

simtime_t HeUlSchedulerBase::computeCommonDuration(const ScheduleContext& context,
        const std::vector<RuAllocation>& allocations) const
{
    ASSERT(context.requestedDuration >= SIMTIME_ZERO);
    ASSERT(context.txopLimit >= SIMTIME_ZERO);
    simtime_t duration = context.requestedDuration;
    if (duration <= SIMTIME_ZERO) {
        duration = SIMTIME_ZERO;
        for (const auto& allocation : allocations)
            // IEEE 802.11-2024 Clause 27.3.11.12: all HE-TB users transmit with
            // the same number of symbols and are padded to the common duration.
            // The slowest selected user therefore determines that duration.
            duration = std::max(duration, allocation.estimatedDuration);
    }
    if (context.txopLimit > SIMTIME_ZERO)
        duration = std::min(duration, context.txopLimit);
    duration = std::min(duration, SimTime(5.484, SIMTIME_MS));
    return duration;
}

void HeUlSchedulerBase::recordSchedule(const ScheduleContext& context, const Schedule& schedule, const char *reason)
{
    lastCandidateCount = context.candidates.size();
    lastScheduledUserCount = 0;
    lastRandomAccessRuCount = 0;
    lastTargetRssiDbm = schedule.allocations.empty() ? computeTargetRssiDbm(context) : schedule.allocations.front().targetRssiDbm;
    for (const auto& allocation : schedule.allocations) {
        if (allocation.randomAccess)
            lastRandomAccessRuCount++;
        else
            lastScheduledUserCount++;
    }
    lastCommonDuration = schedule.commonDuration;
    lastChannelBandwidth = context.channelBandwidth;
    lastSchedulingReason = reason == nullptr ? "" : reason;
    lastCandidates = context.candidates;
    lastRuAllocations = schedule.allocations;
}

std::string HeUlSchedulerBase::getLastScheduleSummary() const
{
    std::stringstream stream;
    stream << lastSchedulingReason
           << ": candidates=" << lastCandidateCount
           << ", scheduled=" << lastScheduledUserCount
           << ", randomAccessRUs=" << lastRandomAccessRuCount
           << ", targetRssi=" << lastTargetRssiDbm
           << " dBm, bandwidth=" << lastChannelBandwidth
           << ", duration=" << lastCommonDuration;
    return stream.str();
}

} // namespace ieee80211
} // namespace inet
