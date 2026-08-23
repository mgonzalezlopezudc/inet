//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel;

import inet.physicallayer.wireless.ieee80211.channelmodel.TgnChannelModel;
import inet.physicallayer.wireless.ieee80211.pathloss.TgnIndoorPathLoss;

//
// Opt-in dimensional IEEE 802.11 medium with a time-varying TGn A-F MIMO
// channel. Transmitters attach their spatial plans and receivers select a
// configured combining or spatial-stream-detection strategy.
//
module Ieee80211TgnRadioMedium extends Ieee80211DimensionalRadioMedium
{
    parameters:
        string profile @enum("A", "B", "C", "D", "E", "F") = default("B");
        string condition @enum("nlos", "los") = default("nlos");
        bool reciprocal = default(false);
        bool timeVariation = default(true);
        bool vehicleEffect = default(true);
        bool fluorescentEffect = default(true);
        double fluorescentMainsFrequency @unit(Hz) = default(50Hz);
        double environmentalSpeed @unit(mps) = default(0.333333333333mps);
        double vehicleSpeed @unit(mps) = default(11.111111111111mps);
        double antennaSpacing = default(0.5);
        int oscillatorCount = default(256);
        double referenceDistance @unit(m) = default(1m);
        int linkSeedRng = default(0);
        int shadowingRng = default(0);
        int diffuseRng = default(0);
        int fluorescentRng = default(0);

        rangeFilter = default("");
        channelModel.typename = default("TgnChannelModel");
        pathLoss.typename = default("TgnIndoorPathLoss");

        channelModel.profile = this.profile;
        channelModel.condition = this.condition;
        channelModel.reciprocal = this.reciprocal;
        channelModel.timeVariation = this.timeVariation;
        channelModel.vehicleEffect = this.vehicleEffect;
        channelModel.fluorescentEffect = this.fluorescentEffect;
        channelModel.fluorescentMainsFrequency = this.fluorescentMainsFrequency;
        channelModel.environmentalSpeed = this.environmentalSpeed;
        channelModel.vehicleSpeed = this.vehicleSpeed;
        channelModel.antennaSpacing = this.antennaSpacing;
        channelModel.oscillatorCount = this.oscillatorCount;
        channelModel.linkSeedRng = this.linkSeedRng;
        channelModel.shadowingRng = this.shadowingRng;
        channelModel.diffuseRng = this.diffuseRng;
        channelModel.fluorescentRng = this.fluorescentRng;

        pathLoss.profile = this.profile;
        pathLoss.referenceDistance = this.referenceDistance;
}
