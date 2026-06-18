//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLSCHEDULERBACKLOGBASED_H
#define __INET_HEDLSCHEDULERBACKLOGBASED_H

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerBase.h"

namespace inet {
namespace ieee80211 {

class INET_API HeDlSchedulerBacklogBased : public HeDlSchedulerBase
{
  protected:
    double deltaPlMax = 10;

  protected:
    virtual void initialize(int stage) override;

  public:
    using HeDlSchedulerBase::schedule;
    virtual std::vector<RuAllocation> schedule(const ScheduleContext& context) override;
};

} // namespace ieee80211
} // namespace inet

#endif
