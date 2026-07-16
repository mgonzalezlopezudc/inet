//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.channelmodel;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.IWidebandChannelModel;

//
// A cached reciprocal NLOS realization of a TGax indoor channel profile. The
// default is a SISO response; Models B and D optionally provide static matrix
// responses for one or two antennas per endpoint. The frequency response is
// sampled on an absolute grid and is constant over each immutable snapshot.
//
module TgaxChannelModel extends Module like IWidebandChannelModel
{
    parameters:
        string channelModel @enum("A", "B", "C", "D", "E", "F") = default("B");
        double systemBandwidth @unit(Hz) = default(20MHz);
        bool reciprocal = default(true);
        int rngStream = default(0);
        // The reference frequency must match the carrier when timeVariation is enabled.
        double referenceFrequency @unit(Hz) = default(5GHz);
        double frequencyResolution @unit(Hz) = default(78.125kHz);
        // IEEE 802.11-09/0308r12 stationary-user ambient Doppler. Mobile
        // indoor Doppler is explicitly TBD in that source.
        bool timeVariation = default(false);
        double environmentalSpeed @unit(mps) = default(0.0247222mps); // 0.089 km/h
        int numDopplerOscillators = default(32); // finite spectral approximation per tap
        double timeResolution @unit(s) = default(1ms); // piecewise-constant snapshot grid
        bool spatialChannel = default(false); // Opt-in static B/D NLOS matrix response, with receive rows and transmit columns
        @class(TgaxChannelModel);
        @display("i=block/tunnel");
}
