// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.dl_ul_ofdma;

import inet.examples.ieee80211.common.SingleBssNetwork;

//
// \brief Simultaneous Downlink and Uplink OFDMA simulation network.
//
// Extends \p SingleBssNetwork with 3 wireless hosts to model bidirectional
// multi-user traffic where the AP and client stations contend and trigger
// simultaneous transmissions in opposite directions.
//
network Lan80211AxDlUlOfdma extends SingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
