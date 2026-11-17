// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.networklayer.icmpv6.Ipv6NeighbourDiscovery;

simple LowpanIpv6NeighbourDiscovery extends Ipv6NeighbourDiscovery
{
    parameters:
        @class(LowpanIpv6NeighbourDiscovery);
        string nativeNdRole @enum("static", "router", "host", "borderRouter") = default("static"); // Router enables native registration/RA; borderRouter additionally couples native admission to the routed DAD authority. Both require forwarding. Host uses M=0 discovery/SLAAC/registration and drops ordinary data until registered. Static retains configured bindings. Native ND always computes wire checksums; checksumMode governs ordinary interfaces.
        int maxDuplicateAddressEntries = default(64); // Positive module-wide capacity of the borderRouter DAD table; no native binding or data route is created by this table.
        int maxRegistrations = default(64); // Positive module-wide registration limit across native interfaces.
        string registrationAuthorityAddress = default(""); // Router role: one non-link-local routed DAD authority; empty retains local registration.
        string registrationSourceAddress = default(""); // Assigned valid non-tentative non-loopback non-link-local DAR source; required with authority. Pending+confirmed keys share maxRegistrations.
        int maxPendingRouterAdvertisements = default(64); // Positive module-wide bound for delayed unicast RA responses; identical pending requester tuples are coalesced.
        int maxCandidateRouters = default(4); // Positive module-wide host router bound.
        int maxHostAddresses = default(16); // Positive module-wide host address inventory bound, including configured, autonomous and rejected addresses.
        int nativeRegistrationLifetime = default(1); // Host ARO lifetime in minutes, range 1..65535.
        double nativeRegistrationRetryInterval @unit(s) = default(1s); // At least one second; floor for the advertised per-interface RetransTimer. Three requests per attempt, with twice the effective interval to await the final response.
        double nativeRegistrationRefreshFraction = default(0.8); // Strictly between zero and one; fraction of accepted lifetime before refresh.
        bool contextDistribution = default(false); // Native ABRO/6CO learning and distribution. Reuses inherited RA intervals; learned CIDs must be disjoint from configured slots.
        string contextAuthorityAddress = default(""); // borderRouter origin: explicit non-link-local unicast authority identity.
        int contextInitialVersion = default(0); // Origin unsigned 32-bit version, preserved across restart; semantic updates at UINT32_MAX are rejected.
        int contextAuthorityLifetime = default(0); // Origin ABRO lifetime in minutes, 0..65535; zero means the RFC default 10000 minutes.
        int maxContextAuthorityGroups = default(4); // Positive module-wide bound; separate from candidate-default-router capacity.
        @signal[packetDropped](type=inet::Packet);
        @statistic[packetDropped](record=count);
        @signal[registrationCountChanged](type=long);
        @statistic[registrationCount](source=registrationCountChanged;record=last,max);
        @signal[nativeRouterDiscovered](type=inet::Packet);
        @signal[nativeHostRegistrationSucceeded](type=inet::Packet);
        @statistic[nativeRouterDiscovered](record=count);
        @statistic[nativeHostRegistrationSucceeded](record=count);
        @signal[duplicateAddressEntryCountChanged](type=long);
        @statistic[duplicateAddressEntryCount](source=duplicateAddressEntryCountChanged;record=last,max);
}
