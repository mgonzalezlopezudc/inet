//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixResponse.h"

#include <cmath>
#include <utility>

namespace inet {
namespace physicallayer {

ChannelMatrix::ChannelMatrix(int numReceiveAntennas, int numTransmitAntennas,
        std::vector<std::complex<double>> coefficients) :
    numReceiveAntennas(numReceiveAntennas),
    numTransmitAntennas(numTransmitAntennas),
    coefficients(std::move(coefficients))
{
    if (numReceiveAntennas <= 0 || numTransmitAntennas <= 0)
        throw cRuntimeError("Channel matrix dimensions must be positive");
    if (this->coefficients.size() != (size_t)numReceiveAntennas * numTransmitAntennas)
        throw cRuntimeError("Channel matrix coefficient count does not match its dimensions");
    for (const auto& coefficient : this->coefficients)
        if (!std::isfinite(coefficient.real()) || !std::isfinite(coefficient.imag()))
            throw cRuntimeError("Channel matrix coefficients must be finite");
}

const std::complex<double>& ChannelMatrix::getCoefficient(int receiveAntenna, int transmitAntenna) const
{
    if (receiveAntenna < 0 || receiveAntenna >= numReceiveAntennas ||
            transmitAntenna < 0 || transmitAntenna >= numTransmitAntennas)
        throw cRuntimeError("Channel matrix antenna index is out of range");
    return coefficients[receiveAntenna * numTransmitAntennas + transmitAntenna];
}

ChannelMatrix ChannelMatrix::transposed() const
{
    std::vector<std::complex<double>> transposedCoefficients(coefficients.size());
    for (int receiveAntenna = 0; receiveAntenna < numReceiveAntennas; receiveAntenna++)
        for (int transmitAntenna = 0; transmitAntenna < numTransmitAntennas; transmitAntenna++)
            transposedCoefficients[transmitAntenna * numReceiveAntennas + receiveAntenna] =
                    getCoefficient(receiveAntenna, transmitAntenna);
    return ChannelMatrix(numTransmitAntennas, numReceiveAntennas, std::move(transposedCoefficients));
}

} // namespace physicallayer
} // namespace inet
