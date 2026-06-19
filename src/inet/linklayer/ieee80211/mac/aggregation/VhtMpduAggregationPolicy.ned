//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.aggregation;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IMpduAggregationPolicy;

//
// Implements a VHT-aware MPDU aggregation policy that limits size based on VHT exponent.
//
simple VhtMpduAggregationPolicy extends SimpleModule like IMpduAggregationPolicy
{
    parameters:
        @class(VhtMpduAggregationPolicy);
        @display("i=block/control");
        int maxAmpduLengthExponent = default(7); // 0-7, default 7 (1,048,575 bytes)
}
