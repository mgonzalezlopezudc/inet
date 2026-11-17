// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;

import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154MacServiceProvider;
import inet.linklayer.ieee802154.contract.IIeee802154AccessClient;
import inet.physicallayer.wireless.ieee802154.contract.IIeee802154PhyServiceClient;

// Native direct DATA/ACK owner. PAN management is added in N07.
simple Ieee802154StandardMac extends SimpleModule like IIeee802154MacServiceProvider, IIeee802154AccessClient, IIeee802154PhyServiceClient
{
    parameters:
        string clientModule = default("^.adapter");
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
