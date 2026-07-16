//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/signal/ChannelMatrixSignal.h"

#include <cmath>
#include <utility>

#include "inet/physicallayer/wireless/common/signal/ChannelMatrixCombiner.h"

namespace inet {
namespace physicallayer {

ChannelMatrixSignal::ChannelMatrixSignal(
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& inputPower,
        const std::shared_ptr<const IChannelMatrixResponse>& channelMatrixResponse,
        std::vector<std::complex<double>> transmitWeights) :
    inputPower(inputPower),
    channelMatrixResponse(channelMatrixResponse),
    transmitWeights(std::move(transmitWeights))
{
    if (inputPower == nullptr)
        throw cRuntimeError("Channel matrix signal input power must not be null");
    if (channelMatrixResponse == nullptr)
        throw cRuntimeError("Channel matrix signal response must not be null");
    if ((int)this->transmitWeights.size() != channelMatrixResponse->getNumTransmitAntennas())
        throw cRuntimeError("Channel matrix signal transmit weight count does not match the response");
    double weightNorm = 0;
    for (const auto& weight : this->transmitWeights) {
        if (!std::isfinite(weight.real()) || !std::isfinite(weight.imag()))
            throw cRuntimeError("Channel matrix signal transmit weights must be finite");
        weightNorm += std::norm(weight);
    }
    if (std::abs(weightNorm - 1) > 1e-12)
        throw cRuntimeError("Channel matrix signal transmit weights must have unit norm");
}

std::vector<std::complex<double>> ChannelMatrixSignal::computeEffectiveChannel(simsec time, Hz frequency) const
{
    auto channelMatrix = channelMatrixResponse->getChannelMatrix(time, frequency);
    if (channelMatrix.getNumReceiveAntennas() != channelMatrixResponse->getNumReceiveAntennas() ||
            channelMatrix.getNumTransmitAntennas() != channelMatrixResponse->getNumTransmitAntennas())
        throw cRuntimeError("Channel matrix signal response dimensions changed during evaluation");
    return ChannelMatrixCombiner::computeEffectiveChannel(channelMatrix, transmitWeights);
}

std::shared_ptr<const ChannelMatrixSignal> ChannelMatrixSignal::withInputPower(
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& inputPower) const
{
    return std::make_shared<const ChannelMatrixSignal>(inputPower, channelMatrixResponse, transmitWeights);
}

} // namespace physicallayer
} // namespace inet
