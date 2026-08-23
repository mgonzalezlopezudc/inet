//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IMultibandTransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

#include <memory>

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Transmitter);

namespace {

const Ieee80211Channel *cloneChannelForBand(const Ieee80211Channel *channel, const IIeee80211Band *band)
{
    if (channel == nullptr)
        return nullptr;
    if (band == nullptr)
        throw cRuntimeError("Cannot retain an IEEE 802.11 channel without a band");
    if (channel->isExplicitGeometry())
        return new Ieee80211Channel(band, channel->getChannelNumber(), channel->getChannelWidth(),
                channel->getCenterFrequencyIndex0(), channel->getCenterFrequencyIndex1());
    return new Ieee80211Channel(band, channel->getChannelNumber(), channel->getSecondaryChannelOffset());
}

void validateTransmissionMode(const Ieee80211Channel *channel, const IIeee80211Mode *mode)
{
    if (channel == nullptr || mode == nullptr)
        throw cRuntimeError("Transmission channel and mode must be defined");
    Hz bandwidth = mode->getDataMode()->getBandwidth();
    // Legacy DSSS/CCK has a 22 MHz occupied signal while its operating
    // channel is the 20 MHz primary.  Keep that long-standing behavior while
    // applying the explicit 20/40/80/160 geometry checks to VHT/HT widths.
    if (bandwidth == MHz(22)) {
        if (channel->getOperatingBandwidth() != MHz(20))
            throw cRuntimeError("22 MHz legacy PPDUs require a 20 MHz operating channel");
    }
    else
        channel->validatePpduWidth(bandwidth);
    if (bandwidth >= MHz(80) && dynamic_cast<const Ieee80211VhtMode *>(mode) == nullptr)
        throw cRuntimeError("IEEE 802.11 80/160 MHz PPDUs require a VHT mode");
    if (channel->is80Plus80() && bandwidth == MHz(160) && dynamic_cast<const Ieee80211VhtMode *>(mode) == nullptr)
        throw cRuntimeError("IEEE 802.11 80+80 MHz PPDUs require a VHT mode");
}

}

Hz Ieee80211Transmitter::getTransmissionCenterFrequency(const Ieee80211Channel *channel, Hz bandwidth)
{
    if (channel == nullptr)
        throw cRuntimeError("Transmission channel is undefined");
    if (bandwidth == MHz(22)) {
        if (channel->getOperatingBandwidth() != MHz(20))
            throw cRuntimeError("22 MHz legacy PPDUs require a 20 MHz operating channel");
        return channel->getPrimary20CenterFrequency();
    }
    if (bandwidth == MHz(20))
        return channel->getPrimary20CenterFrequency();
    if (bandwidth == MHz(40))
        return channel->getPrimary40CenterFrequency();
    if (bandwidth == MHz(80))
        return channel->getPrimary80CenterFrequency();
    if (bandwidth == MHz(160) && !channel->is80Plus80())
        return channel->getOperatingCenterFrequency();
    throw cRuntimeError("Unsupported IEEE 802.11 contiguous PPDU bandwidth: %s", bandwidth.str().c_str());
}

std::vector<Ieee80211ChannelBand> Ieee80211Transmitter::getTransmissionOccupiedBands(const Ieee80211Channel *channel, Hz bandwidth)
{
    if (channel == nullptr)
        throw cRuntimeError("Transmission channel is undefined");
    if (channel->is80Plus80() && bandwidth == MHz(160))
        return channel->getOccupiedBands();
    return {Ieee80211ChannelBand(getTransmissionCenterFrequency(channel, bandwidth), bandwidth)};
}

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
        int channelNumber = par("channelNumber");
        if (channelNumber != -1)
            setChannelNumber(channelNumber);
        if (modeSet != nullptr) {
            const IIeee80211Mode *initialMode = nullptr;
            if (bitrate != bps(-1))
                initialMode = std::isnan(bandwidth.get()) ? modeSet->findMode(bitrate) : modeSet->findMode(bitrate, bandwidth);
            // Radio-level initialization resolves the complete target
            // channel and may need to replace a legacy/default bitrate with a
            // width-compatible VHT mode.  Do not fail early merely because an
            // inherited default bitrate is absent from the selected mode set.
            if (initialMode == nullptr)
                initialMode = std::isnan(bandwidth.get()) ? modeSet->getFastestMode() : modeSet->getFastestMode(bandwidth);
            setMode(initialMode);
        }
    }
}

const IIeee80211Mode *Ieee80211Transmitter::computeTransmissionMode(const Packet *packet) const
{
    const IIeee80211Mode *transmissionMode;
    const auto& modeReq = const_cast<Packet *>(packet)->findTag<Ieee80211ModeReq>();
    const auto& bitrateReq = const_cast<Packet *>(packet)->findTag<SignalBitrateReq>();
    if (modeReq != nullptr) {
        if (modeSet != nullptr && !modeSet->supportsMode(modeReq->getMode()))
            throw cRuntimeError("Unsupported mode requested");
        transmissionMode = modeReq->getMode();
    }
    else if (modeSet != nullptr && bitrateReq != nullptr)
        transmissionMode = std::isnan(bandwidth.get()) ? modeSet->getMode(bitrateReq->getDataBitrate()) :
                modeSet->getMode(bitrateReq->getDataBitrate(), bandwidth);
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
        auto newMode = mode;
        if (mode != nullptr && modeSet != nullptr && !modeSet->containsMode(mode)) {
            newMode = modeSet->findCompatibleMode(mode);
            if (newMode == nullptr)
                throw cRuntimeError("Cannot map current mode to operation mode '%s' without changing bitrate, bandwidth, spatial streams, or guard interval", modeSet->getName());
        }
        else if (modeSet == nullptr)
            newMode = nullptr;
        this->modeSet = modeSet;
        mode = newMode;
    }
}

void Ieee80211Transmitter::setModeSetAndMode(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode)
{
    if (modeSet != nullptr && mode != nullptr && !modeSet->containsMode(mode))
        throw cRuntimeError("Invalid mode");
    this->modeSet = modeSet;
    this->mode = mode;
}

void Ieee80211Transmitter::setMode(const IIeee80211Mode *mode)
{
    if (this->mode != mode) {
        if (modeSet != nullptr && mode != nullptr && !modeSet->containsMode(mode))
            throw cRuntimeError("Invalid mode");
        this->mode = mode;
    }
}

std::function<void()> Ieee80211Transmitter::saveChannelState()
{
    auto savedChannel = std::make_shared<std::unique_ptr<const Ieee80211Channel>>();
    if (channel != nullptr)
        savedChannel->reset(new Ieee80211Channel(*channel));
    return [this, savedChannel, oldBand = band, oldBandwidth = bandwidth, oldCenterFrequency = centerFrequency]() {
        delete channel;
        channel = savedChannel->release();
        band = oldBand;
        bandwidth = oldBandwidth;
        centerFrequency = oldCenterFrequency;
    };
}

void Ieee80211Transmitter::setBand(const IIeee80211Band *band)
{
    if (this->band != band) {
        std::unique_ptr<const Ieee80211Channel> replacement(cloneChannelForBand(channel, band));
        this->band = band;
        if (replacement != nullptr)
            setChannel(replacement.release());
    }
}

void Ieee80211Transmitter::setChannel(const Ieee80211Channel *channel)
{
    if (this->channel != channel) {
        delete this->channel;
        this->channel = channel;
        this->band = channel->getBand();
        setCenterFrequency(channel->getOperatingCenterFrequency());
    }
}

void Ieee80211Transmitter::setChannelNumber(int channelNumber)
{
    if (channel == nullptr || channelNumber != channel->getChannelNumber())
        setChannel(channel != nullptr && channel->isExplicitGeometry() ?
                new Ieee80211Channel(band, channelNumber, channel->getChannelWidth(), channel->getCenterFrequencyIndex0(), channel->getCenterFrequencyIndex1()) :
                new Ieee80211Channel(band, channelNumber, channel == nullptr ? IEEE80211_SECONDARY_CHANNEL_NONE : channel->getSecondaryChannelOffset()));
}

bool Ieee80211Transmitter::isHtChannelWidthSupported(Hz channelWidth) const
{
    bool channelConfigured = channelWidth == MHz(20) ||
            (channelWidth == MHz(40) && channel != nullptr &&
             channel->getSecondaryChannelOffset() != IEEE80211_SECONDARY_CHANNEL_NONE);
    return channelConfigured && modeSet != nullptr && modeSet->getHtSupportedChannelWidths().count(channelWidth) != 0;
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
    validateTransmissionMode(transmissionChannel, transmissionMode);
    if (transmissionMode->getDataMode()->getNumberOfSpatialStreams() > transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Number of spatial streams is higher than the number of antennas");
    const simtime_t duration = transmissionMode->getDuration(B(phyHeader->getLengthField()));
    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord& startPosition = mobility->getCurrentPosition();
    const Coord& endPosition = mobility->getCurrentPosition();
    const Quaternion& startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion& endOrientation = mobility->getCurrentAngularPosition();
    const simtime_t preambleDuration = transmissionMode->getPreambleDuration();
    const simtime_t headerDuration = transmissionMode->getHeaderDuration();
    const simtime_t dataDuration = transmissionMode->getDataDuration(B(phyHeader->getLengthField()));
    if (preambleDuration < SIMTIME_ZERO || headerDuration < SIMTIME_ZERO || dataDuration < SIMTIME_ZERO ||
        preambleDuration + headerDuration + dataDuration != duration)
        throw cRuntimeError("Invalid transmission duration decomposition for mode %s", transmissionMode->getName());
    // IEEE Std 802.11-2024, 19.3.15.4 and Table 21-22: a PPDU is placed on
    // the selected primary hierarchy.  A complete VHT 80+80 PPDU is the one
    // exception: it remains one transmission while its analog signal has two
    // occupied 80 MHz components.
    ITransmissionAnalogModel *analogModel;
    if (transmissionChannel->is80Plus80() && transmissionBandwidth == MHz(160)) {
        auto multibandFactory = dynamic_cast<const IMultibandTransmitterAnalogModel *>(getAnalogModel());
        if (multibandFactory == nullptr)
            throw cRuntimeError("IEEE 802.11 VHT 80+80 MHz requires a multiband transmitter analog model");
        auto occupiedBands = transmissionChannel->getOccupiedBands();
        if (occupiedBands.size() != 2)
            throw cRuntimeError("IEEE 802.11 VHT 80+80 MHz requires exactly two occupied 80 MHz bands");
        analogModel = multibandFactory->createAnalogModel(preambleDuration, headerDuration, dataDuration, occupiedBands, transmissionPower);
    }
    else {
        Hz transmissionCenterFrequency = getTransmissionCenterFrequency(transmissionChannel, transmissionBandwidth);
        analogModel = getAnalogModel()->createAnalogModel(preambleDuration, headerDuration, dataDuration, transmissionCenterFrequency, transmissionBandwidth, transmissionPower);
    }
    return new Ieee80211Transmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, nullptr, nullptr, nullptr, nullptr, analogModel, transmissionMode, transmissionChannel);
}

} // namespace physicallayer

} // namespace inet
