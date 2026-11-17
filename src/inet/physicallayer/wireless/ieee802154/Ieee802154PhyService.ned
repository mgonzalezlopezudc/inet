// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.physicallayer.wireless.ieee802154;
import inet.common.SimpleModule;
import inet.physicallayer.wireless.ieee802154.contract.IIeee802154PhyServiceProvider;

simple Ieee802154PhyService extends SimpleModule like IIeee802154PhyServiceProvider
{
    parameters:
        string clientModule = default("^.mac");
        string radioModule = default("^.radio");
        int channelNumber = default(20);
        double ccaEdThreshold @unit(W) = default(-85dBm);
        double ccaDuration @unit(s) = default(128us);
        @class(Ieee802154PhyService);
        @display("i=block/wrxtx");
        @namespace(inet::physicallayer);
        @lifecycleSupport;
    gates:
        input lowerLayerIn;
}
