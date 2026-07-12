// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.he_channel_widths;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

//
// \brief Contiguous channel width comparison simulation network.
//
// This network extends the base \p HeSingleBssNetwork with a default of 4 wireless hosts.
// It is used to compare physical layer bandwidth and transmission delay differences
// across contiguous 20, 40, 80, and 160 MHz configurations.
//
network HeChannelWidthsNetwork extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(4);
}
