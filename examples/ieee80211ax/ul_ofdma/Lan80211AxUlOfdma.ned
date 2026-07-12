// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.ul_ofdma;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

//
// \brief Uplink OFDMA, Uplink MU-MIMO, and UORA simulation network.
//
// This network extends the base \p HeSingleBssNetwork with a default of 3 wireless hosts.
// It is used to simulate uplink multi-user transmissions where multiple client stations
// transmit concurrently to the AP. Transactions are either scheduled by the AP coordinator
// (via Basic or BSRP Trigger frames) or contested by the stations on random-access RUs
// using Uplink OFDMA Random Access (UORA).
//
network Lan80211AxUlOfdma extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
