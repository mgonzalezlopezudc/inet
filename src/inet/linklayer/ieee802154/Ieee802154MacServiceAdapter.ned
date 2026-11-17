// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;

import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154MacServiceClient;

// Native tagged gate facade for the MCPS service. Empty upperProtocol keeps received MSDUs opaque.
simple Ieee802154MacServiceAdapter extends SimpleModule like IIeee802154MacServiceClient
{
    parameters:
        string serviceModule = default("^.mac");
        string upperProtocol = default("");
        @class(Ieee802154MacServiceAdapter);
        @display("i=block/encap");
    gates:
        input upperLayerIn;
        output upperLayerOut;
}
