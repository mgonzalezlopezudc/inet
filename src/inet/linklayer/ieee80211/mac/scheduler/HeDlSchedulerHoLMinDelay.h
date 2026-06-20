//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLSCHEDULERHOLMINDELAY_H
#define __INET_HEDLSCHEDULERHOLMINDELAY_H

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerBase.h"

namespace inet {
namespace ieee80211 {

/** HE DL scheduler that ranks eligible STAs by head-of-line packet delay. */
class INET_API HeDlSchedulerHoLMinDelay : public HeDlSchedulerBase
{
  public:
    using HeDlSchedulerBase::schedule;
    virtual std::vector<RuAllocation> schedule(const ScheduleContext& context) override;
};

} // namespace ieee80211
} // namespace inet

#endif
