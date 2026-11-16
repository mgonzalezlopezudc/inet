// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;

import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154AccessProvider;

simple Ieee802154SlottedAccess extends SimpleModule like IIeee802154AccessProvider
{
    parameters:
        string clientModule = default("^.mac");
        string clockModule = default("");
        int backoffRng = default(0);
        @class(Ieee802154SlottedAccess);
        @display("i=block/timer");
        @lifecycleSupport;
        @signal[backoffSlots](type=long);
        @signal[backoffCount](type=long);
        @signal[backoffExponent](type=long);
        @signal[contentionWindow](type=long);
}
