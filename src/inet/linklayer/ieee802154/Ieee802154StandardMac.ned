// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;

import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154MacServiceProvider;
import inet.linklayer.ieee802154.contract.IIeee802154AccessClient;
import inet.linklayer.ieee802154.contract.IIeee802154ManagementServiceProvider;
import inet.linklayer.ieee802154.contract.IIeee802154SuperframeClient;
import inet.physicallayer.wireless.ieee802154.contract.IIeee802154PhyServiceClient;

// Native service owner with explicitly selected managed receive capability.
simple Ieee802154StandardMac extends SimpleModule like IIeee802154MacServiceProvider, IIeee802154AccessClient, IIeee802154PhyServiceClient, IIeee802154ManagementServiceProvider, IIeee802154SuperframeClient
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
        string slottedAccessModule = default("");
        string providerModule = default("");
        string superframeModule = default("");
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
        @signal[periodicBeaconStart](type=long); // Actual first-symbol local time, low 24 bits in symbols.
        @signal[outgoingCapActive](type=bool); // Outgoing CAP receive duty starts/ends.
        @signal[syncMissed](type=long); // Consecutive acquisition or tracking failures.
        @signal[syncLost](type=long); // BEACON_LOST after four failures.
        @signal[capWindowInstalled](type=long); // Borrowed Ieee802154MacWindowObservation: physical CAP bounds and origin.
        @signal[cfpWindowInstalled](type=long); // Borrowed Ieee802154MacWindowObservation: physical CFP bounds and role.
        @signal[gtsAllocated](type=long); // Coordinator ledger allocation; borrowed Ieee802154MacGtsObservation.
        @signal[gtsDeallocated](type=long); // Individual coordinator deallocation, excluding lifecycle-wide clears.
        @signal[gtsActivity](type=long); // Qualifying received DATA or matching ACK, not mode/configuration changes.
        @statistic[gtsAllocations](source=gtsAllocated; record=count; interpolationmode=none);
        @statistic[gtsDeallocations](source=gtsDeallocated; record=count; interpolationmode=none);
        @statistic[gtsActivities](source=gtsActivity; record=count; interpolationmode=none);
}
