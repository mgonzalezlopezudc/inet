//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

#include <cmath>

#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

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
        setMode(modeSet != nullptr ? (bitrate != bps(-1) ? modeSet->getMode(bitrate, band ? band->getSpacing() : bandwidth) : modeSet->getFastestMode(band ? band->getSpacing() : bandwidth)) : nullptr);
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
        transmissionMode = modeSet->getMode(bitrateReq->getDataBitrate(), band ? band->getSpacing() : bandwidth);
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
            mode = modeSet != nullptr ? modeSet->getMode(mode->getDataMode()->getNetBitrate(), band ? band->getSpacing() : bandwidth) : nullptr;
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
    int requiredSpatialStreams = transmissionMode->getDataMode()->getNumberOfSpatialStreams();
    if (auto heMuHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(phyHeader))
        for (unsigned int i = 0; i < heMuHeader->getUsersArraySize(); ++i)
            requiredSpatialStreams = std::max(requiredSpatialStreams,
                    (int)heMuHeader->getUsers(i).numberOfSpatialStreams);
    if (requiredSpatialStreams > transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Number of spatial streams is higher than the number of antennas");
    simtime_t duration = transmissionMode->getDuration(B(phyHeader->getLengthField()));
    simtime_t preambleDuration = transmissionMode->getPreambleMode()->getDuration();
    simtime_t headerDuration = transmissionMode->getHeaderMode()->getDuration();
    Hz transmissionCenterFrequency = centerFrequency;
    std::vector<Ieee80211HeUserPhyParameters> heUserPhyParameters;
    if (auto heMuHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(phyHeader)) {
        constexpr double HE_TONE_SPACING = 78125;
        if (heMuHeader->getCommonDuration() > SIMTIME_ZERO)
            duration = heMuHeader->getCommonDuration();
        preambleDuration = SimTime(40, SIMTIME_US);
        headerDuration = SimTime(8, SIMTIME_US);
        for (unsigned int i = 0; i < heMuHeader->getUsersArraySize(); ++i) {
            const auto& user = heMuHeader->getUsers(i);
            Ieee80211HeRu ru;
            ru.index = user.ruIndex;
            ru.toneSize = std::max<int>(user.ruToneSize, 26);
            ru.toneOffset = user.ruToneOffset;
            ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * HE_TONE_SPACING);
            heUserPhyParameters.push_back(computeHeUserPhyParameters(
                    user.psduLength, ru, user.mcs, user.numberOfSpatialStreams,
                    user.dcm,
                    static_cast<Ieee80211HeGuardInterval>(heMuHeader->getGuardInterval()),
                    static_cast<Ieee80211HeCoding>(heMuHeader->getCoding())));
        }
        if (heMuHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK && heMuHeader->getUsersArraySize() == 1) {
            const auto& user = heMuHeader->getUsers(0);
            int channelTones = getHeChannelToneCount(transmissionBandwidth);
            transmissionBandwidth = Hz(user.ruToneSize * HE_TONE_SPACING);
            double centerTone = user.ruToneOffset + user.ruToneSize / 2.0 - channelTones / 2.0;
            transmissionCenterFrequency = centerFrequency + Hz(centerTone * HE_TONE_SPACING);
            if (auto request = packet->findTag<Ieee80211HeMuReq>())
                if (!std::isnan(request->getTransmitPower().get()))
                    transmissionPower = request->getTransmitPower();
        }
    }
    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord& startPosition = mobility->getCurrentPosition();
    const Coord& endPosition = mobility->getCurrentPosition();
    const Quaternion& startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion& endOrientation = mobility->getCurrentAngularPosition();
    const simtime_t dataDuration = std::max(SIMTIME_ZERO, duration - headerDuration - preambleDuration);
    auto analogModel = getAnalogModel()->createAnalogModel(preambleDuration, headerDuration, dataDuration, transmissionCenterFrequency, transmissionBandwidth, transmissionPower);
    return new Ieee80211Transmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, nullptr, nullptr, nullptr, nullptr, analogModel, transmissionMode, transmissionChannel, heUserPhyParameters);
}

} // namespace physicallayer

} // namespace inet
