// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.examples.lowpan;

import inet.node.inet.StandardHost;
import inet.linklayer.lowpan.LowpanNativeLinkDomain;
import inet.physicallayer.wireless.ieee802154.packetlevel.Ieee802154NarrowbandScalarRadioMedium;

// Static native IEEE 802.15.4 link with RFC 4944/6282 adaptation.
network LowpanNetwork
{
    submodules:
        radioMedium: Ieee802154NarrowbandScalarRadioMedium { @display("p=100,50"); }
        domain: LowpanNativeLinkDomain { @display("p=250,50"); }
        node[2]: StandardHost { @display("p=100,160,row,180"); }
}
