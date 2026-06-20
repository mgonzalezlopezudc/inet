//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLSCHEDULEREQUALIZEDRUS_H
#define __INET_HEDLSCHEDULEREQUALIZEDRUS_H

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerBase.h"

namespace inet {
namespace ieee80211 {

/**
 * Downlink OFDMA scheduler that partitions the primary channel into N
 * equal-bandwidth Resource Units (RUs) and assigns one RU per selected STA.
 * At most maxMuStations candidates are scheduled per TXOP.
 */
class INET_API HeDlSchedulerEqualSizedRUs : public HeDlSchedulerBase
{
  protected:
    std::string schedulingFunction;
    int nextAnchorIndex = 0;

  protected:
    virtual void initialize(int stage) override;
  public:
    using HeDlSchedulerBase::schedule;
    virtual std::vector<RuAllocation> schedule(const ScheduleContext& context) override;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEDLSCHEDULEREQUALIZEDRUS_H
