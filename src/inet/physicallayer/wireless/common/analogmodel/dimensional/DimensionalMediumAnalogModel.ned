//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.physicallayer.wireless.common.analogmodel.dimensional;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.IMediumAnalogModel;

//
// This analog model computes with dimensional analog power representation. In
// other words, the signal power may change over time and/or frequency.
//
module DimensionalMediumAnalogModel extends Module like IMediumAnalogModel
{
    parameters:
        bool attenuateWithCenterFrequency = default(true); // True means frequency selectivity will always use the carrier frequency as opposed to using the frequency where the change in power occurs
        bool enableChannelMatrixMrc = default(false); // Enables selected-transmit-antenna, single-stream MRC for matrix channel snapshots
        string channelMatrixInterferenceMode @enum("reject", "lmmse") = default("reject"); // Controls overlapping interference for channel-matrix receptions; lmmse requires enableChannelMatrixMrc
        int channelMatrixTransmitAntenna = default(0); // Explicit one-hot transmit precoder for the selected antenna
        double channelMatrixTimeResolution @unit(s) = default(10us); // Piecewise-constant effective L-MMSE SINR grid
        double channelMatrixFrequencyResolution @unit(Hz) = default(78.125kHz); // Shared MRC/L-MMSE frequency grid
        @display("i=block/tunnel");
        @class(DimensionalMediumAnalogModel);
}
