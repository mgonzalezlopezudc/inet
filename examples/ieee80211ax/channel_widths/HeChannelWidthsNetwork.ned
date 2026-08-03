// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.channel_widths;

import inet.examples.ieee80211.common.SingleBssNetwork;

//
// \brief Channel Widths simulation network.
//
// This network extends the base \p SingleBssNetwork with a default of 4 wireless hosts.
// It is used to simulate different bandwidth configurations (20, 40, 80, 160 MHz).
//
network HeChannelWidthsNetwork extends SingleBssNetwork
{
    parameters:
        numHosts = default(4);
}
