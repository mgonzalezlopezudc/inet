//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalMediumAnalogModel.h"

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/MultibandFunction.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSnir.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/MultibandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Reception.h"
#include "inet/physicallayer/wireless/common/signal/PowerFunctions.h"

namespace inet {

namespace physicallayer {

Define_Module(DimensionalMediumAnalogModel);

void DimensionalMediumAnalogModel::initialize(int stage)
{
    AnalogModelBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        attenuateWithCenterFrequency = par("attenuateWithCenterFrequency"); // TODO rename center
}

std::ostream& DimensionalMediumAnalogModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "DimensionalMediumAnalogModel";
    if (level <= PRINT_LEVEL_DEBUG)
        stream << EV_FIELD(attenuateWithCenterFrequency);
    return stream;
}

const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> DimensionalMediumAnalogModel::computeReceptionPower(const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival) const
{
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
    const auto& occupiedBands = analogModel->getOccupiedBands();
    if (occupiedBands.size() == 1 && attenuateWithCenterFrequency) {
        const auto& constantAttenuationFunction = makeShared<ConstantFunction<double, Domain<simsec, Hz>>>(attenuationFunction->getValue(Point<simsec, Hz>(simsec(0), analogModel->getCenterFrequency())));
        receptionPower = propagatedTransmissionPowerFunction->multiply(constantAttenuationFunction);
    }
    else if (occupiedBands.size() == 1) {
        Hz lower = analogModel->getCenterFrequency() - analogModel->getBandwidth() / 2;
        Hz upper = analogModel->getCenterFrequency() + analogModel->getBandwidth() / 2;
        Hz step = analogModel->getBandwidth() / 10; // TODO: parameter for 10
        const auto& approximatedAttenuationFunction = makeShared<ApproximatedFunction<double, Domain<simsec, Hz>, 1, Hz>>(lower, upper, step, &AverageInterpolator<Hz, double>::singleton, attenuationFunction);
        receptionPower = propagatedTransmissionPowerFunction->multiply(approximatedAttenuationFunction);
    }
    else {
        std::vector<Ptr<const IFunction<double, Domain<simsec, Hz>>>> attenuationComponents;
        attenuationComponents.reserve(occupiedBands.size());
        for (const auto& band : occupiedBands) {
            if (attenuateWithCenterFrequency)
                attenuationComponents.push_back(makeShared<ConstantFunction<double, Domain<simsec, Hz>>>(attenuationFunction->getValue(Point<simsec, Hz>(simsec(0), band.centerFrequency))));
            else {
                Hz step = band.bandwidth / 10; // TODO: parameter for 10
                attenuationComponents.push_back(makeShared<ApproximatedFunction<double, Domain<simsec, Hz>, 1, Hz>>(band.getLowerFrequency(), band.getUpperFrequency(), step, &AverageInterpolator<Hz, double>::singleton, attenuationFunction));
            }
        }
        auto multibandAttenuation = makeShared<MultibandFunction<double>>(occupiedBands, attenuationComponents);
        receptionPower = propagatedTransmissionPowerFunction->multiply(multibandAttenuation);
    }
    EV_TRACE << "Reception power begin " << endl;
    EV_TRACE << *receptionPower << endl;
    EV_TRACE << "Reception power end" << endl;
    return receptionPower;
}

const INoise *DimensionalMediumAnalogModel::computeNoise(const IListening *listening, const IInterference *interference) const
{
    const auto multibandListening = dynamic_cast<const MultibandListening *>(listening);
    const BandListening *bandListening = multibandListening == nullptr ? check_and_cast<const BandListening *>(listening) : nullptr;
    Hz centerFrequency = bandListening != nullptr ? bandListening->getCenterFrequency() : multibandListening->getCenterFrequency();
    Hz bandwidth = bandListening != nullptr ? bandListening->getBandwidth() : multibandListening->getBandwidth();
    std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> receptionPowers;
    const DimensionalNoise *dimensionalBackgroundNoise = check_and_cast_nullable<const DimensionalNoise *>(interference->getBackgroundNoise());
    if (dimensionalBackgroundNoise) {
        const auto& backgroundNoisePower = dimensionalBackgroundNoise->getPower();
        receptionPowers.push_back(backgroundNoisePower);
    }
    const std::vector<const IReception *> *interferingReceptions = interference->getInterferingReceptions();
    for (const auto & interferingReception : *interferingReceptions) {
        auto dimensionalSignal = check_and_cast<const DimensionalReceptionAnalogModel *>(interferingReception->getAnalogModel());
        auto receptionPower = dimensionalSignal->getPower();
        receptionPowers.push_back(receptionPower);
        EV_TRACE << "Interference power begin " << endl;
        EV_TRACE << *receptionPower << endl;
        EV_TRACE << "Interference power end" << endl;
    }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& noisePower = makeShared<SummedFunction<WpHz, Domain<simsec, Hz>>>(receptionPowers);
    EV_TRACE << "Noise power begin " << endl;
    EV_TRACE << *noisePower << endl;
    EV_TRACE << "Noise power end" << endl;
    if (multibandListening == nullptr) {
        const auto& bandpassFilter = makeShared<Boxcar2DFunction<double, simsec, Hz>>(simsec(listening->getStartTime()), simsec(listening->getEndTime()), centerFrequency - bandwidth / 2, centerFrequency + bandwidth / 2, 1);
        return new DimensionalNoise(listening->getStartTime(), listening->getEndTime(), centerFrequency, bandwidth, noisePower->multiply(bandpassFilter));
    }
    std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> noiseComponents(multibandListening->getOccupiedBands().size(), noisePower);
    auto maskedNoisePower = makeShared<MultibandFunction<WpHz>>(multibandListening->getOccupiedBands(), noiseComponents);
    return new DimensionalNoise(listening->getStartTime(), listening->getEndTime(), multibandListening->getOccupiedBands(), maskedNoisePower);
}

const INoise *DimensionalMediumAnalogModel::computeNoise(const IReception *reception, const INoise *noise) const
{
    auto dimensionalReception = check_and_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
    auto dimensionalNoise = check_and_cast<const DimensionalNoise *>(noise);
    if (dimensionalReception->getOccupiedBands().size() == 1) {
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& noisePower = makeShared<AddedFunction<WpHz, Domain<simsec, Hz>>>(dimensionalReception->getPower(), dimensionalNoise->getPower());
        return new DimensionalNoise(reception->getStartTime(), reception->getEndTime(), dimensionalReception->getCenterFrequency(), dimensionalReception->getBandwidth(), noisePower);
    }
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> backgroundPower = dimensionalNoise->getPower();
    if (dimensionalNoise->getOccupiedBands().size() == 1) {
        std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> backgroundComponents(dimensionalReception->getOccupiedBands().size(), backgroundPower);
        backgroundPower = makeShared<MultibandFunction<WpHz>>(dimensionalReception->getOccupiedBands(), backgroundComponents);
    }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& noisePower = makeShared<AddedFunction<WpHz, Domain<simsec, Hz>>>(dimensionalReception->getPower(), backgroundPower);
    return new DimensionalNoise(reception->getStartTime(), reception->getEndTime(), dimensionalReception->getOccupiedBands(), noisePower);
}

const ISnir *DimensionalMediumAnalogModel::computeSNIR(const IReception *reception, const INoise *noise) const
{
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
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& receptionPower = computeReceptionPower(receiverRadio, transmission, arrival);
    const auto& occupiedBands = transmissionAnalogModel->getOccupiedBands();
    DimensionalReceptionAnalogModel *receptionAnalogModel;
    if (occupiedBands.size() == 1)
        receptionAnalogModel = new DimensionalReceptionAnalogModel(transmissionAnalogModel->getPreambleDuration(), transmissionAnalogModel->getHeaderDuration(), transmissionAnalogModel->getDataDuration(), transmissionAnalogModel->getCenterFrequency(), transmissionAnalogModel->getBandwidth(), receptionPower);
    else
        receptionAnalogModel = new DimensionalReceptionAnalogModel(transmissionAnalogModel->getPreambleDuration(), transmissionAnalogModel->getHeaderDuration(), transmissionAnalogModel->getDataDuration(), occupiedBands, receptionPower);
    return new Reception(receiverRadio, transmission, receptionStartTime, receptionEndTime, receptionStartPosition, receptionEndPosition, receptionStartOrientation, receptionEndOrientation, receptionAnalogModel);
}

} // namespace physicallayer

} // namespace inet
