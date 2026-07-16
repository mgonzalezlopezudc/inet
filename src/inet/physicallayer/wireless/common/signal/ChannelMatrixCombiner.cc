//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/signal/ChannelMatrixCombiner.h"

#include <cmath>

namespace inet {
namespace physicallayer {

double ChannelMatrixCombiner::computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix)
{
    if (channelMatrix.getNumTransmitAntennas() != 1)
        throw cRuntimeError("Single-stream MRC requires an explicit transmit precoder when multiple transmit antennas are present");
    return computeSingleStreamMrcPowerGain(channelMatrix, {{1, 0}});
}

double ChannelMatrixCombiner::computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix,
        const std::vector<std::complex<double>>& transmitWeights)
{
    if ((int)transmitWeights.size() != channelMatrix.getNumTransmitAntennas())
        throw cRuntimeError("Single-stream MRC transmit precoder size does not match the channel matrix");
    double weightNorm = 0;
    for (const auto& weight : transmitWeights) {
        if (!std::isfinite(weight.real()) || !std::isfinite(weight.imag()))
            throw cRuntimeError("Single-stream MRC transmit precoder must be finite");
        weightNorm += std::norm(weight);
    }
    if (std::abs(weightNorm - 1) > 1e-12)
        throw cRuntimeError("Single-stream MRC transmit precoder must have unit norm");
    double powerGain = 0;
    for (int receiveAntenna = 0; receiveAntenna < channelMatrix.getNumReceiveAntennas(); receiveAntenna++) {
        std::complex<double> effectiveCoefficient(0, 0);
        for (int transmitAntenna = 0; transmitAntenna < channelMatrix.getNumTransmitAntennas(); transmitAntenna++)
            effectiveCoefficient += channelMatrix.getCoefficient(receiveAntenna, transmitAntenna) *
                    transmitWeights[transmitAntenna];
        powerGain += std::norm(effectiveCoefficient);
    }
    return powerGain;
}

} // namespace physicallayer
} // namespace inet
