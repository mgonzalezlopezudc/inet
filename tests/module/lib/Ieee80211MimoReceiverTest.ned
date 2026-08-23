//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.test.moduletest.lib;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.WirelessHost;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211TgnRadioMedium;

network Ieee80211MimoReceiverTest
{
    submodules:
        configurator: Ipv4NetworkConfigurator;
        radioMedium: Ieee80211TgnRadioMedium;
        singleStreamSource: WirelessHost;
        spatialStreamSource: WirelessHost;
        destination: WirelessHost;
}
