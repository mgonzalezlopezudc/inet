//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.preamble_puncturing;

import inet.examples.ieee80211ax.common.HeSingleBssNetwork;
import inet.node.inet.WirelessHost;

network Lan80211AxPreamblePuncturing extends HeSingleBssNetwork
{
    parameters:
        numHosts = default(4);
        bool hasInterferer = default(false);
    submodules:
        interferer: WirelessHost if hasInterferer {
            @display("p=350,240");
        }
}
