// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.ul_ofdma;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

network Lan80211AxUlOfdma extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
