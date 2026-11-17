// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.applications.ieee802154;

import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154MacServiceClient;
import inet.linklayer.ieee802154.contract.IIeee802154ManagementServiceClient;

// Example PAN-selection/allocation policy and two complete join/data/poll/leave cycles.
simple Ieee802154ManagementApp extends SimpleModule like IIeee802154MacServiceClient, IIeee802154ManagementServiceClient
{
    parameters:
        string serviceModule = default("^.nic.mac");
        bool coordinator = default(false);
        bool beaconOperation = default(false); // Explicit opt-in to a BO3/SO1 managed PAN.
        bool gtsOperation = default(false); // Opt in to a bidirectional GTS exchange.
        bool enhancedData = default(false); // Select unacknowledged version-2 DATA.
        bool enhancedSequenceSuppressed = default(false);
        bool enhancedPanIdSuppressed = default(false);
        int gtsBatchSize = default(2); // Reopen permit once after each batch of grants.
        double gtsBatchDelay @unit(s) = default(0.5s);
        int panId = default(4660);
        int channelNumber = default(20);
        int cycles = default(2);
        @signal[completedCycle](type=long);
        @class(Ieee802154ManagementApp);
        @display("i=block/app");
}
