// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.networklayer.icmpv6.Ipv6NeighbourDiscovery;

simple LowpanIpv6NeighbourDiscovery extends Ipv6NeighbourDiscovery
{
    parameters:
        @class(LowpanIpv6NeighbourDiscovery);
        @signal[packetDropped](type=inet::Packet);
        @statistic[packetDropped](record=count);
}
