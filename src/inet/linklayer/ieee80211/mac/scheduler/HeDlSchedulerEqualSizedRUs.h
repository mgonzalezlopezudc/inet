//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLSCHEDULEREQUALIZEDRUS_H
#define __INET_HEDLSCHEDULEREQUALIZEDRUS_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"

namespace inet {
namespace ieee80211 {

/**
 * Downlink OFDMA scheduler that partitions the primary channel into N
 * equal-bandwidth Resource Units (RUs) and assigns one RU per selected STA.
 * At most maxMuStations candidates are scheduled per TXOP.
 */
class INET_API HeDlSchedulerEqualSizedRUs : public IIeee80211HeDlScheduler, public SimpleModule
{
  protected:
    int maxMuStations = 4;

  protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }

  public:
    virtual std::vector<RuAllocation> schedule(
            const std::vector<MacAddress>& candidates,
            Hz channelCenterFrequency,
            Hz channelBandwidth) override;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEDLSCHEDULEREQUALIZEDRUS_H
