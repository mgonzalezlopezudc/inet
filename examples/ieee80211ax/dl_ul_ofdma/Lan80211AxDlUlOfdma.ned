// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.dl_ul_ofdma;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

//
// \brief Bidirectional downlink and uplink OFDMA comparison network.
//
// Three stations generate downlink and uplink traffic at the same time. The
// corresponding INI configurations compare HE multi-user scheduling with a
// matched single-user EDCA baseline.
//
network Lan80211AxDlUlOfdma extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
