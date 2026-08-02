//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.dl_ofdma_bar;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

//
// \brief Downlink OFDMA Block Ack Request (BAR) signaling comparison network.
//
// Extends \p HeSingleBssNetwork with 3 wireless hosts to compare the performance
// and overhead of sequential unicast BARs versus MU-BAR Trigger frames in DL OFDMA,
// along with a Downlink Single-User (DL SU) baseline.
//
network Lan80211AxDlOfdmaBar extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
