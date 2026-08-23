//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduLayout.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyModeResolver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211SpatialTransmissionPlanBuilder.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

#include <limits>
#include <sstream>
#include <string>

namespace inet {

namespace physicallayer {

namespace {

std::vector<int> parseSpatialTransmitAntennaIndices(const std::string& value)
{
    std::string normalized = value;
    for (char& character : normalized)
        if (character == ',' || character == ';')
            character = ' ';
    std::stringstream stream(normalized);
    std::vector<int> indices;
    std::string token;
    while (stream >> token) {
        size_t parsedCharacters = 0;
        long long parsedValue;
        try {
            parsedValue = std::stoll(token, &parsedCharacters);
        }
        catch (const std::exception&) {
            throw cRuntimeError("Invalid spatialTransmitAntennaIndices entry '%s'", token.c_str());
        }
        if (parsedCharacters != token.size() || parsedValue < std::numeric_limits<int>::min() ||
            parsedValue > std::numeric_limits<int>::max())
            throw cRuntimeError("Invalid spatialTransmitAntennaIndices entry '%s'", token.c_str());
        indices.push_back((int)parsedValue);
    }
    return indices;
}

} // namespace

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
    std::shared_ptr<const Ieee80211HtPpduDescription> htPpduDescription;
    if (const auto htMode = dynamic_cast<const Ieee80211HtMode *>(transmissionMode)) {
        const auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader);
        if (htHeader == nullptr)
            throw cRuntimeError("HT mode requires an HT PHY header");

        Ieee80211HtPpduContext context;
        context.bandMode = htMode->getCenterFrequencyMode();
        context.preambleFormat = htMode->getPreambleMode()->getPreambleFormat();
        context.channelNumber = transmissionChannel->getChannelNumber();
        context.centerFrequency = transmissionChannel->getCenterFrequency();
        context.bandwidth = transmissionBandwidth;
        const auto resolution = Ieee80211PhyModeResolver::resolve(*htHeader, context);
        if (!resolution.isSuccess()) {
            const char *status = resolution.status == Ieee80211PhyModeResolver::Status::FORMAT_VIOLATION ? "format violation" : "reserved HT-SIG";
            throw cRuntimeError("Cannot publish HT transmission: %s", status);
        }

        const auto& signalField = resolution.description->getSignalField();
        const unsigned int modeMcs = htMode->getDataMode()->getModulationAndCodingScheme()->getMcsIndex();
        const bool modeCbw = transmissionBandwidth == MHz(40);
        const bool modeShortGi = htMode->getDataMode()->getGuardIntervalType() == Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT;
        const unsigned int modeStbc = htMode->getHeaderMode()->getSTBC();
        if (htCapabilities == nullptr)
            throw cRuntimeError("HT transmission requires immutable radio-owned local capabilities");
        const int modeNss = htMode->getDataMode()->getNumberOfSpatialStreams();
        htCapabilities->validateTransmission(modeMcs, modeNss,
            transmissionBandwidth, modeStbc != 0);
        if (modeStbc != 0)
            throw cRuntimeError("HT STBC transmission requires negotiated peer Rx-STBC/MCS/width capabilities; peer capability exchange is not enabled");
        if (transmissionBandwidth != MHz(20) && transmissionBandwidth != MHz(40))
            throw cRuntimeError("HT mode has unsupported channel bandwidth %s", transmissionBandwidth.str().c_str());
        if (signalField.mcs != modeMcs || signalField.cbw != modeCbw || signalField.shortGi != modeShortGi ||
            signalField.stbc != modeStbc || signalField.fecCoding) {
            throw cRuntimeError("HT-SIG contradicts the selected sender mode");
        }
        htPpduDescription = resolution.description;
    }
    if (transmissionMode->getDataMode()->getNumberOfSpatialStreams() > transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Number of spatial streams is higher than the number of antennas");
    const simtime_t duration = transmissionMode->getDuration(B(phyHeader->getLengthField()));
    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord& startPosition = mobility->getCurrentPosition();
    const Coord& endPosition = mobility->getCurrentPosition();
    const Quaternion& startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion& endOrientation = mobility->getCurrentAngularPosition();
    const simtime_t preambleDuration = transmissionMode->getPreambleMode()->getDuration();
    const simtime_t headerDuration = transmissionMode->getHeaderMode()->getDuration();
    const simtime_t dataDuration = duration - headerDuration - preambleDuration;
    std::shared_ptr<const SpatialTransmissionPlan> spatialTransmissionPlan;
    const std::string antennaIndexText = par("spatialTransmitAntennaIndices").stdstringValue();
    const auto antennaIndices = parseSpatialTransmitAntennaIndices(antennaIndexText);
    if (htPpduDescription != nullptr) {
        const Ieee80211HtPpduLayout layout(*htPpduDescription, duration);
        if (preambleDuration != layout.getDataStart())
            throw cRuntimeError("HT sender mode preamble duration %s contradicts canonical HT-LTF boundary %s",
                preambleDuration.str().c_str(), layout.getDataStart().str().c_str());
        spatialTransmissionPlan = Ieee80211SpatialTransmissionPlanBuilder::build(*htPpduDescription,
            duration, transmitter->getAntenna()->getNumAntennas(), antennaIndices);
    }
    else {
        const int numberOfTransmitAntennas = transmitter->getAntenna()->getNumAntennas();
        const int antennaIndex = antennaIndices.empty() ? 0 : antennaIndices.front();
        if (antennaIndex < 0 || antennaIndex >= numberOfTransmitAntennas)
            throw cRuntimeError("Legacy transmit antenna index %d is outside [0,%d)",
                antennaIndex, numberOfTransmitAntennas);
        ComplexMatrix mapping(numberOfTransmitAntennas, 1);
        mapping.get(antennaIndex, 0) = 1;
        SpatialTransmissionPlan::Segment segment(SIMTIME_ZERO, duration, 1, 1,
            mapping, {1.0});
        spatialTransmissionPlan = std::make_shared<const SpatialTransmissionPlan>(
            numberOfTransmitAntennas, std::vector<SpatialTransmissionPlan::Segment>{segment});
        spatialTransmissionPlan->validateCompleteCoverage(duration);
    }
    auto analogModel = getAnalogModel()->createAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, transmissionBandwidth, transmissionPower);
    return new Ieee80211Transmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, nullptr, nullptr, nullptr, nullptr, analogModel, transmissionMode, transmissionChannel, spatialTransmissionPlan, htPpduDescription);
}

} // namespace physicallayer

} // namespace inet
