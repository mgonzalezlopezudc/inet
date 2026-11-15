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
        int panId = default(4660);
        int channelNumber = default(20);
        int cycles = default(2);
        @class(Ieee802154ManagementApp);
        @display("i=block/app");
}
