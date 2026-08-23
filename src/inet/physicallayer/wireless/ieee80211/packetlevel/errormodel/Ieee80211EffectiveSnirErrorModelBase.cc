//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211EffectiveSnirErrorModelBase.h"

#include <algorithm>
#include <cmath>

#include "inet/physicallayer/wireless/common/contract/packetlevel/IDimensionalSnir.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IPartitionedTransmissionSpectrum.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211OfdmSubcarrierPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211Eesm.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

namespace inet {
namespace physicallayer {

namespace {

static bool equalBand(const PowerSpectralBand& a, const PowerSpectralBand& b)
{
    const double tolerance = 1e-12;
    return std::abs((a.lowerFrequencyOffset - b.lowerFrequencyOffset).get()) <= tolerance &&
           std::abs((a.upperFrequencyOffset - b.upperFrequencyOffset).get()) <= tolerance &&
           std::abs(a.powerFraction - b.powerFraction) <= tolerance;
}

static void validateSingleTransmitReceiveChain(const ITransmission *transmission, const IReception *reception, const char *modelName, const char *chainDescription)
{
    const auto *transmitterRadio = transmission->getTransmitterRadio();
    if (transmitterRadio == nullptr)
        throw cRuntimeError("%s requires a non-null transmitter radio for the %s", modelName, chainDescription);
    const auto *receiverRadio = reception->getReceiverRadio();
    if (receiverRadio == nullptr)
        throw cRuntimeError("%s requires a non-null receiver radio for the %s", modelName, chainDescription);
    const auto *transmitterAntenna = transmitterRadio->getAntenna();
    if (transmitterAntenna == nullptr)
        throw cRuntimeError("%s requires a transmitter antenna for the %s", modelName, chainDescription);
    const auto *receiverAntenna = receiverRadio->getAntenna();
    if (receiverAntenna == nullptr)
        throw cRuntimeError("%s requires a receiver antenna for the %s", modelName, chainDescription);
    if (transmitterAntenna->getNumAntennas() != 1)
        throw cRuntimeError("%s supports one transmit antenna, got %d", modelName, transmitterAntenna->getNumAntennas());
    if (receiverAntenna->getNumAntennas() != 1)
        throw cRuntimeError("%s supports one receive antenna, got %d", modelName, receiverAntenna->getNumAntennas());
}

} // namespace

void Ieee80211EffectiveSnirErrorModelBase::initialize(int stage)
{
    ErrorModelBase::initialize(stage);
}

const IIeee80211HtDataMode *Ieee80211EffectiveSnirErrorModelBase::getHtDataMode(const ISnir *snir) const
{
    const char *modelName = getErrorModelName();
    const auto *transmission = check_and_cast<const Ieee80211Transmission *>(snir->getReception()->getTransmission());
    const auto *dataMode = dynamic_cast<const IIeee80211HtDataMode *>(transmission->getMode()->getDataMode());
    if (dataMode == nullptr)
        throw cRuntimeError("%s requires the HT data-mode carrier-plan capability", modelName);
    return dataMode;
}

void Ieee80211EffectiveSnirErrorModelBase::validateHtDataMode(const IIeee80211HtDataMode *dataMode) const
{
    const char *modelName = getErrorModelName();
    if (dataMode->getMcsIndex() > 7)
        throw cRuntimeError("%s supports only HT MCS 0 through 7", modelName);
    if (dataMode->getBandwidth() != MHz(20) && dataMode->getBandwidth() != MHz(40))
        throw cRuntimeError("%s supports only HT20 and HT40", modelName);
    if (dataMode->getNumberOfSpatialStreams() != 1)
        throw cRuntimeError("%s supports only SISO HT", modelName);
    if (!dataMode->isBcc())
        throw cRuntimeError("%s requires BCC coding", modelName);
}

void Ieee80211EffectiveSnirErrorModelBase::validateSpectrum(const ISnir *snir, const IIeee80211HtDataMode *dataMode) const
{
    const char *modelName = getErrorModelName();
    const auto *transmission = check_and_cast<const Ieee80211Transmission *>(snir->getReception()->getTransmission());
    validateSingleTransmitReceiveChain(transmission, snir->getReception(), modelName, getSisoChainDescription());
    const auto *analogModel = transmission->getAnalogModel();
    const auto *partitioned = dynamic_cast<const IPartitionedTransmissionSpectrum *>(analogModel);
    if (partitioned == nullptr || partitioned->getDataSpectrum() == nullptr)
        throw cRuntimeError("%s requires retained occupied-carrier Data spectrum metadata", modelName);
    const auto& expected = dataMode->getEqualPowerSpectrum().getBands();
    const auto& actual = partitioned->getDataSpectrum()->getBands();
    if (expected.size() != actual.size())
        throw cRuntimeError("%s: Data spectrum carrier count mismatch", modelName);
    for (size_t i = 0; i < expected.size(); i++)
        if (!equalBand(expected[i], actual[i]))
            throw cRuntimeError("%s: Data spectrum metadata does not match authoritative HT carrier plan at band %zu", modelName, i);
}

std::vector<double> Ieee80211EffectiveSnirErrorModelBase::collectDataSnir(const ISnir *snir, const IIeee80211HtDataMode *dataMode) const
{
    const char *modelName = getErrorModelName();
    const auto *dimensionalSnir = dynamic_cast<const IDimensionalSnir *>(snir);
    if (dimensionalSnir == nullptr)
        throw cRuntimeError("%s requires dimensional SNIR", modelName);
    validateHtDataMode(dataMode);
    validateSpectrum(snir, dataMode);
    const auto *receptionAnalog = dynamic_cast<const INarrowbandSignalAnalogModel *>(snir->getReception()->getAnalogModel());
    if (receptionAnalog == nullptr)
        throw cRuntimeError("%s requires dimensional reception-domain analog data", modelName);
    const auto snirFunction = dimensionalSnir->getSnir();
    const simtime_t dataStart = snir->getReception()->getDataStartTime();
    const simtime_t dataEnd = snir->getReception()->getDataEndTime();
    if (!(dataEnd > dataStart))
        throw cRuntimeError("%s requires a positive Data interval", modelName);
    const Hz centerFrequency = receptionAnalog->getCenterFrequency();
    const auto& carriers = dataMode->getSubcarriers();
    std::vector<double> values;
    values.reserve(dataMode->getNumberOfDataSubcarriers());
    for (const auto& carrier : carriers) {
        if (carrier.role != Ieee80211SubcarrierRole::DATA)
            continue;
        const Hz frequency = centerFrequency + carrier.centerFrequencyOffset;
        const math::Point<simsec, Hz> lower(simsec(dataStart), frequency);
        const math::Point<simsec, Hz> upper(simsec(dataEnd), frequency);
        const math::Interval<simsec, Hz> interior(lower, upper, 0b01, 0b01, 0b01);
        const double minimum = snirFunction->getMin(interior);
        const double maximum = snirFunction->getMax(interior);
        if (!std::isfinite(minimum) || !std::isfinite(maximum))
            throw cRuntimeError("%s: non-finite Data SNIR", modelName);
        if (Ieee80211Eesm::hasSignificantInteriorVariation(minimum, maximum))
            throw cRuntimeError("%s rejects SNIR variation strictly inside the Data interval", modelName);
        const math::Point<simsec, Hz> midpoint(simsec(dataStart + (dataEnd - dataStart) / 2), frequency);
        const double value = snirFunction->getValue(midpoint) * snirOffset;
        if (!std::isfinite(value) || value < 0)
            throw cRuntimeError("%s: invalid Data-carrier SNIR", modelName);
        values.push_back(value);
    }
    const int expectedDataCarriers = dataMode->getBandwidth() == MHz(20) ? 52 : 108;
    if (int(values.size()) != expectedDataCarriers)
        throw cRuntimeError("%s requires exactly %d Data carriers, got %zu", modelName, expectedDataCarriers, values.size());
    return values;
}

double Ieee80211EffectiveSnirErrorModelBase::computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const
{
    Enter_Method("computePacketErrorRate");
    const char *modelName = getErrorModelName();
    if (part != IRadioSignal::SIGNAL_PART_WHOLE)
        throw cRuntimeError("%s supports only WHOLE packet error probability", modelName);
    const auto *transmission = check_and_cast<const Ieee80211Transmission *>(snir->getReception()->getTransmission());
    const auto *dataMode = getHtDataMode(snir);
    const auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket());
    const uint64_t psduLengthBytes = phyHeader->getLengthField().get<B>();
    if (psduLengthBytes == 0)
        throw cRuntimeError("%s rejects zero-length HT NDP/PSDU", modelName);
    const auto values = collectDataSnir(snir, dataMode);
    const double effectiveSnrDb = computeEffectiveSnrDb(values, dataMode);
    const int bandwidthMHz = dataMode->getBandwidth() == MHz(20) ? 20 : 40;
    return perTable.getPacketErrorRate(bandwidthMHz, dataMode->getMcsIndex(), psduLengthBytes, effectiveSnrDb);
}

double Ieee80211EffectiveSnirErrorModelBase::computeBitErrorRate(const ISnir *, IRadioSignal::SignalPart)
const
{
    Enter_Method("computeBitErrorRate");
    return NaN;
}

double Ieee80211EffectiveSnirErrorModelBase::computeSymbolErrorRate(const ISnir *, IRadioSignal::SignalPart)
const
{
    Enter_Method("computeSymbolErrorRate");
    return NaN;
}

} // namespace physicallayer
} // namespace inet
