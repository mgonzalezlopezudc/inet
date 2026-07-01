// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211be.mlo;

import inet.node.wireless.AccessPoint;
import inet.examples.ieee80211be.mlo.MldInterface;

//
// An Access Point equipped with a Multi-Link Device (MLD) interface.
//
module MldAp extends AccessPoint
{
    parameters:
        int numLinks = default(2);
        numWlanInterfaces = 0; // Disable standard WLAN interfaces
    submodules:
        mldWlan: MldInterface {
            parameters:
                numLinks = parent.numLinks;
                @display("p=300,400");
        }
    connections allowunconnected:
        // Connect the MLD interface to the link-layer message dispatcher
        mldWlan.upperLayerOut --> li.in++;
        mldWlan.upperLayerIn <-- li.out++;
        

}
