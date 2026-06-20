//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTMPDUAGGREGATIONPOLICY_H
#define __INET_VHTMPDUAGGREGATIONPOLICY_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/contract/IMpduAggregationPolicy.h"

namespace inet {
namespace ieee80211 {

/**
 * MPDU aggregation policy that enforces the IEEE 802.11 VHT A-MPDU limit.
 *
 * The limit is derived from the VHT maximum-A-MPDU-length exponent. The
 * policy returns a prefix of the supplied MPDUs, so callers retain unsent
 * packets for a subsequent transmission opportunity.
 */
class INET_API VhtMpduAggregationPolicy : public IMpduAggregationPolicy, public SimpleModule
{
  protected:
    int maxAmpduLengthExponent = 7; // default to exponent 7 (1,048,575 bytes)

  protected:
    virtual void initialize() override;

  public:
    VhtMpduAggregationPolicy() {}
    virtual ~VhtMpduAggregationPolicy() {}

    void setMaxAmpduLengthExponent(int exponent) { maxAmpduLengthExponent = exponent; }
    int getMaxAmpduLengthExponent() const { return maxAmpduLengthExponent; }

    virtual std::vector<Packet *> *computeAggregateFrames(std::vector<Packet *> *frames) override;
};

} // namespace ieee80211
} // namespace inet

#endif
