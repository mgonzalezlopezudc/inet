// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.ul_ofdma;

import inet.examples.ieee80211.common.SingleBssNetwork;

//
// \brief Uplink Multi-User OFDMA simulation network.
//
// This network extends the base \p SingleBssNetwork with a default of 3 wireless hosts.
// It models scheduled uplink OFDMA transmissions where the AP issues Trigger frames
// and stations transmit simultaneously on assigned RUs.
//
network Lan80211AxUlOfdma extends SingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
