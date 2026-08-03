//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.dl_ofdma_bar;

import inet.examples.ieee80211.common.SingleBssNetwork;

//
// \brief Downlink OFDMA Block Ack Request (BAR) simulation network.
//
// Extends \p SingleBssNetwork with 3 wireless hosts to compare the performance
// of sequential un-triggered Block Ack Requests versus scheduled Multi-User BAR
// (MU-BAR) Trigger frames in downlink OFDMA scenarios.
//
network Lan80211AxDlOfdmaBar extends SingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
