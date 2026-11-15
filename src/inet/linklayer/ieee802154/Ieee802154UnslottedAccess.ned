// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;
import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154AccessProvider;

simple Ieee802154UnslottedAccess extends SimpleModule like IIeee802154AccessProvider
{
    parameters:
        string clientModule = default("^.mac");
        int backoffRng = default(0);
        @class(Ieee802154UnslottedAccess);
        @display("i=block/timer");
        @lifecycleSupport;
        @signal[backoffSlots](type=long);
        @signal[backoffCount](type=long);
        @signal[backoffExponent](type=long);
}
