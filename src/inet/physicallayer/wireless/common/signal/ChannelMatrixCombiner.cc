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
#include "inet/physicallayer/wireless/common/signal/ChannelMatrixSignal.h"

namespace inet {
namespace physicallayer {

std::vector<std::complex<double>> ChannelMatrixCombiner::computeEffectiveChannel(
        const ChannelMatrix& channelMatrix, const std::vector<std::complex<double>>& transmitWeights)
{
    if ((int)transmitWeights.size() != channelMatrix.getNumTransmitAntennas())
        throw cRuntimeError("Single-stream transmit precoder size does not match the channel matrix");
    double weightNorm = 0;
    for (const auto& weight : transmitWeights) {
        if (!std::isfinite(weight.real()) || !std::isfinite(weight.imag()))
            throw cRuntimeError("Single-stream transmit precoder must be finite");
        weightNorm += std::norm(weight);
    }
    if (std::abs(weightNorm - 1) > 1e-12)
        throw cRuntimeError("Single-stream transmit precoder must have unit norm");
    std::vector<std::complex<double>> effectiveChannel(channelMatrix.getNumReceiveAntennas(), {0, 0});
    for (int receiveAntenna = 0; receiveAntenna < channelMatrix.getNumReceiveAntennas(); receiveAntenna++)
        for (int transmitAntenna = 0; transmitAntenna < channelMatrix.getNumTransmitAntennas(); transmitAntenna++)
            effectiveChannel[receiveAntenna] += channelMatrix.getCoefficient(receiveAntenna, transmitAntenna) *
                    transmitWeights[transmitAntenna];
    return effectiveChannel;
}

double ChannelMatrixCombiner::computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix)
{
    if (channelMatrix.getNumTransmitAntennas() != 1)
        throw cRuntimeError("Single-stream MRC requires an explicit transmit precoder when multiple transmit antennas are present");
    return computeSingleStreamMrcPowerGain(channelMatrix, {{1, 0}});
}

double ChannelMatrixCombiner::computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix,
        const std::vector<std::complex<double>>& transmitWeights)
{
    auto effectiveChannel = computeEffectiveChannel(channelMatrix, transmitWeights);
    double powerGain = 0;
    for (const auto& coefficient : effectiveChannel)
        powerGain += std::norm(coefficient);
    return powerGain;
}

double ChannelMatrixCombiner::computeSingleStreamLmmseSinr(
        const std::vector<std::complex<double>>& desiredChannel, double desiredPower,
        const std::vector<std::vector<std::complex<double>>>& interferingChannels,
        const std::vector<double>& interferingPowers, double noisePower)
{
    auto numReceiveAntennas = desiredChannel.size();
    if (numReceiveAntennas == 0)
        throw cRuntimeError("L-MMSE desired channel must contain at least one receive antenna");
    if (!std::isfinite(desiredPower) || desiredPower < 0)
        throw cRuntimeError("L-MMSE desired power must be finite and non-negative");
    if (!std::isfinite(noisePower) || noisePower <= 0)
        throw cRuntimeError("L-MMSE per-antenna noise power must be finite and positive");
    if (interferingChannels.size() != interferingPowers.size())
        throw cRuntimeError("L-MMSE interfering channel and power counts differ");
    for (const auto& coefficient : desiredChannel)
        if (!std::isfinite(coefficient.real()) || !std::isfinite(coefficient.imag()))
            throw cRuntimeError("L-MMSE desired channel must be finite");

    std::vector<std::complex<double>> covariance(numReceiveAntennas * numReceiveAntennas, {0, 0});
    for (size_t row = 0; row < numReceiveAntennas; row++)
        covariance[row * numReceiveAntennas + row] = noisePower;
    for (size_t i = 0; i < interferingChannels.size(); i++) {
        const auto& channel = interferingChannels[i];
        auto power = interferingPowers[i];
        if (channel.size() != numReceiveAntennas)
            throw cRuntimeError("L-MMSE interfering channel receive dimension differs from desired channel");
        if (!std::isfinite(power) || power < 0)
            throw cRuntimeError("L-MMSE interfering power must be finite and non-negative");
        for (const auto& coefficient : channel)
            if (!std::isfinite(coefficient.real()) || !std::isfinite(coefficient.imag()))
                throw cRuntimeError("L-MMSE interfering channel must be finite");
        for (size_t row = 0; row < numReceiveAntennas; row++)
            for (size_t column = 0; column < numReceiveAntennas; column++)
                covariance[row * numReceiveAntennas + column] +=
                        power * channel[row] * std::conj(channel[column]);
    }

    // Cholesky factorization R = L L^H. Positive per-antenna noise makes R
    // Hermitian positive definite even when interferer channels are dependent.
    std::vector<std::complex<double>> lower(numReceiveAntennas * numReceiveAntennas, {0, 0});
    for (size_t row = 0; row < numReceiveAntennas; row++) {
        for (size_t column = 0; column <= row; column++) {
            auto value = covariance[row * numReceiveAntennas + column];
            for (size_t k = 0; k < column; k++)
                value -= lower[row * numReceiveAntennas + k] *
                        std::conj(lower[column * numReceiveAntennas + k]);
            if (row == column) {
                auto tolerance = 1e-12 * std::max(1.0, std::abs(covariance[row * numReceiveAntennas + row]));
                if (std::abs(value.imag()) > tolerance || value.real() <= 0)
                    throw cRuntimeError("L-MMSE interference covariance is not positive definite");
                lower[row * numReceiveAntennas + column] = std::sqrt(value.real());
            }
            else
                lower[row * numReceiveAntennas + column] = value /
                        lower[column * numReceiveAntennas + column];
        }
    }

    std::vector<std::complex<double>> forward(numReceiveAntennas);
    for (size_t row = 0; row < numReceiveAntennas; row++) {
        auto value = desiredChannel[row];
        for (size_t column = 0; column < row; column++)
            value -= lower[row * numReceiveAntennas + column] * forward[column];
        forward[row] = value / lower[row * numReceiveAntennas + row];
    }
    std::vector<std::complex<double>> solution(numReceiveAntennas);
    for (size_t reverseRow = numReceiveAntennas; reverseRow-- > 0;) {
        auto value = forward[reverseRow];
        for (size_t column = reverseRow + 1; column < numReceiveAntennas; column++)
            value -= std::conj(lower[column * numReceiveAntennas + reverseRow]) * solution[column];
        solution[reverseRow] = value / std::conj(lower[reverseRow * numReceiveAntennas + reverseRow]);
    }
    std::complex<double> quadraticForm(0, 0);
    for (size_t i = 0; i < numReceiveAntennas; i++)
        quadraticForm += std::conj(desiredChannel[i]) * solution[i];
    auto tolerance = 1e-12 * std::max(1.0, std::abs(quadraticForm));
    if (!std::isfinite(quadraticForm.real()) || !std::isfinite(quadraticForm.imag()) ||
            std::abs(quadraticForm.imag()) > tolerance || quadraticForm.real() < -tolerance)
        throw cRuntimeError("L-MMSE SINR quadratic form is not finite non-negative real");
    return desiredPower * std::max(0.0, quadraticForm.real());
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

Ptr<const IFunction<double, Domain<simsec, Hz>>> ChannelMatrixCombiner::createStaticSingleStreamLmmseSnir(
        const std::shared_ptr<const ChannelMatrixSignal>& desiredSignal,
        const std::vector<std::shared_ptr<const ChannelMatrixSignal>>& interferingSignals,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& backgroundNoisePower,
        simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth,
        simtime_t timeResolution, Hz frequencyResolution)
{
    if (desiredSignal == nullptr)
        throw cRuntimeError("L-MMSE desired channel matrix signal must not be null");
    if (backgroundNoisePower == nullptr)
        throw cRuntimeError("L-MMSE background noise power must not be null");
    if (!desiredSignal->getChannelMatrixResponse()->isTimeInvariant())
        throw cRuntimeError("Static L-MMSE requires a time-invariant desired channel matrix response");
    auto numReceiveAntennas = desiredSignal->getNumReceiveAntennas();
    for (const auto& interferingSignal : interferingSignals) {
        if (interferingSignal == nullptr)
            throw cRuntimeError("L-MMSE interfering channel matrix signal must not be null");
        if (!interferingSignal->getChannelMatrixResponse()->isTimeInvariant())
            throw cRuntimeError("Static L-MMSE requires time-invariant interfering channel matrix responses");
        if (interferingSignal->getNumReceiveAntennas() != numReceiveAntennas)
            throw cRuntimeError("L-MMSE interfering channel receive dimension differs from desired channel");
    }
    auto timeStep = simsec(timeResolution);
    if (endTime <= startTime)
        throw cRuntimeError("L-MMSE reception duration must be positive");
    if (!std::isfinite(timeStep.get<s>().dbl()) || timeStep <= simsec(0) ||
            !std::isfinite(centerFrequency.get<Hz>()) || centerFrequency <= Hz(0) ||
            !std::isfinite(bandwidth.get<Hz>()) || bandwidth <= Hz(0) ||
            !std::isfinite(frequencyResolution.get<Hz>()) || frequencyResolution <= Hz(0))
        throw cRuntimeError("L-MMSE time and frequency grid parameters must be finite and positive");
    auto lowerFrequency = centerFrequency - bandwidth / 2;
    auto upperFrequency = centerFrequency + bandwidth / 2;
    if (lowerFrequency < Hz(0))
        throw cRuntimeError("L-MMSE signal band must not contain negative frequencies");

    auto lowerTimeIndexValue = std::floor((simsec(startTime) / timeStep).get<unit>());
    auto upperTimeIndexValue = std::ceil((simsec(endTime) / timeStep).get<unit>());
    auto lowerFrequencyIndexValue = std::floor((lowerFrequency / frequencyResolution).get<unit>());
    auto upperFrequencyIndexValue = std::ceil((upperFrequency / frequencyResolution).get<unit>());
    if (upperTimeIndexValue <= lowerTimeIndexValue)
        upperTimeIndexValue = lowerTimeIndexValue + 1;
    if (upperFrequencyIndexValue <= lowerFrequencyIndexValue)
        upperFrequencyIndexValue = lowerFrequencyIndexValue + 1;
    auto numTimeCells = upperTimeIndexValue - lowerTimeIndexValue;
    auto numFrequencyCells = upperFrequencyIndexValue - lowerFrequencyIndexValue;
    if (!std::isfinite(numTimeCells) || !std::isfinite(numFrequencyCells) ||
            numTimeCells > std::numeric_limits<int>::max() - 1 ||
            numFrequencyCells > std::numeric_limits<int>::max() - 1 ||
            numTimeCells * numFrequencyCells > 2000000)
        throw cRuntimeError("L-MMSE time-frequency grid has too many cells: %g",
                numTimeCells * numFrequencyCells);

    int sizeTime = (int)numTimeCells + 1;
    int sizeFrequency = (int)numFrequencyCells + 1;
    auto lowerGridTime = simsec(timeStep.get<s>() * lowerTimeIndexValue);
    auto upperGridTime = simsec(timeStep.get<s>() * upperTimeIndexValue);
    auto lowerGridFrequency = lowerFrequencyIndexValue * frequencyResolution;
    auto upperGridFrequency = upperFrequencyIndexValue * frequencyResolution;
    std::vector<double> sinrs(sizeTime * sizeFrequency);
    std::vector<std::vector<std::complex<double>>> interferingChannels(interferingSignals.size());
    std::vector<double> interferingPowers(interferingSignals.size());
    for (int frequencyIndex = 0; frequencyIndex < sizeFrequency - 1; frequencyIndex++) {
        auto cellLowerFrequency = lowerGridFrequency + frequencyResolution * frequencyIndex;
        auto cellUpperFrequency = cellLowerFrequency + frequencyResolution;
        auto sampleFrequency = (std::max(cellLowerFrequency, lowerFrequency) +
                std::min(cellUpperFrequency, upperFrequency)) / 2;
        auto desiredChannel = desiredSignal->computeEffectiveChannel(simsec(startTime), sampleFrequency);
        for (size_t i = 0; i < interferingSignals.size(); i++)
            interferingChannels[i] = interferingSignals[i]->computeEffectiveChannel(simsec(startTime), sampleFrequency);
        for (int timeIndex = 0; timeIndex < sizeTime - 1; timeIndex++) {
            auto cellLowerTime = lowerGridTime + timeStep * timeIndex;
            auto cellUpperTime = cellLowerTime + timeStep;
            auto sampleTime = (std::max(cellLowerTime, simsec(startTime)) +
                    std::min(cellUpperTime, simsec(endTime))) / 2;
            Point<simsec, Hz> samplePoint(sampleTime, sampleFrequency);
            auto desiredPower = desiredSignal->getInputPower()->getValue(samplePoint).get<WpHz>();
            auto noisePower = backgroundNoisePower->getValue(samplePoint).get<WpHz>();
            for (size_t i = 0; i < interferingSignals.size(); i++)
                interferingPowers[i] = interferingSignals[i]->getInputPower()->getValue(samplePoint).get<WpHz>();
            sinrs[frequencyIndex * sizeTime + timeIndex] = computeSingleStreamLmmseSinr(
                    desiredChannel, desiredPower, interferingChannels, interferingPowers, noisePower);
        }
    }
    for (int frequencyIndex = 0; frequencyIndex < sizeFrequency - 1; frequencyIndex++)
        sinrs[frequencyIndex * sizeTime + sizeTime - 1] =
                sinrs[frequencyIndex * sizeTime + sizeTime - 2];
    for (int timeIndex = 0; timeIndex < sizeTime; timeIndex++)
        sinrs[(sizeFrequency - 1) * sizeTime + timeIndex] =
                sinrs[(sizeFrequency - 2) * sizeTime + timeIndex];
    auto gridFunction = makeShared<PeriodicallyInterpolated2DFunction<double, simsec, Hz>>(
            lowerGridTime, upperGridTime, sizeTime, lowerGridFrequency, upperGridFrequency, sizeFrequency,
            LeftInterpolator<simsec, double>::singleton, LeftInterpolator<Hz, double>::singleton, sinrs);
    auto receptionWindow = makeShared<Boxcar2DFunction<double, simsec, Hz>>(
            simsec(startTime), simsec(endTime), lowerFrequency, upperFrequency, 1);
    return gridFunction->multiply(receptionWindow);
}

} // namespace physicallayer
} // namespace inet
