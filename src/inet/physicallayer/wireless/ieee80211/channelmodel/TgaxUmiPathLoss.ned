//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.channelmodel;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.IPathLoss;

//
// Median ITU-R Urban Micro (UMi) path loss selected by IEEE 802.11ax for
// outdoor evaluation. LOS selection, shadow fading, and penetration loss are
// intentionally outside this link-level model.
//
module TgaxUmiPathLoss extends Module like IPathLoss
{
    parameters:
        string propagationCondition @enum("LOS", "NLOS") = default("NLOS");
        double baseStationHeight @unit(m) = default(10m);
        double userTerminalHeight @unit(m) = default(1.5m);
        bool enforceApplicabilityRange = default(true);
        @class(TgaxUmiPathLoss);
        @display("i=block/control");
}
