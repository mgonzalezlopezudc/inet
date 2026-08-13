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
    IIeee80211HeUlScheduler::RandomAccessTarget randomAccessTarget =
            IIeee80211HeUlScheduler::RandomAccessTarget::ASSOCIATED_STAS;
    int defaultMcs = 0;
    std::string mcsSelectionPolicy;
    std::vector<double> mcsSnrThresholds;
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
    uint64_t committedScheduleCount = 0;

  protected:
    virtual void initialize(int stage) override;
    virtual int computeRandomAccessRuCount(const ScheduleContext& context, int availableRus) const;
    virtual int computeTargetRssiDbm(const ScheduleContext& context) const;
    virtual double estimateSnrDb(const ScheduleContext& context, const CandidateInfo& candidate) const;
    virtual int selectMcsBySnr(double snrDb, const CandidateInfo& candidate,
            const physicallayer::Ieee80211HeRu& ru, int nss) const;
    virtual int selectMcs(const ScheduleContext& context, const CandidateInfo& candidate,
            const physicallayer::Ieee80211HeRu& ru, int nss = 1) const;
    virtual simtime_t computeCommonDuration(const ScheduleContext& context,
            const std::vector<RuAllocation>& allocations) const;
    std::string getLastScheduleSummary() const;

  public:
    virtual void commitSchedule(const ScheduleContext& context,
            const Schedule& schedule) override;
    int getCommittedCandidateCount() const { return lastCandidateCount; }
    int getCommittedScheduledUserCount() const { return lastScheduledUserCount; }
    int getCommittedRandomAccessRuCount() const { return lastRandomAccessRuCount; }
    const std::string& getCommittedSchedulingReason() const { return lastSchedulingReason; }
    uint64_t getCommittedScheduleCount() const { return committedScheduleCount; }
    virtual void invalidatePeer(const MacAddress& peer) override;
    IIeee80211HeUlScheduler::RandomAccessTarget getRandomAccessTarget() const
        { return randomAccessTarget; }
};

} // namespace ieee80211
} // namespace inet

#endif
