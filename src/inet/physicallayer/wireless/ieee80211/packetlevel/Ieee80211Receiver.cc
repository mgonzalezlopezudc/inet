//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Protocol.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReceptionDecision.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISnir.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IInterference.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INoise.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ReceptionResult.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandNoiseBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Receiver);

static Ptr<const Ieee80211HeMuPhyHeader> peekHeMuPhyHeader(const ITransmission *transmission)
{
    auto packet = transmission->getPacket();
    return transmission->getPacketProtocol() == &Protocol::ieee80211HePhy && packet != nullptr && packet->hasAtFront<Ieee80211HeMuPhyHeader>()
            ? packet->peekAtFront<Ieee80211HeMuPhyHeader>()
            : nullptr;
}

static bool containsHeMuUser(const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader, uint16_t staId)
{
    for (unsigned int i = 0; i < phyHeader->getUsersArraySize(); ++i)
        if (phyHeader->getUsers(i).staId == staId)
            return true;
    return false;
}

static Ptr<Ieee80211HeMuPhyHeader> copyHeMuPhyHeader(const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader)
{
    return staticPtrCast<Ieee80211HeMuPhyHeader>(phyHeader->dupShared());
}

static void addReceptionIndications(Packet *packet, const IReception *reception, const IInterference *interference, const ISnir *snir)
{
    auto snirInd = packet->addTagIfAbsent<SnirInd>();
    snirInd->setMinimumSnir(snir->getMin());
    snirInd->setMaximumSnir(snir->getMax());
    snirInd->setAverageSnir(snir->getMean());
    auto signalTimeInd = packet->addTagIfAbsent<SignalTimeInd>();
    signalTimeInd->setStartTime(reception->getStartTime());
    signalTimeInd->setEndTime(reception->getEndTime());
    if (auto narrowbandNoise = dynamic_cast<const NarrowbandNoiseBase *>(snir->getNoise())) {
        auto analogModel = reception->getTransmission()->getMedium()->getAnalogModel();
        auto signalPlusNoise = dynamic_cast<const NarrowbandNoiseBase *>(analogModel->computeNoise(reception, narrowbandNoise));
        if (signalPlusNoise != nullptr) {
            auto signalPower = signalPlusNoise->computeMinPower(reception->getStartTime(), reception->getEndTime()) - narrowbandNoise->computeMinPower(reception->getStartTime(), reception->getEndTime());
            auto signalPowerInd = packet->addTagIfAbsent<SignalPowerInd>();
            signalPowerInd->setPower(signalPower);
        }
        delete signalPlusNoise;
    }
    if (snir->getMax() == 0) {
        auto errorRateInd = packet->addTagIfAbsent<ErrorRateInd>();
        errorRateInd->setSymbolErrorRate(1);
        errorRateInd->setBitErrorRate(1);
        errorRateInd->setPacketErrorRate(1);
    }
    else if (snir->getMin() == INFINITY) {
        auto errorRateInd = packet->addTagIfAbsent<ErrorRateInd>();
        errorRateInd->setSymbolErrorRate(0);
        errorRateInd->setBitErrorRate(0);
        errorRateInd->setPacketErrorRate(0);
    }
}

static bool isReceptionSuccessful(const std::vector<const IReceptionDecision *> *decisions)
{
    bool successful = true;
    for (auto decision : *decisions)
        successful &= decision->isReceptionSuccessful();
    return successful;
}

static bool applyHeMuMpduReceiveOutcomes(Packet *packet,
        const std::vector<const IReceptionDecision *> *decisions, cRNG *rng)
{
    bool commonSuccessful = true;
    bool dataSuccessful = true;
    for (auto decision : *decisions) {
        if (decision->isReceptionSuccessful())
            continue;
        switch (decision->getSignalPart()) {
            case IRadioSignal::SIGNAL_PART_PREAMBLE:
            case IRadioSignal::SIGNAL_PART_HEADER:
            case IRadioSignal::SIGNAL_PART_WHOLE:
                commonSuccessful = false;
                break;
            case IRadioSignal::SIGNAL_PART_DATA:
                dataSuccessful = false;
                break;
            default:
                break;
        }
    }
    auto indication = packet->findTagForUpdate<Ieee80211MpduReceiveInd>();
    if (!commonSuccessful || indication == nullptr)
        return commonSuccessful && dataSuccessful;
    if (dataSuccessful)
        return true;

    int64_t totalBytes = 0;
    for (unsigned int i = 0; i < indication->getResultsArraySize(); ++i) {
        const auto& result = indication->getResults(i);
        if (result.status == MPDU_NOT_EVALUATED)
            totalBytes += std::max<int64_t>(1, result.length.get<B>());
    }
    if (totalBytes == 0)
        return false;

    int64_t failedByte = std::min<int64_t>(totalBytes - 1,
            static_cast<int64_t>(rng->doubleRand() * totalBytes));
    int64_t cumulativeBytes = 0;
    bool failureAssigned = false;
    for (unsigned int i = 0; i < indication->getResultsArraySize(); ++i) {
        auto result = indication->getResults(i);
        if (result.status == MPDU_NOT_EVALUATED) {
            cumulativeBytes += std::max<int64_t>(1, result.length.get<B>());
            if (!failureAssigned && failedByte < cumulativeBytes) {
                result.status = MPDU_FCS_ERROR;
                failureAssigned = true;
            }
            else
                result.status = MPDU_SUCCESS;
            indication->setResults(i, result);
        }
    }
    return true;
}

static Packet *extractHeMuMpdu(const Packet *transmittedPacket, uint16_t staId)
{
    auto packetCopy = transmittedPacket->dup();
    packetCopy->popAtFront<Ieee80211HeMuPhyHeader>();
    packetCopy->popAtFront<ieee80211::Ieee80211MacHeader>();
    while (packetCopy->getDataLength() > b(0) && packetCopy->hasAtFront<Ieee80211HeMuRuPayloadHeader>()) {
        auto payloadHeader = packetCopy->popAtFront<Ieee80211HeMuRuPayloadHeader>();
        if (payloadHeader->getStaId() == staId) {
            auto mpdu = new Packet(transmittedPacket->getName());
            mpdu->insertAtBack(packetCopy->popAtFront(payloadHeader->getMpduLength()));
            auto indication = mpdu->addTagIfAbsent<Ieee80211MpduReceiveInd>();
            auto parser = mpdu->dup();
            B offset(0);
            while (parser->getDataLength() > b(0) &&
                    parser->hasAtFront<ieee80211::Ieee80211MpduSubframeHeader>()) {
                auto delimiter = parser->popAtFront<ieee80211::Ieee80211MpduSubframeHeader>(
                        B(-1), Chunk::PF_ALLOW_INCORRECT);
                Ieee80211MpduReceiveResult receiveResult;
                receiveResult.offset = offset;
                receiveResult.length = B(delimiter->getLength());
                receiveResult.status = delimiter->isIncorrect() ?
                        MPDU_DELIMITER_ERROR : MPDU_NOT_EVALUATED;
                if (parser->getDataLength() >= receiveResult.length) {
                    auto macHeader = dynamicPtrCast<const ieee80211::Ieee80211DataHeader>(
                            parser->peekAtFront<ieee80211::Ieee80211MacHeader>(
                                    B(-1), Chunk::PF_ALLOW_INCORRECT));
                    if (macHeader != nullptr) {
                        receiveResult.sequenceNumber = macHeader->getSequenceNumber().get();
                        receiveResult.fragmentNumber = macHeader->getFragmentNumber();
                        receiveResult.tid = macHeader->getTid();
                    }
                    parser->popAtFront(receiveResult.length, Chunk::PF_ALLOW_INCORRECT);
                }
                else {
                    receiveResult.status = MPDU_PAYLOAD_ERROR;
                    parser->popAtFront(parser->getDataLength(), Chunk::PF_ALLOW_INCORRECT);
                }
                indication->appendResults(receiveResult);
                offset += B(4) + receiveResult.length;
                int padding = (4 - (B(4) + receiveResult.length).get<B>() % 4) % 4;
                if (padding > 0 && parser->getDataLength() >= B(padding)) {
                    parser->popAtFront(B(padding), Chunk::PF_ALLOW_INCORRECT);
                    offset += B(padding);
                }
            }
            delete parser;
            delete packetCopy;
            return mpdu;
        }
        packetCopy->popAtFront(payloadHeader->getMpduLength());
    }
    delete packetCopy;
    return nullptr;
}

static Packet *buildHeMuPhyPacket(const Packet *transmittedPacket, const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader, uint16_t staId)
{
    auto packet = extractHeMuMpdu(transmittedPacket, staId);
    if (packet == nullptr)
        return nullptr;
    auto phyHeaderCopy = copyHeMuPhyHeader(phyHeader);
    phyHeaderCopy->setLengthField(B(packet->getDataLength()));
    packet->insertAtFront(phyHeaderCopy);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);
    return packet;
}

static Packet *buildLegacyHeMuPreambleIndication(const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader, const IReception *reception)
{
    auto packet = new Packet("HE-MU-Legacy-Preamble");
    auto phyHeaderCopy = copyHeMuPhyHeader(phyHeader);
    phyHeaderCopy->setLengthField(B(0));
    packet->insertAtFront(phyHeaderCopy);
    packet->addTagIfAbsent<Ieee80211HeMuLegacyPreambleInd>()->setDurationField(reception->getTransmission()->getDuration());
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);
    return packet;
}

Ieee80211Receiver::~Ieee80211Receiver()
{
    delete channel;
}

void Ieee80211Receiver::initialize(int stage)
{
    FlatReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        const char *opMode = par("opMode");
        setModeSet(*opMode ? Ieee80211ModeSet::getModeSet(opMode) : nullptr);
        const char *bandName = par("bandName");
        setBand(*bandName != '\0' ? Ieee80211CompliantBands::getBand(bandName) : nullptr);
        int channelNumber = par("channelNumber");
        if (channelNumber != -1)
            setChannelNumber(channelNumber);
    }
}

std::ostream& Ieee80211Receiver::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211Receiver";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(modeSet, printFieldToString(modeSet, level + 1, evFlags))
               << EV_FIELD(band, printFieldToString(band, level + 1, evFlags));
    if (level <= PRINT_LEVEL_INFO)
        stream << EV_FIELD(channel, printFieldToString(channel, level + 1, evFlags));
    return FlatReceiverBase::printToStream(stream, level);
}

bool Ieee80211Receiver::isAssignedHeMuRu(const ITransmission *transmission) const
{
    auto heMuPhyHeader = peekHeMuPhyHeader(transmission);
    if (heMuPhyHeader == nullptr)
        return true;
    if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK)
        return true;
    auto networkInterface = getContainingNicModule(this);
    auto staId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
    return staId.has_value() && containsHeMuUser(heMuPhyHeader, *staId);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    auto heMuPhyHeader = peekHeMuPhyHeader(transmission);
    if (heMuPhyHeader != nullptr)
        return ieee80211Transmission && heMuPhyHeader->getUsersArraySize() > 0 &&
               NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) &&
           NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto heMuPhyHeader = peekHeMuPhyHeader(reception->getTransmission());
    if (heMuPhyHeader != nullptr)
        return ieee80211Transmission && heMuPhyHeader->getUsersArraySize() > 0 &&
               getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) &&
           getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
}

bool Ieee80211Receiver::computeIsReceptionAttempted(const IListening *listening, const IReception *reception,
        IRadioSignal::SignalPart part, const IInterference *interference) const
{
    auto heMuPhyHeader = peekHeMuPhyHeader(reception->getTransmission());
    if (heMuPhyHeader == nullptr || heMuPhyHeader->getPpduFormat() != HE_TRIGGER_BASED_UPLINK)
        return FlatReceiverBase::computeIsReceptionAttempted(listening, reception, part, interference);
    if (!computeIsReceptionPossible(listening, reception, part))
        return false;

    // Propagation delay makes aligned STA responses arrive a few nanoseconds
    // apart, so ordinary single-reception arbitration would admit only the
    // first RU. Allow concurrent UL-TB reception only within one Trigger
    // exchange; the RU-aware interference model still decides success.
    auto currentTransmission = reception->getReceiverRadio()->getReceptionInProgress();
    if (currentTransmission == nullptr || currentTransmission == reception->getTransmission())
        return true;
    auto currentHeader = peekHeMuPhyHeader(currentTransmission);
    return currentHeader != nullptr &&
           currentHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK &&
           currentHeader->getTriggerId() == heMuPhyHeader->getTriggerId();
}

const IReceptionResult *Ieee80211Receiver::computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto transmittedPacket = transmission->getPacket();
    auto heMuPhyHeader = peekHeMuPhyHeader(transmission);
    if (heMuPhyHeader != nullptr) {
        if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK) {
            auto packet = transmittedPacket->dup();
            if (!isReceptionSuccessful(decisions))
                packet->setBitError(true);
            addReceptionIndications(packet, reception, interference, snir);
            packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
            packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
            return new ReceptionResult(reception, decisions, packet);
        }
        auto networkInterface = getContainingNicModule(this);
        auto myStaId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
        auto packet = myStaId.has_value() && containsHeMuUser(heMuPhyHeader, *myStaId) &&
                modeSet->containsMode(transmission->getMode())
                ? buildHeMuPhyPacket(transmittedPacket, heMuPhyHeader, *myStaId)
                : buildLegacyHeMuPreambleIndication(heMuPhyHeader, reception);
        if (packet == nullptr)
            packet = buildLegacyHeMuPreambleIndication(heMuPhyHeader, reception);
        if (!applyHeMuMpduReceiveOutcomes(packet, decisions, getRNG(0)))
            packet->setBitError(true);
        addReceptionIndications(packet, reception, interference, snir);
        packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
        packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
        return new ReceptionResult(reception, decisions, packet);
    }

    auto receptionResult = FlatReceiverBase::computeReceptionResult(listening, reception, interference, snir, decisions);
    auto packet = const_cast<Packet *>(receptionResult->getPacket());
    packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
    packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
    return receptionResult;
}

void Ieee80211Receiver::setModeSet(const Ieee80211ModeSet *modeSet)
{
    this->modeSet = modeSet;
}

void Ieee80211Receiver::setBand(const IIeee80211Band *band)
{
    if (this->band != band) {
        this->band = band;
        if (channel != nullptr)
            setChannel(new Ieee80211Channel(band, channel->getChannelNumber()));
    }
}

void Ieee80211Receiver::setChannel(const Ieee80211Channel *channel)
{
    if (this->channel != channel) {
        delete this->channel;
        this->channel = channel;
        setCenterFrequency(channel->getCenterFrequency());
    }
}

void Ieee80211Receiver::setChannelNumber(int channelNumber)
{
    if (channel == nullptr || channelNumber != channel->getChannelNumber())
        setChannel(new Ieee80211Channel(band, channelNumber));
}

} // namespace physicallayer

} // namespace inet
