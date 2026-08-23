//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceiverAnalogModel.h"

#include "inet/common/math/Functions.h"

namespace inet {
namespace physicallayer {

Define_Module(DimensionalReceiverAnalogModel);

void DimensionalReceiverAnalogModel::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        defaultCenterFrequency = Hz(par("defaultCenterFrequency"));
        defaultBandwidth = Hz(par("defaultBandwidth"));
        defaultSensitivity = mW(math::dBmW2mW(par("defaultSensitivity")));
    }
}

IListening *DimensionalReceiverAnalogModel::createListening(const IRadio *radio, const simtime_t startTime, const simtime_t endTime, const Coord& startPosition, const Coord& endPosition, Hz centerFrequency, Hz bandwidth) const
{
    if (std::isnan(centerFrequency.get()))
        centerFrequency = defaultCenterFrequency;
    if (std::isnan(bandwidth.get()))
        bandwidth = defaultBandwidth;

    if (std::isnan(centerFrequency.get()))
        throw cRuntimeError("DimensionalReceiverAnalogModel: Center frequency is not specified (neither as default nor as a runtime parameter)");
    if (std::isnan(bandwidth.get()))
        throw cRuntimeError("DimensionalReceiverAnalogModel: Bandwidth is not specified (neither as default nor as a runtime parameter)");

    return new BandListening(radio, startTime, endTime, startPosition, endPosition, centerFrequency, bandwidth);
}

IListening *DimensionalReceiverAnalogModel::createListening(const IRadio *radio, const simtime_t startTime, const simtime_t endTime,
        const Coord& startPosition, const Coord& endPosition, const std::vector<FrequencyBand>& occupiedBands) const
{
    return new MultibandListening(radio, startTime, endTime, startPosition, endPosition, occupiedBands);
}

bool DimensionalReceiverAnalogModel::computeIsReceptionPossible(const IListening *listening, const IReception *reception, W sensitivity) const
{
    if (std::isnan(sensitivity.get()))
        sensitivity = defaultSensitivity;

    if (std::isnan(sensitivity.get()))
        throw cRuntimeError("DimensionalReceiverAnalogModel: Sensitivity is not specified (neither as default nor as a runtime parameter)");

    const DimensionalReceptionAnalogModel *analogModel = check_and_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
    if (auto multibandListening = dynamic_cast<const MultibandListening *>(listening)) {
        if (!multibandListening->contains(analogModel->getOccupiedBands())) {
            EV_DEBUG << "Computing whether reception is possible: listening and reception band sets differ -> reception is impossible" << endl;
            return false;
        }
    }
    else {
        if (analogModel->getOccupiedBands().size() > 1) {
            EV_DEBUG << "Computing whether reception is possible: a multiband reception requires a multiband listening mask" << endl;
            return false;
        }
        const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
        auto listeningMin = bandListening->getCenterFrequency() - bandListening->getBandwidth() / 2;
        auto listeningMax = bandListening->getCenterFrequency() + bandListening->getBandwidth() / 2;
        auto receptionMin = analogModel->getCenterFrequency() - analogModel->getBandwidth() / 2;
        auto receptionMax = analogModel->getCenterFrequency() + analogModel->getBandwidth() / 2;
        if (receptionMin >= listeningMin && receptionMax <= listeningMax) {
            Point<simsec> startPoint{ simsec(reception->getStartTime()) };
            Point<simsec> endPoint{ simsec(reception->getEndTime()) };
            W minReceptionPower = integrate<WpHz, Domain<simsec, Hz>, 0b10, W, Domain<simsec>>(analogModel->getPower())->getMin(Interval<simsec>(startPoint, endPoint, 0b1, 0b1, 0b0));
            ASSERT(W(0.0) <= minReceptionPower);
            bool isReceptionPossible = minReceptionPower >= sensitivity;
            EV_DEBUG << "Computing whether reception is possible" << EV_FIELD(minReceptionPower) << EV_FIELD(sensitivity) << " -> reception is " << (isReceptionPossible ? "possible" : "impossible") << endl;
            return isReceptionPossible;
        }
        EV_DEBUG << "Computing whether reception is possible: listening and reception bands are different -> reception is impossible" << endl;
        return false;
    }
    Point<simsec> startPoint{ simsec(reception->getStartTime()) };
    Point<simsec> endPoint{ simsec(reception->getEndTime()) };
    W minReceptionPower = integrate<WpHz, Domain<simsec, Hz>, 0b10, W, Domain<simsec>>(analogModel->getPower())->getMin(Interval<simsec>(startPoint, endPoint, 0b1, 0b1, 0b0));
    ASSERT(W(0.0) <= minReceptionPower);
    bool isReceptionPossible = minReceptionPower >= sensitivity;
    EV_DEBUG << "Computing whether reception is possible" << EV_FIELD(minReceptionPower) << EV_FIELD(sensitivity) << " -> reception is " << (isReceptionPossible ? "possible" : "impossible") << endl;
    return isReceptionPossible;
}


} // namespace physicallayer
} // namespace inet
