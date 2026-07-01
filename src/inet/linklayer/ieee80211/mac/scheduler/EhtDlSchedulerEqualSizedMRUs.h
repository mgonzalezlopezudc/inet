//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_EHTDLSCHEDULEREQUALSIZEDMRUS_H
#define __INET_EHTDLSCHEDULEREQUALSIZEDMRUS_H

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.h"

namespace inet {
namespace ieee80211 {

/**
 * Downlink OFDMA scheduler for EHT that partitions the bandwidth into
 * equal-sized Multiple Resource Units (MRUs) and assigns one MRU per STA.
 */
class INET_API EhtDlSchedulerEqualSizedMRUs : public HeDlSchedulerEqualSizedRUs
{
  protected:
    virtual void initialize(int stage) override;
  public:
    virtual std::vector<IIeee80211HeDlScheduler::RuAllocation> schedule(const ScheduleContext& context) override;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_EHTDLSCHEDULEREQUALSIZEDMRUS_H
