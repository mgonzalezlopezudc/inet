//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLSCHEDULERBASE_H
#define __INET_HEDLSCHEDULERBASE_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"

namespace inet {
namespace ieee80211 {

/**
 * Common standards-aware machinery for queue-sensitive HE downlink schedulers.
 *
 * Derived classes supply a ranking policy. This base class translates backlog
 * into standard RU requests, estimates SNR/MCS and duration, and shrinks or
 * expands RUs to fit a valid, duration-aligned MU PPDU.
 */
class INET_API HeDlSchedulerBase : public IIeee80211HeDlScheduler, public SimpleModule
{
  protected:
    int maxMuStations = -1;
    int smallBacklogThreshold = 80;
    int mediumBacklogThreshold = 500;
    int mtuBacklogThreshold = 1500;
    int largeBacklogThreshold = 6000;
    double thermalNoisePsdDbmHz = -174;
    double lowDurationRatio = 0.5;
    double highDurationRatio = 1.5;
    int maxDurationAlignmentIterations = 16;
    std::vector<double> mcsSnrThresholds;

  protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }

    virtual int requestRuForBytes(int64_t bytes, Hz channelBandwidth) const;
    virtual int getNextSmallerRu(int toneSize) const;
    virtual int getNextLargerRu(int toneSize) const;
    virtual double estimateSnrDb(const ScheduleContext& context, const CandidateInfo& candidate,
            const physicallayer::Ieee80211HeRu& ru) const;
    virtual int selectMcs(double snrDb, bool hasFreshPathLoss) const;
    virtual simtime_t estimateDuration(int64_t bytes, int toneSize, int mcs,
            physicallayer::Ieee80211HeGuardInterval guardInterval) const;

    std::vector<RuAllocation> fitRequestedRus(const ScheduleContext& context,
            const std::vector<CandidateInfo>& candidates, std::vector<int> requestedTones,
            const std::vector<int64_t>& payloadBytes) const;
    static bool defaultCandidateLess(const CandidateInfo& a, const CandidateInfo& b);

  public:
    virtual std::vector<RuAllocation> schedule(const std::vector<MacAddress>& candidates,
            Hz channelCenterFrequency, Hz channelBandwidth) override;
    virtual std::vector<RuAllocation> schedule(const ScheduleContext& context) override = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
