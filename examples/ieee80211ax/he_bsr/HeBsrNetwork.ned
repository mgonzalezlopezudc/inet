// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.he_bsr;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

network HeBsrNetwork extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(3);
        @display("bgb=620,420");
}
