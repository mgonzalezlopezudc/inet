//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.showcases.wireless.tgnchannel;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.WirelessHost;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211TgnRadioMedium;

network TgnChannelShowcase
{
    parameters:
        @display("bgb=600,300");
    submodules:
        configurator: Ipv4NetworkConfigurator {
            @display("p=70,50");
        }
        radioMedium: Ieee80211TgnRadioMedium {
            @display("p=70,130");
        }
        sourceHost: WirelessHost {
            @display("p=250,150");
        }
        destinationHost: WirelessHost {
            @display("p=350,150");
        }
}
