//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.aggregation;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IMpduAggregationPolicy;

//
// VHT-aware MPDU aggregation policy.
//
// Select this policy for an IEEE 802.11ac (VHT) transmitter when an A-MPDU
// must honor the negotiated VHT maximum-A-MPDU-length exponent. The policy
// preserves packet order and returns the largest prefix that fits the limit;
// it does not fragment an MPDU. Configure the exponent from the peer's VHT
// capabilities when capability negotiation is modelled.
//
simple VhtMpduAggregationPolicy extends SimpleModule like IMpduAggregationPolicy
{
    parameters:
        @class(VhtMpduAggregationPolicy);
        @display("i=block/control");
        int maxAmpduLengthExponent = default(7); // VHT maximum A-MPDU exponent (0..7); 7 permits 1,048,575 bytes
}
