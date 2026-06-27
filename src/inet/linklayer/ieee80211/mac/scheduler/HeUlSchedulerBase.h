//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULSCHEDULERBASE_H
#define __INET_HEULSCHEDULERBASE_H

#include <string>

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeRateControl.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h"

namespace inet {
namespace ieee80211 {

/**
 * Shared implementation for HE uplink OFDMA schedulers.
 *
 * It determines the bounded UORA reservation, requested target RSSI, and a
 * common HE-TB duration after a derived scheduler has selected its users.
 */
class INET_API HeUlSchedulerBase : public IIeee80211HeUlScheduler, public SimpleModule
{
  protected:
    int maxMuStations = 8;
    int minRandomAccessRus = 1;
    int maxRandomAccessRus = 4;
    int defaultMcs = 0;
    IIeee80211HeRateControl *heRateControl = nullptr;
    int lastCandidateCount = 0;
    int lastScheduledUserCount = 0;
    int lastRandomAccessRuCount = 0;
    int lastTargetRssiDbm = 0;
    simtime_t lastCommonDuration = SIMTIME_ZERO;
    Hz lastChannelBandwidth = Hz(NaN);
    std::string lastSchedulingReason = "not scheduled yet";
    std::vector<CandidateInfo> lastCandidates;
    std::vector<RuAllocation> lastRuAllocations;

  protected:
    virtual void initialize(int stage) override;
    virtual int computeRandomAccessRuCount(const ScheduleContext& context, int availableRus) const;
    virtual int computeTargetRssiDbm(const ScheduleContext& context) const;
    virtual int selectMcs(const ScheduleContext& context, const CandidateInfo& candidate,
            const physicallayer::Ieee80211HeRu& ru) const;
    virtual simtime_t computeCommonDuration(const ScheduleContext& context,
            const std::vector<RuAllocation>& allocations) const;
    void recordSchedule(const ScheduleContext& context, const Schedule& schedule, const char *reason);
    std::string getLastScheduleSummary() const;
};

} // namespace ieee80211
} // namespace inet

#endif
