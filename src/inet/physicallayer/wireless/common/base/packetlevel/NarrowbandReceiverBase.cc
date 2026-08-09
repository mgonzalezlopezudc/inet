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

static bool containsFrequencySegments(const std::vector<FrequencySegment>& listeningSegments,
        const std::vector<FrequencySegment>& signalSegments)
{
    for (const auto& signal : signalSegments) {
        const Hz signalMin = signal.centerFrequency - signal.bandwidth / 2;
        const Hz signalMax = signal.centerFrequency + signal.bandwidth / 2;
        bool contained = false;
        for (const auto& listening : listeningSegments) {
            const Hz listeningMin = listening.centerFrequency - listening.bandwidth / 2;
            const Hz listeningMax = listening.centerFrequency + listening.bandwidth / 2;
            contained |= signalMin >= listeningMin - Hz(100) && signalMax <= listeningMax + Hz(100);
        }
        if (!contained)
            return false;
    }
    return true;
}

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
    auto bandListening = check_and_cast<const BandListening *>(listening);
    return containsFrequencySegments(bandListening->getFrequencySegments(),
            narrowbandTransmission->getFrequencySegments());
}

// TODO this is not purely functional, see interface comment
bool NarrowbandReceiverBase::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
    auto narrowbandReception = check_and_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel());
    if (!containsFrequencySegments(bandListening->getFrequencySegments(), narrowbandReception->getFrequencySegments())) {
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
