//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.pathloss;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.IPathLoss;

//
// Breakpoint path loss from the nonnormative propagation model in
// IEEE 802.11-03/940r4, TGn Channel Models, May 10, 2004, Section 2, Table I.
//
module TgnIndoorPathLoss extends Module like IPathLoss
{
    parameters:
        string profile @enum("A", "B", "C", "D", "E", "F") = default("B");
        double referenceDistance @unit(m) = default(1m);
        @class(TgnIndoorPathLoss);
        @display("i=block/control");
}
