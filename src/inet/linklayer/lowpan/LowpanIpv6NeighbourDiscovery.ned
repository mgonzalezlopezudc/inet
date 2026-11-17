// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.networklayer.icmpv6.Ipv6NeighbourDiscovery;

simple LowpanIpv6NeighbourDiscovery extends Ipv6NeighbourDiscovery
{
    parameters:
        @class(LowpanIpv6NeighbourDiscovery);
        string nativeNdRole @enum("static", "router") = default("static"); // Router requires IPv6 forwarding and enables local RFC 6775 ARO registration on native boundaries; static retains configured bindings. Native ND always computes wire checksums; checksumMode governs ordinary interfaces.
        int maxRegistrations = default(64); // Positive module-wide registration limit across native interfaces.
        int maxPendingRouterAdvertisements = default(64); // Positive module-wide bound for delayed unicast RA responses; identical pending requester tuples are coalesced.
        @signal[packetDropped](type=inet::Packet);
        @statistic[packetDropped](record=count);
        @signal[registrationCountChanged](type=long);
        @statistic[registrationCount](source=registrationCountChanged;record=last,max);
}
