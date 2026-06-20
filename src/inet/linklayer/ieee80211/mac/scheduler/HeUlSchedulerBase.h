//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULSCHEDULERBASE_H
#define __INET_HEULSCHEDULERBASE_H

#include "inet/common/SimpleModule.h"
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

  protected:
    virtual void initialize(int stage) override;
    virtual int computeRandomAccessRuCount(const ScheduleContext& context, int availableRus) const;
    virtual int computeTargetRssiDbm(const ScheduleContext& context) const;
    virtual simtime_t computeCommonDuration(const ScheduleContext& context,
            const std::vector<RuAllocation>& allocations) const;
};

} // namespace ieee80211
} // namespace inet

#endif
