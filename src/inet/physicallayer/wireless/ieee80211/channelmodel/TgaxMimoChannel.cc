//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxMimoChannel.h"

#include <cmath>
#include <map>
#include <mutex>
#include <utility>

namespace inet {
namespace physicallayer {

namespace {

uint64_t nextSplitMix64(uint64_t& state)
{
    auto value = (state += 0x9E3779B97F4A7C15ULL);
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

double nextUniform(uint64_t& state)
{
    return ((nextSplitMix64(state) >> 11) + 0.5) * 0x1.0p-53;
}

std::complex<double> nextUnitComplexNormal(uint64_t& state)
{
    auto radius = std::sqrt(-std::log(nextUniform(state)));
    auto angle = 2 * M_PI * nextUniform(state);
    return std::polar(radius, angle);
}

double wrapAngle(double angle)
{
    return std::remainder(angle, 2 * M_PI);
}

std::vector<std::complex<double>> createCorrelationSquareRoot(int size, std::complex<double> correlation)
{
    if (size == 1)
        return {{1, 0}};
    if (size != 2)
        throw cRuntimeError("TGax MIMO channel currently supports only one or two antennas per endpoint");
    auto magnitude = std::abs(correlation);
    if (magnitude > 1 + 1e-12)
        throw cRuntimeError("TGax spatial correlation magnitude exceeds one");
    magnitude = std::min(1.0, magnitude);
    auto diagonal = (std::sqrt(1 + magnitude) + std::sqrt(1 - magnitude)) / 2;
    auto offDiagonalMagnitude = (std::sqrt(1 + magnitude) - std::sqrt(1 - magnitude)) / 2;
    auto phase = magnitude == 0 ? std::complex<double>(1, 0) : correlation / magnitude;
    return {{diagonal, 0}, offDiagonalMagnitude * phase,
            offDiagonalMagnitude * std::conj(phase), {diagonal, 0}};
}

const TgaxChannelProfile::Cluster& findCluster(const TgaxChannelProfile& profile, int clusterIndex)
{
    for (const auto& cluster : profile.getClusters())
        if (cluster.clusterIndex == clusterIndex)
            return cluster;
    throw cRuntimeError("Missing TGax spatial metadata for cluster %d", clusterIndex);
}

} // namespace

TgaxMimoChannel::TgaxMimoChannel(Hz referenceFrequency, int numReceiveAntennas, int numTransmitAntennas,
        std::vector<Tap> taps) :
    referenceFrequency(referenceFrequency),
    numReceiveAntennas(numReceiveAntennas),
    numTransmitAntennas(numTransmitAntennas),
    taps(std::move(taps))
{
    if (!std::isfinite(referenceFrequency.get<Hz>()) || referenceFrequency <= Hz(0))
        throw cRuntimeError("TGax MIMO reference frequency must be finite and positive");
    if (numReceiveAntennas < 1 || numReceiveAntennas > 2 || numTransmitAntennas < 1 || numTransmitAntennas > 2)
        throw cRuntimeError("TGax MIMO channel currently supports one or two antennas per endpoint");
    if (this->taps.empty())
        throw cRuntimeError("TGax MIMO channel must contain at least one tap");
    for (const auto& tap : this->taps) {
        if (!std::isfinite(tap.excessDelay.get<s>().dbl()) || tap.excessDelay < simsec(0))
            throw cRuntimeError("TGax MIMO tap delay must be finite and non-negative");
        if (tap.coefficient.getNumReceiveAntennas() != numReceiveAntennas ||
                tap.coefficient.getNumTransmitAntennas() != numTransmitAntennas)
            throw cRuntimeError("TGax MIMO tap dimensions do not match the channel dimensions");
    }
}

std::complex<double> TgaxMimoChannel::computeHalfWavelengthUlaCorrelation(double meanAngleDegrees,
        double angularSpreadDegrees)
{
    if (!std::isfinite(meanAngleDegrees) || !std::isfinite(angularSpreadDegrees) || angularSpreadDegrees <= 0)
        throw cRuntimeError("TGax cluster angle must be finite and angular spread must be finite and positive");
    static std::map<std::pair<double, double>, std::complex<double>> correlationCache;
    static std::mutex correlationCacheMutex;
    std::lock_guard<std::mutex> lock(correlationCacheMutex);
    auto key = std::make_pair(meanAngleDegrees, angularSpreadDegrees);
    auto cached = correlationCache.find(key);
    if (cached != correlationCache.end())
        return cached->second;
    auto meanAngle = meanAngleDegrees * M_PI / 180;
    auto angularSpread = angularSpreadDegrees * M_PI / 180;
    constexpr int numIntervals = 16384;
    double normalization = 0;
    std::complex<double> correlation(0, 0);
    for (int i = 0; i < numIntervals; i++) {
        auto angle = -M_PI + (i + 0.5) * 2 * M_PI / numIntervals;
        auto offset = wrapAngle(angle - meanAngle);
        auto weight = std::exp(-std::sqrt(2.0) * std::abs(offset) / angularSpread);
        normalization += weight;
        correlation += weight * std::exp(std::complex<double>(0, M_PI * std::sin(angle)));
    }
    auto result = correlation / normalization;
    correlationCache.emplace(key, result);
    return result;
}

std::shared_ptr<const TgaxMimoChannel> TgaxMimoChannel::create(const TgaxChannelProfile& profile,
        Hz referenceFrequency, int numReceiveAntennas, int numTransmitAntennas, uint64_t seed)
{
    if (!profile.hasSpatialMetadata())
        throw cRuntimeError("TGax spatial channel metadata is available only for the selected B/D profiles");
    if (numReceiveAntennas < 1 || numReceiveAntennas > 2 || numTransmitAntennas < 1 || numTransmitAntennas > 2)
        throw cRuntimeError("TGax MIMO channel currently supports one or two antennas per endpoint");
    auto state = seed;
    state = nextSplitMix64(state);
    std::vector<Tap> taps;
    taps.reserve(profile.getComponents().size());
    for (const auto& component : profile.getComponents()) {
        const auto& cluster = findCluster(profile, component.clusterIndex);
        auto receiveCorrelation = computeHalfWavelengthUlaCorrelation(cluster.angleOfArrivalDegrees,
                cluster.receiverAngularSpreadDegrees);
        auto transmitCorrelation = computeHalfWavelengthUlaCorrelation(cluster.angleOfDepartureDegrees,
                cluster.transmitterAngularSpreadDegrees);
        auto receiveSquareRoot = createCorrelationSquareRoot(numReceiveAntennas, receiveCorrelation);
        auto transmitSquareRoot = createCorrelationSquareRoot(numTransmitAntennas, transmitCorrelation);
        std::vector<std::complex<double>> iid(numReceiveAntennas * numTransmitAntennas);
        for (auto& coefficient : iid)
            coefficient = nextUnitComplexNormal(state);
        std::vector<std::complex<double>> correlated(numReceiveAntennas * numTransmitAntennas, {0, 0});
        for (int receiveAntenna = 0; receiveAntenna < numReceiveAntennas; receiveAntenna++)
            for (int transmitAntenna = 0; transmitAntenna < numTransmitAntennas; transmitAntenna++)
                for (int receiveIndex = 0; receiveIndex < numReceiveAntennas; receiveIndex++)
                    for (int transmitIndex = 0; transmitIndex < numTransmitAntennas; transmitIndex++)
                        correlated[receiveAntenna * numTransmitAntennas + transmitAntenna] +=
                                receiveSquareRoot[receiveAntenna * numReceiveAntennas + receiveIndex] *
                                iid[receiveIndex * numTransmitAntennas + transmitIndex] *
                                transmitSquareRoot[transmitAntenna * numTransmitAntennas + transmitIndex];
        auto amplitude = std::sqrt(component.normalizedPower);
        for (auto& coefficient : correlated)
            coefficient *= amplitude;
        taps.push_back({nsimsec(component.excessDelayNs),
                ChannelMatrix(numReceiveAntennas, numTransmitAntennas, std::move(correlated))});
    }
    return std::make_shared<const TgaxMimoChannel>(referenceFrequency, numReceiveAntennas,
            numTransmitAntennas, std::move(taps));
}

std::shared_ptr<const TgaxMimoChannel> TgaxMimoChannel::transposed() const
{
    std::vector<Tap> transposedTaps;
    transposedTaps.reserve(taps.size());
    for (const auto& tap : taps)
        transposedTaps.push_back({tap.excessDelay, tap.coefficient.transposed()});
    return std::make_shared<const TgaxMimoChannel>(referenceFrequency, numTransmitAntennas,
            numReceiveAntennas, std::move(transposedTaps));
}

ChannelMatrix TgaxMimoChannel::getChannelMatrix(simsec time, Hz frequency) const
{
    if (!std::isfinite(time.get<s>().dbl()) || !std::isfinite(frequency.get<Hz>()) || frequency <= Hz(0))
        throw cRuntimeError("TGax MIMO query time must be finite and frequency must be finite and positive");
    auto frequencyOffset = frequency - referenceFrequency;
    std::vector<std::complex<double>> response(numReceiveAntennas * numTransmitAntennas, {0, 0});
    for (const auto& tap : taps) {
        auto phase = std::exp(std::complex<double>(0,
                -2 * M_PI * frequencyOffset.get<Hz>() * tap.excessDelay.get<s>().dbl()));
        for (size_t i = 0; i < response.size(); i++)
            response[i] += tap.coefficient.getCoefficients()[i] * phase;
    }
    return ChannelMatrix(numReceiveAntennas, numTransmitAntennas, std::move(response));
}

} // namespace physicallayer
} // namespace inet
