//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
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
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Receiver);

static const Ieee80211HeMuRuAllocation *findAllocationForReceiver(const Ieee80211HeMuTag *tag, const MacAddress& receiverAddress)
{
    if (tag == nullptr)
        return nullptr;
    for (const auto& allocation : tag->getAllocations())
        if (allocation.staAddress == receiverAddress)
            return &allocation;
    return nullptr;
}

static void appendHeMuAllocations(const Ptr<Ieee80211HeMuPhyHeader>& phyHeader, const std::vector<Ieee80211HeMuRuAllocation>& allocations)
{
    for (const auto& allocation : allocations) {
        Ieee80211HeMuRuAllocationInfo info;
        info.ruIndex = allocation.ru.index;
        info.staAddress = allocation.staAddress;
        phyHeader->appendAllocations(info);
    }
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

static Packet *buildHeMuPhyPacket(Packet *macPacket, const Ieee80211HeMuTag *tag, const IReception *reception)
{
    auto packet = macPacket->dup();
    auto phyHeader = makeShared<Ieee80211HeMuPhyHeader>();
    appendHeMuAllocations(phyHeader, tag->getAllocations());
    phyHeader->setLengthField(B(packet->getDataLength()));
    phyHeader->setChunkLength(b(8 + phyHeader->getAllocationsArraySize() * 56));
    packet->insertAtFront(phyHeader);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);
    return packet;
}

static Packet *buildLegacyHeMuPreambleIndication(const Ieee80211HeMuTag *tag, const IReception *reception)
{
    auto packet = new Packet("HE-MU-Legacy-Preamble");
    auto phyHeader = makeShared<Ieee80211HeMuPhyHeader>();
    appendHeMuAllocations(phyHeader, tag->getAllocations());
    phyHeader->setLengthField(B(0));
    phyHeader->setChunkLength(b(8 + phyHeader->getAllocationsArraySize() * 56));
    packet->insertAtFront(phyHeader);
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
    auto packet = transmission->getPacket();
    auto heMuTag = packet != nullptr ? packet->findTag<Ieee80211HeMuTag>() : nullptr;
    if (heMuTag == nullptr)
        return true;
    auto networkInterface = getContainingNicModule(this);
    auto myMacAddress = networkInterface->getMacAddress();
    return findAllocationForReceiver(heMuTag.get(), myMacAddress) != nullptr;
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    auto packet = transmission->getPacket();
    auto heMuTag = packet != nullptr ? packet->findTag<Ieee80211HeMuTag>() : nullptr;
    if (heMuTag != nullptr)
        return ieee80211Transmission && !heMuTag->getAllocations().empty() &&
               NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) &&
           NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto packet = reception->getTransmission()->getPacket();
    auto heMuTag = packet != nullptr ? packet->findTag<Ieee80211HeMuTag>() : nullptr;
    if (heMuTag != nullptr)
        return ieee80211Transmission && !heMuTag->getAllocations().empty() &&
               getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) &&
           getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
}

const IReceptionResult *Ieee80211Receiver::computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto transmittedPacket = transmission->getPacket();
    auto heMuTag = transmittedPacket != nullptr ? transmittedPacket->findTag<Ieee80211HeMuTag>() : nullptr;
    if (heMuTag != nullptr) {
        auto networkInterface = getContainingNicModule(this);
        auto myMacAddress = networkInterface->getMacAddress();
        auto allocation = findAllocationForReceiver(heMuTag.get(), myMacAddress);
        auto packet = allocation != nullptr && modeSet->containsMode(transmission->getMode())
                ? buildHeMuPhyPacket(allocation->packet, heMuTag.get(), reception)
                : buildLegacyHeMuPreambleIndication(heMuTag.get(), reception);
        if (!isReceptionSuccessful(decisions))
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
