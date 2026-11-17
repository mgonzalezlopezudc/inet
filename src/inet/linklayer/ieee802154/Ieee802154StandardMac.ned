// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;

import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154MacServiceProvider;
import inet.linklayer.ieee802154.contract.IIeee802154AccessClient;
import inet.linklayer.ieee802154.contract.IIeee802154ManagementServiceProvider;
import inet.physicallayer.wireless.ieee802154.contract.IIeee802154PhyServiceClient;

// Native service owner with explicitly selected managed receive capability.
simple Ieee802154StandardMac extends SimpleModule like IIeee802154MacServiceProvider, IIeee802154AccessClient, IIeee802154PhyServiceClient, IIeee802154ManagementServiceProvider
{
    parameters:
        string clientModule = default("^.adapter");
        string managementClientModule = default("");
        bool enableManagement = default(false);
        int pendingCapacity = default(16);
        int scanCapacity = default(16);
        double pollResponseWait @unit(s) = default(2s); // Model bound for the selected peer's response access delay.
        string interfaceModule = default("^");
        string accessModule = default("");
        string providerModule = default("");
        string extendedAddress = default(ieee802154LocalAddress());
        int shortAddress = default(65535);
        int panId = default(65535);
        int dsnRng = default(0);
        bool rxOnWhenIdle = default(false);
        bool isPanCoordinator = default(false);
        double maxPropagationDelay @unit(s) = default(1us);
        double initialCcaDuration @unit(s) = default(128us);
        @lifecycleSupport;
        @class(Ieee802154StandardMac);
        @display("i=block/rxtx");
        @signal[packetSentToLower](type=inet::Packet);
        @signal[packetReceivedFromLower](type=inet::Packet);
}
