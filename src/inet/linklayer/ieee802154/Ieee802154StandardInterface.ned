// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;

import inet.networklayer.common.NetworkInterface;
import inet.linklayer.ieee802154.contract.IIeee802154MacServiceProvider;
import inet.linklayer.contract.IWirelessInterface;
import inet.physicallayer.wireless.ieee802154.Ieee802154PhyService;
import inet.physicallayer.wireless.ieee802154.packetlevel.Ieee802154StandardRadio;

// Native direct-data interface with optional radio/access composition.
module Ieee802154StandardInterface extends NetworkInterface like IWirelessInterface
{
    parameters:
        string interfaceTableModule = default("");
        string extendedAddress = default(ieee802154LocalAddress());
        int shortAddress = default(65535);
        int panId = default(65535);
        string upperProtocol = default("");
        bool enableRadio = default(true);
        bool enableManagement = default(false);
        bool enableSuperframe = default(false); // Requires radio and management.
        string clockModule = default(""); // Empty selects simulation time.
        bool rxOnWhenIdle = default(false);
        double ccaDuration @unit(s) = default(128us);
        @class(NetworkInterface);
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input radioIn @labels(IWirelessSignal);
    submodules:
        adapter: Ieee802154MacServiceAdapter {
            serviceModule = "^.mac";
            upperProtocol = parent.upperProtocol;
        }
        mac: <default("Ieee802154StandardMac")> like IIeee802154MacServiceProvider {
            clientModule = default("^.adapter");
            extendedAddress = parent.extendedAddress;
            shortAddress = parent.shortAddress;
            panId = parent.panId;
            accessModule = parent.enableRadio ? "^.access" : "";
            slottedAccessModule = parent.enableSuperframe ? "^.slottedAccess" : "";
            providerModule = parent.enableRadio ? "^.phy" : "";
            superframeModule = parent.enableSuperframe ? "^.superframeController" : "";
            rxOnWhenIdle = parent.rxOnWhenIdle;
            initialCcaDuration = parent.ccaDuration;
            enableManagement = parent.enableManagement;
        }
        access: Ieee802154UnslottedAccess if enableRadio {
            clientModule = "^.mac";
        }
        slottedAccess: Ieee802154SlottedAccess if enableSuperframe {
            clientModule = "^.mac";
            clockModule = parent.clockModule;
        }
        superframeController: Ieee802154SuperframeController if enableSuperframe {
            clientModule = "^.mac";
            clockModule = parent.clockModule;
        }
        phy: Ieee802154PhyService if enableRadio {
            clientModule = "^.mac";
            radioModule = "^.radio";
            ccaDuration = parent.ccaDuration;
        }
        radio: Ieee802154StandardRadio if enableRadio;
    connections allowunconnected:
        upperLayerIn --> adapter.upperLayerIn;
        adapter.upperLayerOut --> upperLayerOut;
        if enableRadio {
            radioIn --> radio.radioIn;
            radio.upperLayerOut --> phy.lowerLayerIn;
        }
}
