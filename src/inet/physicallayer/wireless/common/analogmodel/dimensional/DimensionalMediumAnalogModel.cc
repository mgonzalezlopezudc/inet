//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalMediumAnalogModel.h"

#include <algorithm>
#include <cmath>

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixSnir.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixPhysicalPowerMaterializer.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixReceptionMaterializer.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSnir.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixReceiver.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISpatialTransmission.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Reception.h"
#include "inet/physicallayer/wireless/common/signal/PowerFunctions.h"

namespace inet {

namespace physicallayer {

Define_Module(DimensionalMediumAnalogModel);

namespace {

Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> createZeroPowerFunction()
{
    return makeShared<ConstantFunction<WpHz, Domain<simsec, Hz>>>(WpHz(0));
}

std::shared_ptr<const SpatialTransmissionPlan> getSpatialTransmissionPlan(
    const ITransmission *transmission, const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
    int selectedTransmitAntenna, simtime_t duration)
{
    if (const auto spatialTransmission = dynamic_cast<const ISpatialTransmission *>(transmission)) {
        const auto& plan = spatialTransmission->getSpatialTransmissionPlan();
        if (plan) {
            if (plan->getNumberOfTransmitAntennas() != snapshot->getNumTransmitAntennas())
                throw cRuntimeError("Attached spatial plan has %d transmit antennas instead of channel snapshot dimension %d",
                    plan->getNumberOfTransmitAntennas(), snapshot->getNumTransmitAntennas());
            plan->validateCompleteCoverage(duration);
            return plan;
        }
    }
    if (selectedTransmitAntenna < 0 || selectedTransmitAntenna >= snapshot->getNumTransmitAntennas())
        throw cRuntimeError("Selected transmit antenna %d is outside a %d-column channel snapshot",
            selectedTransmitAntenna, snapshot->getNumTransmitAntennas());
    ComplexMatrix mapping(snapshot->getNumTransmitAntennas(), 1);
    mapping.get(selectedTransmitAntenna, 0) = 1;
    SpatialTransmissionPlan::Segment segment(0, duration, 1, 1, mapping, {1.0});
    auto plan = std::make_shared<const SpatialTransmissionPlan>(snapshot->getNumTransmitAntennas(),
        std::vector<SpatialTransmissionPlan::Segment>{segment});
    plan->validateCompleteCoverage(duration);
    return plan;
}

} // namespace

void DimensionalMediumAnalogModel::initialize(int stage)
{
    AnalogModelBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        attenuateWithCenterFrequency = par("attenuateWithCenterFrequency"); // TODO rename center
        selectedTransmitAntenna = par("selectedTransmitAntenna");
        if (selectedTransmitAntenna < 0)
            throw cRuntimeError("Selected transmit antenna must be nonnegative");
    }
}

std::ostream& DimensionalMediumAnalogModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "DimensionalMediumAnalogModel";
    if (level <= PRINT_LEVEL_DEBUG)
        stream << EV_FIELD(attenuateWithCenterFrequency)
               << EV_FIELD(selectedTransmitAntenna);
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
    EV_TRACE << "Reception power begin " << endl;
    EV_TRACE << *receptionPower << endl;
    EV_TRACE << "Reception power end" << endl;
    return receptionPower;
}

const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> DimensionalMediumAnalogModel::computeCompatibilityNoisePower(
    const IReception *reception, const ChannelMatrixNoise *noise) const
{
    const auto desired = check_and_cast<const ChannelMatrixReceptionAnalogModel *>(reception->getAnalogModel());
    std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> powers;
    powers.push_back(noise->getCompatibilityBackgroundPower());
    for (const auto& interfering : noise->getInterferers()) {
        const bool overlapsTime = interfering.endTime > reception->getStartTime() &&
            interfering.startTime < reception->getEndTime();
        const bool overlapsFrequency = interfering.centerFrequency + interfering.bandwidth / 2 >
                desired->getCenterFrequency() - desired->getBandwidth() / 2 &&
            interfering.centerFrequency - interfering.bandwidth / 2 <
                desired->getCenterFrequency() + desired->getBandwidth() / 2;
        if (!overlapsTime || !overlapsFrequency)
            continue;
        if (desired->getSnapshot()->getNumReceiveAntennas() > 1)
            throw cRuntimeError("Overlapping matrix interference remains unsupported until a covariance-aware receiver strategy is attached");
        powers.push_back(interfering.physicalAggregatePower);
    }
    return makeShared<SummedFunction<WpHz, Domain<simsec, Hz>>>(powers);
}

const INoise *DimensionalMediumAnalogModel::computeNoise(const IListening *listening, const IInterference *interference) const
{
    if (listening->getReceiverRadio()->getMedium()->getChannelModel() != nullptr) {
        const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
        const Hz centerFrequency = bandListening->getCenterFrequency();
        const Hz bandwidth = bandListening->getBandwidth();
        const auto bandpassFilter = makeShared<Boxcar2DFunction<double, simsec, Hz>>(
            simsec(listening->getStartTime()), simsec(listening->getEndTime()),
            centerFrequency - bandwidth / 2, centerFrequency + bandwidth / 2, 1);
        Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> background = createZeroPowerFunction();
        const DimensionalNoise *dimensionalBackgroundNoise = check_and_cast_nullable<const DimensionalNoise *>(interference->getBackgroundNoise());
        if (dimensionalBackgroundNoise)
            background = dimensionalBackgroundNoise->getPower();
        background = background->multiply(bandpassFilter);
        std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> ccaPowers = {background};
        std::vector<ChannelMatrixNoise::Interferer> interferers;
        const auto matrixReceiver = dynamic_cast<const IChannelMatrixReceiver *>(
            listening->getReceiverRadio()->getReceiver());
        for (const IReception *interferingReception : *interference->getInterferingReceptions()) {
            const auto matrixSignal = check_and_cast<const ChannelMatrixReceptionAnalogModel *>(interferingReception->getAnalogModel());
            ccaPowers.push_back(matrixSignal->getCcaPower()->multiply(bandpassFilter));
            const auto resourceCells = matrixReceiver == nullptr ?
                std::vector<ChannelMatrixResourceCell>{} :
                matrixReceiver->getChannelMatrixResourceCells(*interferingReception);
            interferers.emplace_back(interferingReception->getTransmission()->getId(),
                interferingReception->getStartTime(), interferingReception->getEndTime(),
                matrixSignal->getCenterFrequency(), matrixSignal->getBandwidth(),
                matrixSignal->getSnapshot(), matrixSignal->getSpatialTransmissionPlan(),
                matrixSignal->getDeterministicLargeScalePower(), matrixSignal->getPower(), resourceCells);
        }
        const auto ccaAggregate = makeShared<SummedFunction<WpHz, Domain<simsec, Hz>>>(ccaPowers);
        return new ChannelMatrixNoise(listening->getStartTime(), listening->getEndTime(), centerFrequency, bandwidth,
            ccaAggregate, background, background, interferers);
    }
    const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
    Hz centerFrequency = bandListening->getCenterFrequency();
    Hz bandwidth = bandListening->getBandwidth();
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
    const auto& bandpassFilter = makeShared<Boxcar2DFunction<double, simsec, Hz>>(simsec(listening->getStartTime()), simsec(listening->getEndTime()), centerFrequency - bandwidth / 2, centerFrequency + bandwidth / 2, 1);
    return new DimensionalNoise(listening->getStartTime(), listening->getEndTime(), centerFrequency, bandwidth, noisePower->multiply(bandpassFilter));
}

const INoise *DimensionalMediumAnalogModel::computeNoise(const IReception *reception, const INoise *noise) const
{
    if (reception->getReceiverRadio()->getMedium()->getChannelModel() != nullptr) {
        const auto desired = check_and_cast<const ChannelMatrixReceptionAnalogModel *>(reception->getAnalogModel());
        const auto matrixNoise = check_and_cast<const ChannelMatrixNoise *>(noise);
        const auto matrixReceiver = dynamic_cast<const IChannelMatrixReceiver *>(
            reception->getReceiverRadio()->getReceiver());
        const auto compatibilityNoise = matrixReceiver != nullptr &&
            matrixReceiver->getChannelMatrixReceptionProcessor() != nullptr ?
            matrixNoise->getPower() : computeCompatibilityNoisePower(reception, matrixNoise);
        const auto signalPlusNoise = makeShared<AddedFunction<WpHz, Domain<simsec, Hz>>>(desired->getPower(), compatibilityNoise);
        return new DimensionalNoise(reception->getStartTime(), reception->getEndTime(),
            desired->getCenterFrequency(), desired->getBandwidth(), signalPlusNoise);
    }
    auto dimensionalReception = check_and_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
    auto dimensionalNoise = check_and_cast<const DimensionalNoise *>(noise);
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& noisePower = makeShared<AddedFunction<WpHz, Domain<simsec, Hz>>>(dimensionalReception->getPower(), dimensionalNoise->getPower());
    return new DimensionalNoise(reception->getStartTime(), reception->getEndTime(), dimensionalReception->getCenterFrequency(), dimensionalReception->getBandwidth(), noisePower);
}

const ISnir *DimensionalMediumAnalogModel::computeSNIR(const IReception *reception, const INoise *noise) const
{
    if (reception->getReceiverRadio()->getMedium()->getChannelModel() != nullptr) {
        const auto matrixNoise = check_and_cast<const ChannelMatrixNoise *>(noise);
        const auto matrixReceiver = dynamic_cast<const IChannelMatrixReceiver *>(
            reception->getReceiverRadio()->getReceiver());
        if (matrixReceiver != nullptr && matrixReceiver->getChannelMatrixReceptionProcessor() != nullptr) {
            const auto materialized = ChannelMatrixReceptionMaterializer::materialize(*reception,
                *matrixNoise, *matrixReceiver);
            return new ChannelMatrixSnir(reception, matrixNoise, materialized);
        }
        return new ChannelMatrixSnir(reception, matrixNoise, computeCompatibilityNoisePower(reception, matrixNoise));
    }
    return new DimensionalSnir(reception, noise);
}

const IReception *DimensionalMediumAnalogModel::computeReception(const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival) const
{
    if (receiverRadio->getMedium()->getChannelModel() != nullptr) {
        auto transmissionAnalogModel = check_and_cast<const DimensionalSignalAnalogModel *>(transmission->getAnalogModel());
        const auto snapshot = receiverRadio->getMedium()->getChannelModel()->computeChannel(receiverRadio, transmission, arrival);
        if (snapshot->getNumReceiveAntennas() != receiverRadio->getAntenna()->getNumAntennas() ||
            snapshot->getNumTransmitAntennas() != transmission->getTransmitterRadio()->getAntenna()->getNumAntennas())
            throw cRuntimeError("Channel snapshot dimensions %d x %d disagree with receiver/transmitter antenna counts %d x %d",
                snapshot->getNumReceiveAntennas(), snapshot->getNumTransmitAntennas(),
                receiverRadio->getAntenna()->getNumAntennas(), transmission->getTransmitterRadio()->getAntenna()->getNumAntennas());
        const auto deterministicPower = computeReceptionPower(receiverRadio, transmission, arrival);
        const auto spatialTransmissionPlan = getSpatialTransmissionPlan(transmission, snapshot,
            selectedTransmitAntenna, arrival->getEndTime() - arrival->getStartTime());
        const auto physicalPower = ChannelMatrixPhysicalPowerMaterializer::materialize(snapshot,
            spatialTransmissionPlan, arrival->getStartTime(), arrival->getEndTime(),
            transmissionAnalogModel->getCenterFrequency(), transmissionAnalogModel->getBandwidth(), deterministicPower);
        auto receptionAnalogModel = new ChannelMatrixReceptionAnalogModel(
            transmissionAnalogModel->getPreambleDuration(), transmissionAnalogModel->getHeaderDuration(),
            transmissionAnalogModel->getDataDuration(), transmissionAnalogModel->getCenterFrequency(),
            transmissionAnalogModel->getBandwidth(), snapshot, spatialTransmissionPlan,
            deterministicPower, physicalPower);
        return new Reception(receiverRadio, transmission, arrival->getStartTime(), arrival->getEndTime(),
            arrival->getStartPosition(), arrival->getEndPosition(), arrival->getStartOrientation(),
            arrival->getEndOrientation(), receptionAnalogModel);
    }
    auto transmissionAnalogModel = check_and_cast<const DimensionalSignalAnalogModel *>(transmission->getAnalogModel());
    const simtime_t receptionStartTime = arrival->getStartTime();
    const simtime_t receptionEndTime = arrival->getEndTime();
    const Coord& receptionStartPosition = arrival->getStartPosition();
    const Coord& receptionEndPosition = arrival->getEndPosition();
    const Quaternion& receptionStartOrientation = arrival->getStartOrientation();
    const Quaternion& receptionEndOrientation = arrival->getEndOrientation();
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& receptionPower = computeReceptionPower(receiverRadio, transmission, arrival);
    auto receptionAnalogModel = new DimensionalReceptionAnalogModel(transmissionAnalogModel->getPreambleDuration(), transmissionAnalogModel->getHeaderDuration(), transmissionAnalogModel->getDataDuration(), transmissionAnalogModel->getCenterFrequency(), transmissionAnalogModel->getBandwidth(), receptionPower);
    return new Reception(receiverRadio, transmission, receptionStartTime, receptionEndTime, receptionStartPosition, receptionEndPosition, receptionStartOrientation, receptionEndOrientation, receptionAnalogModel);
}

} // namespace physicallayer

} // namespace inet
