//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.channelmodel;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.IPathLoss;

//
// Median indoor path loss for TGax Model B and Model D scenarios. The model
// follows free-space propagation up to the model-specific breakpoint and a
// path-loss exponent of 3.5 after it. Wall loss, floor loss, and shadow fading
// are intentionally not included.
//
module TgaxIndoorPathLoss extends Module like IPathLoss
{
    parameters:
        string channelModel @enum("B", "D") = default("B");
        @class(TgaxIndoorPathLoss);
        @display("i=block/control");
}
