//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.channelmodel;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.IWidebandChannelModel;

//
// Time-varying wideband MIMO propagation based on the nonnormative model in
// IEEE 802.11-03/940r4, TGn Channel Models, May 10, 2004. The Rx-row/Tx-column
// matrix convention follows IEEE Std 802.11-2024, Clause 19.3.12.1.
//
module TgnChannelModel extends Module like IWidebandChannelModel
{
    parameters:
        string profile @enum("A", "B", "C", "D", "E", "F") = default("B");
        string condition @enum("nlos", "los") = default("nlos");
        bool reciprocal = default(false);
        bool timeVariation = default(true);
        bool vehicleEffect = default(true);
        bool fluorescentEffect = default(true);
        bool shadowing = default(true);
        bool ensembleNormalization = default(false);
        double fluorescentMainsFrequency @unit(Hz) = default(50Hz);
        double environmentalSpeed @unit(mps) = default(0.333333333333mps);
        double vehicleSpeed @unit(mps) = default(11.111111111111mps);
        double antennaSpacing = default(0.5);
        int oscillatorCount = default(256);
        int linkSeedRng = default(0);
        int shadowingRng = default(0);
        int diffuseRng = default(0);
        int fluorescentRng = default(0);
        @class(TgnChannelModel);
        @display("i=block/wheelbarrow");
}
