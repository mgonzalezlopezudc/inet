//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/signal/ChannelMatrixCombiner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

#include "inet/common/math/Functions.h"

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

Ptr<const IFunction<double, Domain<simsec, Hz>>> ChannelMatrixCombiner::createStaticSingleStreamMrcPowerGain(
        const std::shared_ptr<const IChannelMatrixResponse>& channelMatrixResponse,
        int selectedTransmitAntenna, simtime_t startTime, simtime_t endTime,
        Hz centerFrequency, Hz bandwidth, Hz frequencyResolution)
{
    if (channelMatrixResponse == nullptr)
        throw cRuntimeError("Single-stream MRC channel matrix response must not be null");
    if (!channelMatrixResponse->isTimeInvariant())
        throw cRuntimeError("Static single-stream MRC requires a time-invariant channel matrix response");
    if (selectedTransmitAntenna < 0 || selectedTransmitAntenna >= channelMatrixResponse->getNumTransmitAntennas())
        throw cRuntimeError("Selected single-stream MRC transmit antenna is out of range");
    if (endTime <= startTime)
        throw cRuntimeError("SIMO MRC reception duration must be positive");
    if (!std::isfinite(centerFrequency.get<Hz>()) || centerFrequency <= Hz(0) ||
            !std::isfinite(bandwidth.get<Hz>()) || bandwidth <= Hz(0) ||
            !std::isfinite(frequencyResolution.get<Hz>()) || frequencyResolution <= Hz(0))
        throw cRuntimeError("SIMO MRC frequencies and resolution must be finite and positive");
    auto lowerFrequency = centerFrequency - bandwidth / 2;
    auto upperFrequency = centerFrequency + bandwidth / 2;
    if (lowerFrequency < Hz(0))
        throw cRuntimeError("SIMO MRC signal band must not contain negative frequencies");
    auto lowerIndexValue = std::floor((lowerFrequency / frequencyResolution).get<unit>());
    auto upperIndexValue = std::ceil((upperFrequency / frequencyResolution).get<unit>());
    auto maximumIndex = (double)std::numeric_limits<long long>::max();
    auto minimumIndex = (double)std::numeric_limits<long long>::min();
    if (!std::isfinite(lowerIndexValue) || !std::isfinite(upperIndexValue) ||
            lowerIndexValue <= minimumIndex || lowerIndexValue >= maximumIndex ||
            upperIndexValue <= minimumIndex || upperIndexValue >= maximumIndex ||
            upperIndexValue - lowerIndexValue > 1000000)
        throw cRuntimeError("SIMO MRC frequency grid is outside the supported range");
    auto lowerIndex = (long long)lowerIndexValue;
    auto upperIndex = (long long)upperIndexValue;
    if (upperIndex <= lowerIndex)
        upperIndex = lowerIndex + 1;
    std::map<Hz, double> gainDeltas;
    gainDeltas.emplace(getLowerBound<Hz>(), 0);
    auto sampleTime = simsec((startTime + endTime) / 2);
    std::vector<std::complex<double>> transmitWeights(channelMatrixResponse->getNumTransmitAntennas(), {0, 0});
    transmitWeights[selectedTransmitAntenna] = {1, 0};
    for (auto index = lowerIndex; index < upperIndex; index++) {
        auto cellLowerFrequency = (double)index * frequencyResolution;
        auto cellStartFrequency = std::max(lowerFrequency, cellLowerFrequency);
        if (cellStartFrequency < upperFrequency) {
            auto cellCenterFrequency = cellLowerFrequency + frequencyResolution / 2;
            auto matrix = channelMatrixResponse->getChannelMatrix(sampleTime, cellCenterFrequency);
            if (matrix.getNumReceiveAntennas() != channelMatrixResponse->getNumReceiveAntennas() ||
                    matrix.getNumTransmitAntennas() != channelMatrixResponse->getNumTransmitAntennas())
                throw cRuntimeError("SIMO MRC response dimensions changed during sampling");
            gainDeltas.emplace(cellStartFrequency,
                    computeSingleStreamMrcPowerGain(matrix, transmitWeights) - 1);
        }
    }
    gainDeltas[upperFrequency] = 0;
    gainDeltas.emplace(getUpperBound<Hz>(), 0);
    auto frequencyDeltaFunction = makeShared<Interpolated1DFunction<double, Hz>>(gainDeltas,
            &LeftInterpolator<Hz, double>::singleton);
    auto timeWindowFunction = makeShared<Boxcar1DFunction<double, simsec>>(simsec(startTime), simsec(endTime), 1);
    auto timeFrequencyDeltaFunction = makeShared<Combined2DFunction<double, simsec, Hz>>(
            timeWindowFunction, frequencyDeltaFunction);
    auto identityFunction = makeShared<ConstantFunction<double, Domain<simsec, Hz>>>(1);
    return makeShared<AddedFunction<double, Domain<simsec, Hz>>>(identityFunction, timeFrequencyDeltaFunction);
}

} // namespace physicallayer
} // namespace inet
