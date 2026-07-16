//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.channelmodel;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.IWidebandChannelModel;

//
// A cached reciprocal SISO NLOS realization of a TGax indoor channel profile.
// The frequency response is sampled on an absolute grid and is constant over
// the duration of each immutable channel snapshot.
//
module TgaxChannelModel extends Module like IWidebandChannelModel
{
    parameters:
        string channelModel @enum("A", "B", "C", "D", "E", "F") = default("B");
        double systemBandwidth @unit(Hz) = default(20MHz);
        bool reciprocal = default(true);
        int rngStream = default(0);
        double referenceFrequency @unit(Hz) = default(5GHz);
        double frequencyResolution @unit(Hz) = default(78.125kHz);
        @class(TgaxChannelModel);
        @display("i=block/tunnel");
}
