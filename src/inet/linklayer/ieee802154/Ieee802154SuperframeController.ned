// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.linklayer.ieee802154;

import inet.common.SimpleModule;
import inet.linklayer.ieee802154.contract.IIeee802154SuperframeProvider;

simple Ieee802154SuperframeController extends SimpleModule like IIeee802154SuperframeProvider
{
    parameters:
        string clientModule = default("^.mac");
        string clockModule = default("");
        @class(Ieee802154SuperframeController);
        @display("i=block/timer");
        @lifecycleSupport;
}
