//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalMediumAnalogModel.h"

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSnir.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Reception.h"
#include "inet/physicallayer/wireless/common/signal/PowerFunctions.h"
#include "inet/physicallayer/wireless/common/signal/ChannelMatrixCombiner.h"

namespace inet {

namespace physicallayer {

Define_Module(DimensionalMediumAnalogModel);

void DimensionalMediumAnalogModel::initialize(int stage)
{
    AnalogModelBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        attenuateWithCenterFrequency = par("attenuateWithCenterFrequency"); // TODO rename center
        enableChannelMatrixMrc = par("enableChannelMatrixMrc");
        enableChannelMatrixLmmse = par("channelMatrixInterferenceMode").stdstringValue() == "lmmse";
        channelMatrixTransmitAntenna = par("channelMatrixTransmitAntenna");
        channelMatrixTimeResolution = par("channelMatrixTimeResolution");
        channelMatrixFrequencyResolution = Hz(par("channelMatrixFrequencyResolution"));
        if (enableChannelMatrixLmmse && !enableChannelMatrixMrc)
            throw cRuntimeError("Channel matrix L-MMSE interference processing requires selected-antenna MRC to be enabled");
        if (!std::isfinite(channelMatrixTimeResolution.dbl()) || channelMatrixTimeResolution <= 0)
            throw cRuntimeError("Channel matrix time resolution must be finite and positive");
        if (!std::isfinite(channelMatrixFrequencyResolution.get<Hz>()) || channelMatrixFrequencyResolution <= Hz(0))
            throw cRuntimeError("Channel matrix frequency resolution must be finite and positive");
    }
}

std::ostream& DimensionalMediumAnalogModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "DimensionalMediumAnalogModel";
    if (level <= PRINT_LEVEL_DEBUG)
        stream << EV_FIELD(attenuateWithCenterFrequency)
               << EV_FIELD(enableChannelMatrixMrc)
               << EV_FIELD(enableChannelMatrixLmmse)
               << EV_FIELD(channelMatrixTransmitAntenna)
               << EV_FIELD(channelMatrixTimeResolution)
               << EV_FIELD(channelMatrixFrequencyResolution);
    return stream;
}

const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> DimensionalMediumAnalogModel::computeReceptionPower(
        const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival,
        bool *channelMatrixCombined, Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> *interferencePower,
        std::shared_ptr<const ChannelMatrixSignal> *channelMatrixSignal) const
{
    if (channelMatrixCombined != nullptr)
        *channelMatrixCombined = false;
    if (channelMatrixSignal != nullptr)
        *channelMatrixSignal = nullptr;
    const IRadioMedium *radioMedium = receiverRadio->getMedium();
    auto analogModel = check_and_cast<const DimensionalSignalAnalogModel *>(transmission->getAnalogModel());
    const Coord& transmissionStartPosition = transmission->getStartPosition();
    const Coord& receptionStartPosition = arrival->getStartPosition();
    double transmitterAntennaGain = computeAntennaGain(transmission->getTransmitterAntennaGain(), transmissionStartPosition, arrival->getStartPosition(), transmission->getStartOrientation());
    double receiverAntennaGain = computeAntennaGain(receiverRadio->getAntenna()->getGain().get(), arrival->getStartPosition(), transmissionStartPosition, arrival->getStartOrientation());
    const auto& transmissionPowerFunction = analogModel->getPower();
    EV_TRACE << "Transmission power begin " << endl;
    EV_TRACE << *transmissionPowerFunction << endl;
    EV_TRACE << "Transmission power end" << endl;
    Point<simsec, Hz> propagationShift(simsec(arrival->getStartTime() - transmission->getStartTime()), Hz(0));
    const auto& propagatedTransmissionPowerFunction = makeShared<DomainShiftedFunction<WpHz, Domain<simsec, Hz>>>(transmissionPowerFunction, propagationShift);
    Ptr<const IFunction<double, Domain<simsec, Hz>>> attenuationFunction = makeShared<FrequencyDependentAttenuationFunction>(radioMedium, transmitterAntennaGain, receiverAntennaGain, transmissionStartPosition, receptionStartPosition);
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> receptionPower;
    if (attenuateWithCenterFrequency) {
        const auto& constantAttenuationFunction = makeShared<ConstantFunction<double, Domain<simsec, Hz>>>(attenuationFunction->getValue(Point<simsec, Hz>(simsec(0), analogModel->getCenterFrequency())));
        receptionPower = propagatedTransmissionPowerFunction->multiply(constantAttenuationFunction);
    }
    else {
        Hz lower = analogModel->getCenterFrequency() - analogModel->getBandwidth() / 2;
        Hz upper = analogModel->getCenterFrequency() + analogModel->getBandwidth() / 2;
        Hz step = analogModel->getBandwidth() / 10; // TODO: parameter for 10
        const auto& approximatedAttenuationFunction = makeShared<ApproximatedFunction<double, Domain<simsec, Hz>, 1, Hz>>(lower, upper, step, &AverageInterpolator<Hz, double>::singleton, attenuationFunction);
        receptionPower = propagatedTransmissionPowerFunction->multiply(approximatedAttenuationFunction);
    }
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> scalarInterferencePower = receptionPower;
    if (auto widebandChannelModel = radioMedium->getWidebandChannelModel()) {
        auto channelSnapshot = widebandChannelModel->computeChannel(receiverRadio, transmission, arrival);
        if (auto channelMatrixSnapshot = dynamic_cast<const IChannelMatrixSnapshot *>(channelSnapshot.get())) {
            if (!enableChannelMatrixMrc)
                throw cRuntimeError("Received a channel matrix snapshot but selected-antenna MRC is not enabled in the dimensional medium analog model");
            auto transmitterRadio = transmission->getTransmitterRadio();
            if (transmitterRadio == nullptr)
                throw cRuntimeError("Channel matrix combining requires the transmitting radio");
            auto channelMatrixResponse = channelMatrixSnapshot->getChannelMatrixResponse();
            auto numTransmitAntennas = transmitterRadio->getAntenna()->getNumAntennas();
            auto numReceiveAntennas = receiverRadio->getAntenna()->getNumAntennas();
            if (channelMatrixResponse->getNumTransmitAntennas() != numTransmitAntennas ||
                    channelMatrixResponse->getNumReceiveAntennas() != numReceiveAntennas)
                throw cRuntimeError("Channel matrix dimensions (%d receive x %d transmit) do not match radio antennas (%d receive x %d transmit)",
                        channelMatrixResponse->getNumReceiveAntennas(), channelMatrixResponse->getNumTransmitAntennas(),
                        numReceiveAntennas, numTransmitAntennas);
            scalarInterferencePower = receptionPower->multiply(channelMatrixSnapshot->getPowerGain());
            std::vector<std::complex<double>> transmitWeights(numTransmitAntennas, {0, 0});
            if (channelMatrixTransmitAntenna < 0 || channelMatrixTransmitAntenna >= numTransmitAntennas)
                throw cRuntimeError("Selected channel matrix transmit antenna is out of range");
            transmitWeights[channelMatrixTransmitAntenna] = {1, 0};
            auto spatialSignal = std::make_shared<const ChannelMatrixSignal>(
                    receptionPower, channelMatrixResponse, std::move(transmitWeights));
            if (channelMatrixSignal != nullptr)
                *channelMatrixSignal = spatialSignal;
            auto mrcPowerGain = ChannelMatrixCombiner::createStaticSingleStreamMrcPowerGain(
                    channelMatrixResponse, channelMatrixTransmitAntenna,
                    arrival->getStartTime(), arrival->getEndTime(),
                    analogModel->getCenterFrequency(), analogModel->getBandwidth(), channelMatrixFrequencyResolution);
            receptionPower = receptionPower->multiply(mrcPowerGain);
            if (numReceiveAntennas == 1)
                scalarInterferencePower = receptionPower;
            if (channelMatrixCombined != nullptr)
                *channelMatrixCombined = numReceiveAntennas > 1;
        }
        else {
            receptionPower = receptionPower->multiply(channelSnapshot->getPowerGain());
            scalarInterferencePower = receptionPower;
        }
    }
    if (interferencePower != nullptr)
        *interferencePower = scalarInterferencePower;
    EV_TRACE << "Reception power begin " << endl;
    EV_TRACE << *receptionPower << endl;
    EV_TRACE << "Reception power end" << endl;
    return receptionPower;
}

const INoise *DimensionalMediumAnalogModel::computeNoise(const IListening *listening, const IInterference *interference) const
{
    const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
    Hz centerFrequency = bandListening->getCenterFrequency();
    Hz bandwidth = bandListening->getBandwidth();
    const auto& bandpassFilter = makeShared<Boxcar2DFunction<double, simsec, Hz>>(simsec(listening->getStartTime()),
            simsec(listening->getEndTime()), centerFrequency - bandwidth / 2, centerFrequency + bandwidth / 2, 1);
    std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> receptionPowers;
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> backgroundNoisePower;
    const DimensionalNoise *dimensionalBackgroundNoise = check_and_cast_nullable<const DimensionalNoise *>(interference->getBackgroundNoise());
    if (dimensionalBackgroundNoise) {
        receptionPowers.push_back(dimensionalBackgroundNoise->getPower());
        backgroundNoisePower = dimensionalBackgroundNoise->getPower()->multiply(bandpassFilter);
    }
    const std::vector<const IReception *> *interferingReceptions = interference->getInterferingReceptions();
    std::vector<std::shared_ptr<const ChannelMatrixSignal>> channelMatrixInterferers;
    bool containsNonChannelMatrixInterference = false;
    bool hasOverlappingReception = false;
    auto listeningLowerFrequency = centerFrequency - bandwidth / 2;
    auto listeningUpperFrequency = centerFrequency + bandwidth / 2;
    for (const auto & interferingReception : *interferingReceptions) {
        auto dimensionalSignal = check_and_cast<const DimensionalReceptionAnalogModel *>(interferingReception->getAnalogModel());
        auto receptionPower = dimensionalSignal->getInterferencePower();
        receptionPowers.push_back(receptionPower);
        auto receptionLowerFrequency = dimensionalSignal->getCenterFrequency() - dimensionalSignal->getBandwidth() / 2;
        auto receptionUpperFrequency = dimensionalSignal->getCenterFrequency() + dimensionalSignal->getBandwidth() / 2;
        auto overlaps = std::min(listeningUpperFrequency, receptionUpperFrequency) >
                std::max(listeningLowerFrequency, receptionLowerFrequency);
        hasOverlappingReception |= overlaps;
        if (overlaps) {
            if (dimensionalSignal->getChannelMatrixSignal() != nullptr)
                channelMatrixInterferers.push_back(dimensionalSignal->getChannelMatrixSignal()->withInputPower(
                        dimensionalSignal->getChannelMatrixSignal()->getInputPower()->multiply(bandpassFilter)));
            else
                containsNonChannelMatrixInterference = true;
        }
        EV_TRACE << "Interference power begin " << endl;
        EV_TRACE << *receptionPower << endl;
        EV_TRACE << "Interference power end" << endl;
    }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& noisePower = makeShared<SummedFunction<WpHz, Domain<simsec, Hz>>>(receptionPowers);
    EV_TRACE << "Noise power begin " << endl;
    EV_TRACE << *noisePower << endl;
    EV_TRACE << "Noise power end" << endl;
    return new DimensionalNoise(listening->getStartTime(), listening->getEndTime(), centerFrequency, bandwidth,
            noisePower->multiply(bandpassFilter), hasOverlappingReception, backgroundNoisePower,
            std::move(channelMatrixInterferers), containsNonChannelMatrixInterference);
}

const INoise *DimensionalMediumAnalogModel::computeNoise(const IReception *reception, const INoise *noise) const
{
    auto dimensionalReception = check_and_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
    auto dimensionalNoise = check_and_cast<const DimensionalNoise *>(noise);
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& noisePower = makeShared<AddedFunction<WpHz, Domain<simsec, Hz>>>(dimensionalReception->getPower(), dimensionalNoise->getPower());
    return new DimensionalNoise(reception->getStartTime(), reception->getEndTime(), dimensionalReception->getCenterFrequency(),
            dimensionalReception->getBandwidth(), noisePower, dimensionalNoise->hasInterferingReceptions(),
            dimensionalNoise->getBackgroundNoisePower(), dimensionalNoise->getChannelMatrixInterferers(),
            dimensionalNoise->hasNonChannelMatrixInterference());
}

const ISnir *DimensionalMediumAnalogModel::computeSNIR(const IReception *reception, const INoise *noise) const
{
    auto dimensionalReception = check_and_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
    auto dimensionalNoise = check_and_cast<const DimensionalNoise *>(noise);
    auto channelMatrixSignal = dimensionalReception->getChannelMatrixSignal();
    if (enableChannelMatrixLmmse && channelMatrixSignal != nullptr && dimensionalNoise->hasInterferingReceptions()) {
        if (dimensionalNoise->hasNonChannelMatrixInterference())
            throw cRuntimeError("L-MMSE channel matrix reception cannot combine interference without channel matrix metadata");
        if (dimensionalNoise->getChannelMatrixInterferers().empty())
            throw cRuntimeError("L-MMSE noise reports overlapping receptions but contains no channel matrix interferers");
        auto lmmseSnir = ChannelMatrixCombiner::createStaticSingleStreamLmmseSnir(
                channelMatrixSignal, dimensionalNoise->getChannelMatrixInterferers(),
                dimensionalNoise->getBackgroundNoisePower(), reception->getStartTime(), reception->getEndTime(),
                dimensionalReception->getCenterFrequency(), dimensionalReception->getBandwidth(),
                channelMatrixTimeResolution, channelMatrixFrequencyResolution);
        return new DimensionalSnir(reception, noise, lmmseSnir, true);
    }
    if (dimensionalReception->isChannelMatrixCombined() && dimensionalNoise->hasInterferingReceptions())
        throw cRuntimeError("Selected-antenna MRC with overlapping receptions requires receive-antenna interference covariance, which is not available in the dimensional analog pipeline");
    return new DimensionalSnir(reception, noise);
}

const IReception *DimensionalMediumAnalogModel::computeReception(const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival) const
{
    auto transmissionAnalogModel = check_and_cast<const DimensionalSignalAnalogModel *>(transmission->getAnalogModel());
    const simtime_t receptionStartTime = arrival->getStartTime();
    const simtime_t receptionEndTime = arrival->getEndTime();
    const Coord& receptionStartPosition = arrival->getStartPosition();
    const Coord& receptionEndPosition = arrival->getEndPosition();
    const Quaternion& receptionStartOrientation = arrival->getStartOrientation();
    const Quaternion& receptionEndOrientation = arrival->getEndOrientation();
    bool channelMatrixCombined = false;
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> interferencePower;
    std::shared_ptr<const ChannelMatrixSignal> channelMatrixSignal;
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& receptionPower = computeReceptionPower(
            receiverRadio, transmission, arrival, &channelMatrixCombined, &interferencePower, &channelMatrixSignal);
    auto receptionAnalogModel = new DimensionalReceptionAnalogModel(transmissionAnalogModel->getPreambleDuration(),
            transmissionAnalogModel->getHeaderDuration(), transmissionAnalogModel->getDataDuration(),
            transmissionAnalogModel->getCenterFrequency(), transmissionAnalogModel->getBandwidth(),
            receptionPower, interferencePower, channelMatrixCombined, channelMatrixSignal);
    return new Reception(receiverRadio, transmission, receptionStartTime, receptionEndTime, receptionStartPosition, receptionEndPosition, receptionStartOrientation, receptionEndOrientation, receptionAnalogModel);
}

} // namespace physicallayer

} // namespace inet
