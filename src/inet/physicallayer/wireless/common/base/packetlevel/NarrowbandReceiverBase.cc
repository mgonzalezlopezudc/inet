//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandReceiverBase.h"

#include "inet/physicallayer/wireless/common/base/packetlevel/ApskModulationBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandNoiseBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ListeningDecision.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ReceptionDecision.h"

namespace inet {

namespace physicallayer {

NarrowbandReceiverBase::NarrowbandReceiverBase() :
    SnirReceiverBase(),
    modulation(nullptr),
    centerFrequency(Hz(NaN)),
    bandwidth(Hz(NaN))
{
}

void NarrowbandReceiverBase::initialize(int stage)
{
    SnirReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        modulation = ApskModulationBase::findModulation(par("modulation"));
        centerFrequency = Hz(par("centerFrequency"));
        bandwidth = Hz(par("bandwidth"));
    }
}

std::ostream& NarrowbandReceiverBase::printToStream(std::ostream& stream, int level, int evFlags) const
{
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(modulation, printFieldToString(modulation, level + 1, evFlags))
               << EV_FIELD(centerFrequency)
               << EV_FIELD(bandwidth);
    return SnirReceiverBase::printToStream(stream, level);
}

const IListening *NarrowbandReceiverBase::createListening(const IRadio *radio, const simtime_t startTime, const simtime_t endTime, const Coord& startPosition, const Coord& endPosition) const
{
    return new BandListening(radio, startTime, endTime, startPosition, endPosition, centerFrequency, bandwidth);
}

bool NarrowbandReceiverBase::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    // TODO check if modulation matches?
    auto narrowbandTransmission = check_and_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel());
    Hz rxMin = centerFrequency - bandwidth / 2;
    Hz rxMax = centerFrequency + bandwidth / 2;
    Hz txMin = narrowbandTransmission->getCenterFrequency() - narrowbandTransmission->getBandwidth() / 2;
    Hz txMax = narrowbandTransmission->getCenterFrequency() + narrowbandTransmission->getBandwidth() / 2;
    return (txMin.get() >= rxMin.get() - 100.0) && (txMax.get() <= rxMax.get() + 100.0);
}

// TODO this is not purely functional, see interface comment
bool NarrowbandReceiverBase::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
    auto narrowbandReception = check_and_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel());
    Hz rxMin = bandListening->getCenterFrequency() - bandListening->getBandwidth() / 2;
    Hz rxMax = bandListening->getCenterFrequency() + bandListening->getBandwidth() / 2;
    Hz txMin = narrowbandReception->getCenterFrequency() - narrowbandReception->getBandwidth() / 2;
    Hz txMax = narrowbandReception->getCenterFrequency() + narrowbandReception->getBandwidth() / 2;
    if (!((txMin.get() >= rxMin.get() - 100.0) && (txMax.get() <= rxMax.get() + 100.0))) {
        EV_DEBUG << "Computing whether reception is possible: reception band is not within listening band -> reception is impossible" << endl;
        return false;
    }
    else
        return true;
}

const IReceptionDecision *NarrowbandReceiverBase::computeReceptionDecision(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part, const IInterference *interference, const ISnir *snir) const
{
//    const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
//    const NarrowbandReceptionBase *narrowbandReception = check_and_cast<const NarrowbandReceptionBase *>(reception);
//    if (bandListening->getCenterFrequency() == narrowbandReception->getCenterFrequency() && bandListening->getBandwidth() >= narrowbandReception->getBandwidth())
        return SnirReceiverBase::computeReceptionDecision(listening, reception, part, interference, snir);
//    else
//        return new ReceptionDecision(reception, part, false, false, false);
}

} // namespace physicallayer

} // namespace inet

