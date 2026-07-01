// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211be.mlo;

import inet.node.inet.StandardHost;
import inet.examples.ieee80211be.mlo.MldInterface;

//
// A host equipped with a Multi-Link Device (MLD) interface.
//
module MldHost extends StandardHost
{
    parameters:
        int numLinks = default(2);
        numWlanInterfaces = 0; // Disable standard WLAN interfaces
    submodules:
        mldWlan: MldInterface {
            @display("p=300,400");
        }
    connections allowunconnected:
        // Connect the MLD interface to the link-layer message dispatcher
        mldWlan.upperLayerOut --> li.in++;
        mldWlan.upperLayerIn <-- li.out++;
}
