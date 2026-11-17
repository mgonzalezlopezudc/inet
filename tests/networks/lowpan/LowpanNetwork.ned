// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.tests.lowpan;

import inet.node.inet.StandardHost;
import inet.linklayer.lowpan.LowpanLinkDomain;
import inet.physicallayer.wireless.ieee802154.packetlevel.Ieee802154NarrowbandScalarRadioMedium;

// Explicit legacy compatibility link; native-wire acceptance is a later gate.
network LowpanNetwork
{
    submodules:
        radioMedium: Ieee802154NarrowbandScalarRadioMedium { @display("p=100,50"); }
        domain: LowpanLinkDomain { @display("p=250,50"); }
        node[2]: StandardHost { @display("p=100,160,row,180"); }
}
