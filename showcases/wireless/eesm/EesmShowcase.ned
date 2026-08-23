// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.showcases.wireless.eesm;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.WirelessHost;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211TgnRadioMedium;

network EesmShowcase
{
    submodules:
        configurator: Ipv4NetworkConfigurator;
        radioMedium: Ieee80211TgnRadioMedium;
        sourceHost: WirelessHost;
        destinationHost: WirelessHost;
}
