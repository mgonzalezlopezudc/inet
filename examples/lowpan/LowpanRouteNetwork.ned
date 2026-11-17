// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.examples.lowpan;
import inet.networklayer.configurator.ipv6.Ipv6NetworkConfigurator;
import inet.node.inet.StandardHost;
import inet.linklayer.lowpan.LowpanNativeLinkDomain;
import inet.physicallayer.wireless.ieee802154.packetlevel.Ieee802154NarrowbandScalarRadioMedium;
network LowpanRouteNetwork
{
    submodules:
        mediumLeft: Ieee802154NarrowbandScalarRadioMedium;
        mediumRight: Ieee802154NarrowbandScalarRadioMedium;
        domainLeft: LowpanNativeLinkDomain;
        domainRight: LowpanNativeLinkDomain;
        node[3]: StandardHost;
        configurator: Ipv6NetworkConfigurator;
}
