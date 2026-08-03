// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.er_su;

import inet.examples.ieee80211.common.SingleBssNetwork;

//
// \brief Extended Range Single User (ER SU) and DCM simulation network.
//
// This network extends the base \p SingleBssNetwork with a default of 1 wireless host.
// It is used to simulate range extension techniques in 802.11ax, specifically
// HE ER SU transmission modes and Dual Carrier Modulation (DCM), which improve
// receiver SNR margins over long distances or in high attenuation conditions.
//
network HeErSuNetwork extends SingleBssNetwork
{
    parameters:
        numHosts = default(1);
}
