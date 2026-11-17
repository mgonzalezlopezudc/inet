// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.networklayer.icmpv6.Ipv6NeighbourDiscovery;

simple LowpanIpv6NeighbourDiscovery extends Ipv6NeighbourDiscovery
{
    parameters:
        @class(LowpanIpv6NeighbourDiscovery);
        string nativeNdRole @enum("static", "router", "host") = default("static"); // Native router requires forwarding; host uses M=0 discovery/SLAAC/registration and drops ordinary data until registered. Static retains configured bindings. Native ND always computes wire checksums; checksumMode governs ordinary interfaces.
        int maxRegistrations = default(64); // Positive module-wide registration limit across native interfaces.
        int maxPendingRouterAdvertisements = default(64); // Positive module-wide bound for delayed unicast RA responses; identical pending requester tuples are coalesced.
        int maxCandidateRouters = default(4); // Positive module-wide host router bound.
        int maxHostAddresses = default(16); // Positive module-wide host address inventory bound, including configured, autonomous and rejected addresses.
        int nativeRegistrationLifetime = default(1); // Host ARO lifetime in minutes, range 1..65535.
        double nativeRegistrationRetryInterval @unit(s) = default(1s); // At least one second; registration transmits at most three requests per router/address attempt.
        double nativeRegistrationRefreshFraction = default(0.8); // Strictly between zero and one; fraction of accepted lifetime before refresh.
        @signal[packetDropped](type=inet::Packet);
        @statistic[packetDropped](record=count);
        @signal[registrationCountChanged](type=long);
        @statistic[registrationCount](source=registrationCountChanged;record=last,max);
        @signal[nativeRouterDiscovered](type=inet::Packet);
        @signal[nativeHostRegistrationSucceeded](type=inet::Packet);
        @statistic[nativeRouterDiscovered](record=count);
        @statistic[nativeHostRegistrationSucceeded](record=count);
}
