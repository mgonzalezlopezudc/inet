//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULSCHEDULERBACKLOGBASED_H
#define __INET_HEULSCHEDULERBACKLOGBASED_H

#include "inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBase.h"

namespace inet {
namespace ieee80211 {

/** Capacity-aware HE UL scheduler with deterministic least-recently-served fairness. */
class INET_API HeUlSchedulerBacklogBased : public HeUlSchedulerBase
{
  protected:
    int maxOptimizedStations = 8;

    virtual void initialize(int stage) override;

  public:
    virtual Schedule schedule(const ScheduleContext& context) override;
};

} // namespace ieee80211
} // namespace inet

#endif
