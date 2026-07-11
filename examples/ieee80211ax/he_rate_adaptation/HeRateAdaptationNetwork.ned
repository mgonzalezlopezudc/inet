// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.he_rate_adaptation;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

network HeRateAdaptationNetwork extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(4);
        @display("bgb=700,420");
}
