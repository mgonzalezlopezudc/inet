// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.he_features;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;
import inet.node.inet.WirelessHost;

//
// 802.11ax HE advanced-feature showcase network.
//
// The topology consists of a wired server, one access point, and four wireless
// stations on an 80 MHz channel.  The 80 MHz channel is required because HE
// preamble puncturing is only defined for 80 MHz and 160 MHz operation.  The
// four hosts let the example demonstrate puncturing of one 20 MHz subchannel
// while still serving the remaining three stations concurrently.
//
network Lan80211AxHeFeatures extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(4);
        bool hasInterferer = default(false);
        @display("bgb=700,500");
    submodules:
        interferer: WirelessHost if hasInterferer {
            @display("p=350,150;r=,,#707070");
            wlan[*].mgmt.typename = default("Ieee80211MgmtStaSimplified");
            wlan[*].agent.typename = "";
        }
}
