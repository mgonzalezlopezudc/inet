//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel;

//
// Opt-in dimensional IEEE 802.11 radio whose receiver eagerly materializes
// TGn channel matrices on IEEE OFDM resources.
//
module Ieee80211TgnRadio extends Ieee80211DimensionalRadio
{
    parameters:
        receiver.channelMatrixReceptionProcessor.typename = default("ChannelMatrixReceptionProcessor");
}
