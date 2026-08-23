//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

#include <memory>

#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DataEncodingPlan.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Transmitter);

Ieee80211Transmitter::~Ieee80211Transmitter()
{
    delete channel;
}

void Ieee80211Transmitter::initialize(int stage)
{
    FlatTransmitterBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        const char *opMode = par("opMode");
        setModeSet(*opMode ? Ieee80211ModeSet::getModeSet(opMode) : nullptr);
        const char *bandName = par("bandName");
        setBand(*bandName != '\0' ? Ieee80211CompliantBands::getBand(bandName) : nullptr);
        setMode(modeSet != nullptr ? (bitrate != bps(-1) ? modeSet->getMode(bitrate, bandwidth) : modeSet->getFastestMode()) : nullptr);
        int channelNumber = par("channelNumber");
        if (channelNumber != -1)
            setChannelNumber(channelNumber);
    }
}

const IIeee80211Mode *Ieee80211Transmitter::computeTransmissionMode(const Packet *packet) const
{
    const IIeee80211Mode *transmissionMode;
    const auto& modeReq = const_cast<Packet *>(packet)->findTag<Ieee80211ModeReq>();
    const auto& bitrateReq = const_cast<Packet *>(packet)->findTag<SignalBitrateReq>();
    if (modeReq != nullptr) {
        if (modeSet != nullptr && !modeSet->containsMode(modeReq->getMode()))
            throw cRuntimeError("Unsupported mode requested");
        transmissionMode = modeReq->getMode();
    }
    else if (modeSet != nullptr && bitrateReq != nullptr)
        transmissionMode = modeSet->getMode(bitrateReq->getDataBitrate());
    else
        transmissionMode = mode;
    if (transmissionMode == nullptr)
        throw cRuntimeError("Transmission mode is undefined");
    return transmissionMode;
}

const Ieee80211Channel *Ieee80211Transmitter::computeTransmissionChannel(const Packet *packet) const
{
    const Ieee80211Channel *transmissionChannel;
    const auto& channelReq = const_cast<Packet *>(packet)->findTag<Ieee80211ChannelReq>();
    transmissionChannel = channelReq != nullptr ? channelReq->getChannel() : channel;
    if (transmissionChannel == nullptr)
        throw cRuntimeError("Transmission channel is undefined");
    return transmissionChannel;
}

void Ieee80211Transmitter::setModeSet(const Ieee80211ModeSet *modeSet)
{
    if (this->modeSet != modeSet) {
        this->modeSet = modeSet;
        if (mode != nullptr)
            mode = modeSet != nullptr ? modeSet->getMode(mode->getDataMode()->getNetBitrate()) : nullptr;
    }
}

void Ieee80211Transmitter::setMode(const IIeee80211Mode *mode)
{
    if (this->mode != mode) {
        if (modeSet->findMode(mode->getDataMode()->getNetBitrate(), mode->getDataMode()->getBandwidth()) == nullptr)
            throw cRuntimeError("Invalid mode");
        this->mode = mode;
    }
}

void Ieee80211Transmitter::setBand(const IIeee80211Band *band)
{
    if (this->band != band) {
        this->band = band;
        if (channel != nullptr)
            setChannel(new Ieee80211Channel(band, channel->getChannelNumber()));
    }
}

void Ieee80211Transmitter::setChannel(const Ieee80211Channel *channel)
{
    if (this->channel != channel) {
        delete this->channel;
        this->channel = channel;
        setCenterFrequency(channel->getCenterFrequency());
    }
}

void Ieee80211Transmitter::setChannelNumber(int channelNumber)
{
    if (channel == nullptr || channelNumber != channel->getChannelNumber())
        setChannel(new Ieee80211Channel(band, channelNumber));
}

std::ostream& Ieee80211Transmitter::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211Transmitter";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(modeSet, printFieldToString(modeSet, level + 1, evFlags))
               << EV_FIELD(band, printFieldToString(band, level + 1, evFlags));
    if (level <= PRINT_LEVEL_INFO)
        stream << EV_FIELD(mode, printFieldToString(mode, level + 1, evFlags))
               << EV_FIELD(channel, printFieldToString(channel, level + 1, evFlags));
    return FlatTransmitterBase::printToStream(stream, level);
}

const ITransmission *Ieee80211Transmitter::createTransmission(const IRadio *transmitter, const Packet *packet, simtime_t startTime) const
{
    auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet);
    const IIeee80211Mode *transmissionMode = computeTransmissionMode(packet);
    const Ieee80211Channel *transmissionChannel = computeTransmissionChannel(packet);
    W transmissionPower = computeTransmissionPower(packet);
    Hz transmissionBandwidth = transmissionMode->getDataMode()->getBandwidth();
    if (transmissionMode->getDataMode()->getNumberOfSpatialStreams() > transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Number of spatial streams is higher than the number of antennas");
    const b psduLength = B(phyHeader->getLengthField());
    auto planTag = packet->findTag<Ieee80211DataEncodingPlanTag>();
    const Ieee80211DataEncodingPlan *localEncodingPlan = nullptr;
    auto phyFormat = transmissionMode->getDataMode()->getPhyFormat();
    if (phyFormat == Ieee80211PhyFormat::HT || phyFormat == Ieee80211PhyFormat::VHT_SU) {
        if (planTag == nullptr || !planTag->hasPlan())
            throw cRuntimeError("HT/VHT transmission has no authoritative data encoding plan tag");
        localEncodingPlan = &planTag->getPlan();
        if (localEncodingPlan->getPhyFormat() != phyFormat ||
            localEncodingPlan->getFecType() != transmissionMode->getDataMode()->getFecType())
            throw cRuntimeError("HT/VHT data encoding plan disagrees with the selected mode");
    }
    const simtime_t preambleDuration = transmissionMode->getPreambleMode()->getDuration();
    const simtime_t headerDuration = transmissionMode->getHeaderMode()->getDuration();
    const simtime_t dataDuration = localEncodingPlan != nullptr ?
            localEncodingPlan->getNumberOfSymbols() * transmissionMode->getDataMode()->getSymbolInterval() :
            transmissionMode->getDataMode()->getDuration(psduLength);
    // The plan is a sender-local TXVECTOR construction aid. The receiver and
    // error model reconstruct it from timing and serialized PHY fields.
    const_cast<Packet *>(packet)->removeTagIfPresent<Ieee80211DataEncodingPlanTag>();
    const simtime_t duration = preambleDuration + headerDuration + dataDuration;
    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord& startPosition = mobility->getCurrentPosition();
    const Coord& endPosition = mobility->getCurrentPosition();
    const Quaternion& startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion& endOrientation = mobility->getCurrentAngularPosition();
    auto analogModel = getAnalogModel()->createAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, transmissionBandwidth, transmissionPower);
    return new Ieee80211Transmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, nullptr, nullptr, nullptr, nullptr, analogModel, transmissionMode, transmissionChannel);
}

} // namespace physicallayer

} // namespace inet
