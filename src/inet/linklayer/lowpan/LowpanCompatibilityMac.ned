// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.linklayer.ieee802154.Ieee802154Mac;

// Non-native legacy wire profile with a truthful 23-octet header budget.
module LowpanCompatibilityMac extends Ieee802154Mac
{
    parameters:
        string linkDomainModule = default(""); // Required shared compatibility domain.
        double maxLowerDeliveryLifetime @unit(s) = default(1s); // Enforced deadline, not a latency guarantee.
        upperLayerProtocol = "lowpan";
        headerLength = 184b;
        mtu = 1280B; // IPv6 service MTU; prepared MAC payload limit is independent.
        useMACAcks = default(true);
        backoffMethod = default("exponential");
        macMaxCSMABackoffs = default(4);
        macMaxBE = default(5);
        @class(LowpanCompatibilityMac);
}
