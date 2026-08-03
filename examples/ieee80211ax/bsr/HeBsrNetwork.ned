// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.bsr;

import inet.examples.ieee80211.common.SingleBssNetwork;

//
// \brief Buffer Status Reports (BSR) simulation network.
//
// This network extends the base \p SingleBssNetwork with a default of 3 wireless hosts.
// It is used to simulate uplink OFDMA scheduling where stations send BSR queue information
// to the AP (either implicitly in MAC data frames or explicitly in BSRP responses) so the AP
// can plan RU allocations accordingly.
//
network HeBsrNetwork extends SingleBssNetwork
{
    parameters:
        numHosts = default(3);
        @display("bgb=620,420");
}
