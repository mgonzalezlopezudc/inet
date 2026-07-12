// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.dl_ofdma;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

//
// \brief Downlink OFDMA and Downlink MU-MIMO simulation network.
//
// This network extends the base \p HeSingleBssNetwork with a default of 3 wireless hosts.
// It is used to simulate downlink multi-user transmissions where the AP acts as a
// transmitter, scheduling concurrent payloads to the client stations either in different
// frequency subchannels (OFDMA) or using multiple spatial streams (MU-MIMO).
//
network Lan80211AxDlOfdma extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(3);
}
