// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.frequency_selective_channel;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;
import inet.node.inet.WirelessHost;

//
// A single-BSS topology for studying frequency-selective impairments.
//
// The four stations make the frequency-domain effect easy to see: an HE MU
// transmission can assign a separate RU to several stations, while the matched
// SU configuration uses the whole configured channel for one station at a time.
//
network FrequencySelectiveChannelNetwork extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(4);
        bool hasInterferer = default(false);
        @display("bgb=700,500");
    submodules:
        interferer: WirelessHost if hasInterferer {
            wlan[*].mgmt.typename = default("Ieee80211MgmtAdhoc");
            wlan[*].agent.typename = "";
            @display("p=365,235;r=,,#d62728");
        }
}
