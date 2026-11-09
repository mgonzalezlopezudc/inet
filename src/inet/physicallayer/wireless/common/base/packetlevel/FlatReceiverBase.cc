//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"

#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/ApskModulationBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandNoiseBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ListeningDecision.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ReceptionDecision.h"

namespace inet {
namespace physicallayer {

FlatReceiverBase::FlatReceiverBase() :
    NarrowbandReceiverBase(),
    errorModel(nullptr),
    energyDetection(W(NaN)),
    sensitivity(W(NaN))
{
}

void FlatReceiverBase::initialize(int stage)
{
    NarrowbandReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        errorModel = dynamic_cast<IErrorModel *>(getSubmodule("errorModel"));
        energyDetection = mW(math::dBmW2mW(par("energyDetection")));
        sensitivity = mW(math::dBmW2mW(par("sensitivity")));
    }
}

std::ostream& FlatReceiverBase::printToStream(std::ostream& stream, int level, int evFlags) const
{
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(errorModel, printFieldToString(errorModel, level + 1, evFlags));
    if (level <= PRINT_LEVEL_INFO)
        stream << EV_FIELD(energyDetection)
               << EV_FIELD(sensitivity);
    return NarrowbandReceiverBase::printToStream(stream, level);
}

const IListeningDecision *FlatReceiverBase::computeListeningDecision(const IListening *listening, const IInterference *interference) const
{
    const IRadio *receiver = listening->getReceiverRadio();
    const IRadioMedium *radioMedium = receiver->getMedium();
    const IMediumAnalogModel *analogModel = radioMedium->getAnalogModel();
    const INoise *noise = analogModel->computeNoise(listening, interference);

    W maxPower = noise->computeMaxPower(listening->getStartTime(), listening->getEndTime());
    bool isListeningPossible = maxPower >= energyDetection;
    delete noise;
    EV_DEBUG << "Computing whether listening is possible: maximum power = " << maxPower << ", energy detection = " << energyDetection << " -> listening is " << (isListeningPossible ? "possible" : "impossible") << endl;
    return new ListeningDecision(listening, isListeningPossible);
}

const IReceptionResult *FlatReceiverBase::computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto receptionResult = NarrowbandReceiverBase::computeReceptionResult(listening, reception, interference, snir, decisions);
    if (errorModel != nullptr) {
        auto errorRateInd = const_cast<Packet *>(receptionResult->getPacket())->addTagIfAbsent<ErrorRateInd>();
        errorRateInd->setPacketErrorRate(errorModel->computePacketErrorRate(snir, IRadioSignal::SIGNAL_PART_WHOLE));
        errorRateInd->setBitErrorRate(errorModel->computeBitErrorRate(snir, IRadioSignal::SIGNAL_PART_WHOLE));
        errorRateInd->setSymbolErrorRate(errorModel->computeSymbolErrorRate(snir, IRadioSignal::SIGNAL_PART_WHOLE));
    }
    return receptionResult;
}

const IReceptionDecision *FlatReceiverBase::computeReceptionDecision(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part, const IInterference *interference, const ISnir *snir) const
{
    if (errorModel == nullptr)
        return NarrowbandReceiverBase::computeReceptionDecision(listening, reception, part, interference, snir);
    // the decisions read only the errors of the other parts, because their
    // attempted flags can differ from the one of this decision
    bool isReceptionPossible = computeIsReceptionPossible(listening, reception, part);
    bool isReceptionAttempted = isReceptionPossible && computeIsReceptionAttempted(listening, reception, part, interference);
    PartErrors errors;
    bool isPartReceived = false;
    switch (part) {
        case IRadioSignal::SIGNAL_PART_PREAMBLE:
            isPartReceived = computeIsPreambleReceived(reception, snir);
            break;
        case IRadioSignal::SIGNAL_PART_HEADER:
            if (computeIsPreambleReceived(reception, snir))
                errors = computePartErrors(reception, snir, part);
            isPartReceived = errors.isDecoded && !errors.hasErrors;
            break;
        case IRadioSignal::SIGNAL_PART_DATA: {
            // without a correct header, the receiver cannot decode the data
            const auto& headerErrors = getPartErrors(listening, reception, IRadioSignal::SIGNAL_PART_HEADER);
            if (headerErrors.isDecoded && !headerErrors.hasErrors)
                errors = computePartErrors(reception, snir, part);
            isPartReceived = errors.isDecoded && !errors.hasErrors;
            break;
        }
        case IRadioSignal::SIGNAL_PART_WHOLE: {
            // the whole signal keeps the failed regions of the data, which the received packet gets;
            // without a correct header or a complete data part, it fails without regions
            const auto& headerErrors = getPartErrors(listening, reception, IRadioSignal::SIGNAL_PART_HEADER);
            const auto& dataErrors = getPartErrors(listening, reception, IRadioSignal::SIGNAL_PART_DATA);
            errors.isDecoded = headerErrors.isDecoded && !headerErrors.hasErrors && dataErrors.isDecoded;
            errors.hasErrors = !errors.isDecoded || dataErrors.hasErrors;
            if (errors.isDecoded)
                errors.regions = dataErrors.regions;
            isPartReceived = !errors.hasErrors;
            break;
        }
        default:
            throw cRuntimeError("Unknown signal part: '%s'", IRadioSignal::getSignalPartName(part));
    }
    return new ReceptionDecision(reception, part, isReceptionPossible, isReceptionAttempted, isReceptionAttempted && isPartReceived, errors);
}

bool FlatReceiverBase::computePartRange(const ITransmission *transmission, IRadioSignal::SignalPart part, b& offset, b& length) const
{
    throw cRuntimeError("%s does not define the content of the signal parts, which the error decisions of the signal regions need", getClassName());
}

void FlatReceiverBase::computeRegionInterval(const IReception *reception, IRadioSignal::SignalPart part, b offset, b length, b partLength, simtime_t& startTime, simtime_t& endTime) const
{
    simtime_t partStartTime = reception->getStartTime(part);
    simtime_t partDuration = reception->getEndTime(part) - partStartTime;
    startTime = partStartTime + partDuration * (offset.get<b>() / (double)partLength.get<b>());
    endTime = partStartTime + partDuration * ((offset + length).get<b>() / (double)partLength.get<b>());
}

bool FlatReceiverBase::computeIsPreambleReceived(const IReception *reception, const ISnir *snir) const
{
    simtime_t startTime = reception->getStartTime(IRadioSignal::SIGNAL_PART_PREAMBLE);
    simtime_t endTime = reception->getEndTime(IRadioSignal::SIGNAL_PART_PREAMBLE);
    return endTime <= startTime || computeIsSnirAboveThreshold(snir, startTime, endTime);
}

PartErrors FlatReceiverBase::computePartErrors(const IReception *reception, const ISnir *snir, IRadioSignal::SignalPart part) const
{
    PartErrors errors;
    auto transmission = reception->getTransmission();
    b partOffset, partLength;
    if (!computePartRange(transmission, part, partOffset, partLength))
        return errors;
    errors.isDecoded = true;
    // a part without content in the packet is one region, the packet mode has one
    // region for each part, and the other modes have one region for each chunk
    std::vector<std::pair<b, b>> regions;
    if (partOffset == b(-1))
        regions.push_back({b(-1), partLength});
    else if (partLength != b(0) && errorModel->getCorruptionMode() == IErrorModel::CorruptionMode::CM_PACKET)
        regions.push_back({partOffset, partLength});
    else if (partLength != b(0)) {
        const auto& content = transmission->getPacket()->peekAt(partOffset, partLength, Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
        b offset = partOffset;
        auto addRegion = [&] (const Ptr<const Chunk>& chunk) {
            regions.push_back({offset, chunk->getChunkLength()});
            offset += chunk->getChunkLength();
        };
        if (content->getChunkType() == Chunk::CT_SEQUENCE) {
            for (const auto& chunk : staticPtrCast<const SequenceChunk>(content)->getChunks())
                addRegion(chunk);
        }
        else
            addRegion(content);
    }
    for (const auto& [offset, length] : regions) {
        simtime_t startTime, endTime;
        computeRegionInterval(reception, part, offset == b(-1) ? b(0) : offset - partOffset, length, partLength, startTime, endTime);
        double errorRate = errorModel->computePacketErrorRate(snir, part, length, startTime, endTime);
        if (!(0 <= errorRate && errorRate <= 1))
            throw cRuntimeError("Invalid error rate of a region: %g", errorRate);
        // the threshold is a condition, and the error rate is a probability
        bool isFailed = !computeIsSnirAboveThreshold(snir, startTime, endTime) || (errorRate != 0 && (errorRate == 1 || !(dblrand() > errorRate)));
        if (isFailed) {
            errors.hasErrors = true;
            if (offset != b(-1))
                errors.regions.push_back({offset, length, errorRate});
        }
    }
    return errors;
}

const PartErrors& FlatReceiverBase::getPartErrors(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto radio = reception->getReceiverRadio();
    auto decision = radio->getMedium()->getReceptionDecision(radio, listening, reception->getTransmission(), part);
    return check_and_cast<const ReceptionDecision *>(decision)->getErrors();
}

Packet *FlatReceiverBase::computeReceivedPacket(const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    // the decision of the whole signal keeps the failed regions that the received packet gets
    for (auto decision : *decisions) {
        if (decision->getSignalPart() == IRadioSignal::SIGNAL_PART_WHOLE) {
            const auto& regions = check_and_cast<const ReceptionDecision *>(decision)->getErrors().regions;
            if (!regions.empty()) {
                auto transmittedPacket = snir->getReception()->getTransmission()->getPacket();
                auto receivedPacket = errorModel->computeCorruptedPacket(transmittedPacket, regions);
                receivedPacket->clearTags();
                receivedPacket->addTag<PacketProtocolTag>()->setProtocol(transmittedPacket->getTag<PacketProtocolTag>()->getProtocol());
                return receivedPacket;
            }
        }
    }
    return NarrowbandReceiverBase::computeReceivedPacket(snir, decisions);
}

} // namespace physicallayer
} // namespace inet

