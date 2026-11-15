// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.examples.wireless.ieee802154managed;

import inet.applications.ieee802154.Ieee802154ManagementApp;
import inet.linklayer.ieee802154.Ieee802154StandardInterface;
import inet.mobility.static.StationaryMobility;
import inet.physicallayer.wireless.common.medium.RadioMedium;

module ManagedNode
{
    parameters:
        bool coordinator = default(false);
        string extendedAddress;
        @networkNode;
    submodules:
        mobility: StationaryMobility;
        nic: Ieee802154StandardInterface {
            extendedAddress = parent.extendedAddress;
            enableManagement = true;
            rxOnWhenIdle = parent.coordinator;
            shortAddress = parent.coordinator ? 1 : 65535;
            mac.isPanCoordinator = parent.coordinator;
            mac.clientModule = "^.^.app";
            mac.managementClientModule = "^.^.app";
        }
        app: Ieee802154ManagementApp {
            coordinator = parent.coordinator;
            serviceModule = "^.nic.mac";
        }
    connections allowunconnected:
}

network ManagedPan
{
    submodules:
        radioMedium: RadioMedium;
        coordinator: ManagedNode {
            coordinator = true;
            extendedAddress = "extended:02-01-12-34-56-78-9A-BC";
        }
        device: ManagedNode {
            extendedAddress = "extended:04-01-12-34-56-78-9A-BC";
        }
}
