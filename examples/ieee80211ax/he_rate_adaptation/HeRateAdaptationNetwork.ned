// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.he_rate_adaptation;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;

//
// \brief Rate adaptation algorithm comparison simulation network.
//
// This network extends the base \p HeSingleBssNetwork with a default of 4 wireless hosts.
// It is used to evaluate rate adaptation algorithms (such as Minstrel and Minstrel Mobile)
// under various SNR and packet error conditions, showing how they dynamically adjust
// modulation and coding schemes (MCS) to maximize throughput.
//
network HeRateAdaptationNetwork extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(4);
        @display("bgb=700,420");
}
