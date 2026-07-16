//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxChannelModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>

#include "inet/common/math/Functions.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IDimensionalSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/signal/ChannelSnapshot.h"

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

std::pair<double, double> nextNormalPair(uint64_t& state)
{
    auto radius = std::sqrt(-2 * std::log(nextUniform(state)));
    auto angle = 2 * M_PI * nextUniform(state);
    return {radius * std::cos(angle), radius * std::sin(angle)};
}

} // namespace

Define_Module(TgaxChannelModel);

bool TgaxChannelModel::LinkKey::operator<(const LinkKey& other) const
{
    return std::tie(transmitterRadioId, receiverRadioId) < std::tie(other.transmitterRadioId, other.receiverRadioId);
}

void TgaxChannelModel::initialize(int stage)
{
    Module::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        channelModel = par("channelModel").stdstringValue();
        systemBandwidth = Hz(par("systemBandwidth"));
        reciprocal = par("reciprocal");
        rngStream = par("rngStream");
        referenceFrequency = Hz(par("referenceFrequency"));
        frequencyResolution = Hz(par("frequencyResolution"));

        if (rngStream < 0)
            throw cRuntimeError("TGax channel model RNG stream must be non-negative");
        auto rng = getRNG(rngStream);
        realizationSeed = (uint64_t)rng->intRand() << 32 | rng->intRand();
        if (!std::isfinite(referenceFrequency.get<Hz>()) || referenceFrequency <= Hz(0))
            throw cRuntimeError("TGax channel model reference frequency must be finite and positive");
        if (!std::isfinite(frequencyResolution.get<Hz>()) || frequencyResolution <= Hz(0))
            throw cRuntimeError("TGax channel model frequency resolution must be finite and positive");
        profile.reset(new TgaxChannelProfile(TgaxChannelProfile::create(channelModel.c_str(), systemBandwidth)));
    }
}

TgaxChannelModel::LinkKey TgaxChannelModel::makeLinkKey(int transmitterRadioId, int receiverRadioId) const
{
    if (transmitterRadioId < 0 || receiverRadioId < 0)
        throw cRuntimeError("TGax channel model requires non-negative radio IDs");
    if (reciprocal && transmitterRadioId > receiverRadioId)
        std::swap(transmitterRadioId, receiverRadioId);
    return {transmitterRadioId, receiverRadioId};
}

std::shared_ptr<const TgaxSisoChannel> TgaxChannelModel::createChannel(const LinkKey& key) const
{
    if (profile == nullptr)
        throw cRuntimeError("TGax channel model is not initialized");
    std::vector<TgaxSisoChannel::Tap> taps;
    taps.reserve(profile->getComponents().size());
    auto state = realizationSeed ^ ((uint64_t)(uint32_t)key.transmitterRadioId << 32) ^ (uint32_t)key.receiverRadioId;
    state = nextSplitMix64(state);
    for (const auto& component : profile->getComponents()) {
        auto standardDeviation = std::sqrt(component.normalizedPower / 2);
        auto normalPair = nextNormalPair(state);
        std::complex<double> coefficient(standardDeviation * normalPair.first,
                standardDeviation * normalPair.second);
        taps.push_back({nsimsec(component.excessDelayNs), coefficient});
    }
    return std::make_shared<const TgaxSisoChannel>(std::move(taps));
}

const TgaxSisoChannel *TgaxChannelModel::getOrCreateChannel(int transmitterRadioId, int receiverRadioId) const
{
    auto key = makeLinkKey(transmitterRadioId, receiverRadioId);
    auto it = channels.find(key);
    if (it == channels.end())
        it = channels.emplace(key, createChannel(key)).first;
    return it->second.get();
}

Ptr<const IFunction<double, Domain<simsec, Hz>>> TgaxChannelModel::createPowerGain(const TgaxSisoChannel& channel,
        simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth) const
{
    if (endTime <= startTime)
        throw cRuntimeError("TGax channel model reception duration must be positive");
    if (!std::isfinite(centerFrequency.get<Hz>()) || centerFrequency <= Hz(0))
        throw cRuntimeError("TGax channel model transmission center frequency must be finite and positive");
    if (!std::isfinite(bandwidth.get<Hz>()) || bandwidth <= Hz(0))
        throw cRuntimeError("TGax channel model transmission bandwidth must be finite and positive");
    if (bandwidth > systemBandwidth)
        throw cRuntimeError("TGax channel model transmission bandwidth exceeds the configured system bandwidth");

    auto lowerFrequency = centerFrequency - bandwidth / 2;
    auto upperFrequency = centerFrequency + bandwidth / 2;
    if (lowerFrequency < Hz(0))
        throw cRuntimeError("TGax channel model transmission band must not contain negative frequencies");
    auto lowerIndexValue = std::floor(((lowerFrequency - referenceFrequency) / frequencyResolution).get<unit>());
    auto upperIndexValue = std::ceil(((upperFrequency - referenceFrequency) / frequencyResolution).get<unit>());
    auto maximumIndex = (double)std::numeric_limits<long long>::max();
    auto minimumIndex = (double)std::numeric_limits<long long>::min();
    if (!std::isfinite(lowerIndexValue) || !std::isfinite(upperIndexValue) ||
            lowerIndexValue <= minimumIndex || lowerIndexValue >= maximumIndex ||
            upperIndexValue <= minimumIndex || upperIndexValue >= maximumIndex)
        throw cRuntimeError("TGax channel model frequency grid index is outside the supported range");
    auto numberOfCells = std::max(1.0, upperIndexValue - lowerIndexValue);
    if (numberOfCells > 1000000)
        throw cRuntimeError("TGax channel model frequency grid has too many cells: %g", numberOfCells);
    auto lowerIndex = (long long)lowerIndexValue;
    auto upperIndex = (long long)upperIndexValue;
    if (upperIndex <= lowerIndex)
        upperIndex = lowerIndex + 1;

    std::map<Hz, double> frequencyGainDeltas;
    frequencyGainDeltas.emplace(getLowerBound<Hz>(), 0);
    for (auto index = lowerIndex; index < upperIndex; index++) {
        auto lowerCellFrequency = referenceFrequency + (double)index * frequencyResolution;
        auto cellCenterFrequency = lowerCellFrequency + frequencyResolution / 2;
        auto cellStartFrequency = std::max(lowerFrequency, lowerCellFrequency);
        if (cellStartFrequency < upperFrequency)
            frequencyGainDeltas.emplace(cellStartFrequency, channel.computePowerGain(cellCenterFrequency - referenceFrequency) - 1);
    }
    frequencyGainDeltas[upperFrequency] = 0;
    frequencyGainDeltas.emplace(getUpperBound<Hz>(), 0);

    auto frequencyDeltaFunction = makeShared<Interpolated1DFunction<double, Hz>>(frequencyGainDeltas,
            &LeftInterpolator<Hz, double>::singleton);
    auto timeWindowFunction = makeShared<Boxcar1DFunction<double, simsec>>(simsec(startTime), simsec(endTime), 1);
    auto timeFrequencyDeltaFunction = makeShared<Combined2DFunction<double, simsec, Hz>>(timeWindowFunction, frequencyDeltaFunction);
    auto identityFunction = makeShared<ConstantFunction<double, Domain<simsec, Hz>>>(1);
    return makeShared<AddedFunction<double, Domain<simsec, Hz>>>(identityFunction, timeFrequencyDeltaFunction);
}

std::ostream& TgaxChannelModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "TgaxChannelModel";
    if (level <= PRINT_LEVEL_DEBUG)
        stream << EV_FIELD(channelModel)
               << EV_FIELD(systemBandwidth)
               << EV_FIELD(reciprocal)
               << EV_FIELD(rngStream)
               << EV_FIELD(referenceFrequency)
               << EV_FIELD(frequencyResolution)
               << EV_FIELD(numChannels, channels.size());
    return stream;
}

Ptr<const IChannelSnapshot> TgaxChannelModel::computeChannel(const IRadio *receiver, const ITransmission *transmission,
        const IArrival *arrival) const
{
    if (receiver == nullptr || transmission == nullptr || arrival == nullptr)
        throw cRuntimeError("TGax channel model requires a receiver, transmission, and arrival");
    auto analogModel = transmission->getAnalogModel();
    auto narrowbandAnalogModel = dynamic_cast<const INarrowbandSignalAnalogModel *>(analogModel);
    auto dimensionalAnalogModel = dynamic_cast<const IDimensionalSignalAnalogModel *>(analogModel);
    if (narrowbandAnalogModel == nullptr || dimensionalAnalogModel == nullptr)
        throw cRuntimeError("TGax channel model requires a narrowband dimensional transmission");

    auto channel = getOrCreateChannel(transmission->getTransmitterRadioId(), receiver->getId());
    auto powerGain = createPowerGain(*channel, arrival->getStartTime(), arrival->getEndTime(),
            narrowbandAnalogModel->getCenterFrequency(), narrowbandAnalogModel->getBandwidth());
    return Ptr<const IChannelSnapshot>(new ChannelSnapshot(powerGain));
}

} // namespace physicallayer

} // namespace inet
