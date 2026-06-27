//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

#include <cmath>
#include <sstream>

// IEEE 802.11ax HE transmitter.
//
// Builds Ieee80211Transmission objects and computes per-user RU parameters for
// HE MU and HE TB PPDUs.  Uses the HE PHY calculator (Ieee80211HePhyCalculator)
// to validate PPDU parameters and determine the common duration.
// Relevant clauses:
//   - Clause 27.3.11: HE PPDU formats.
//   - Clause 27.3.11.12: HE TB PPDU format.
//   - Clause 27.3.11.13: HE MU PPDU format and HE-SIG-B.
//   - Clause 27.3.12: modulation and coding for the HE data field.
//
// Approximations / simplifications:
//   - HE-LTF type is hardcoded to 4x for all HE MU/HE TB PPDUs.  The standard
//     permits 1x/2x/4x HE-LTF modes; only 4x is currently supported.
//   - The HE MU PHY header length is estimated as HE-SIG-A + HE-SIG-B common +
//     20 bits per user, not a bit-exact serialization of Tables 27-21..27-24.
//   - MU-MIMO grouping is detected only by checking whether multiple users
//     share the same RU index.  Full standard MU-MIMO grouping constraints are
//     enforced later in Ieee80211HePhyCalculator.
//   - Single-user HE TB transmissions shift center frequency/power to the
//     assigned RU; this approximates per-RU UL transmission without modeling
//     trigger-based timing advance.

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
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/networklayer/common/NetworkInterface.h"

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
        WATCH_PTR(modeSet);
        WATCH_PTR(mode);
        WATCH_PTR(band);
        WATCH_PTR(channel);
        WATCH(lastHePpdu);
        WATCH(lastHePpduFormat);
        WATCH(lastHeMuMimo);
        WATCH(lastHeUserCount);
        WATCH(lastHeTotalNsts);
        WATCH(lastHePacketExtensionDurationUs);
        WATCH(lastHePuncturedSubchannelMask);
        WATCH(lastHeDuration);
        WATCH(lastHeCenterFrequency);
        WATCH(lastHeBandwidth);
        WATCH(lastHeTransmitPower);
        WATCH_VECTOR(lastHeUserPhyParameters);
        WATCH_EXPR("modeName", mode != nullptr ? mode->getName() : "none");
        WATCH_EXPR("lastHeTransmissionSummary", getLastHeTransmissionSummary());
    }
}

std::string Ieee80211Transmitter::getLastHeTransmissionSummary() const
{
    if (!lastHePpdu)
        return "last transmission was not an HE MU/TB PPDU";
    std::stringstream stream;
    stream << "format=" << lastHePpduFormat
           << ", users=" << lastHeUserCount
           << ", totalNsts=" << lastHeTotalNsts
           << ", muMimo=" << (lastHeMuMimo ? "yes" : "no")
           << ", pe=" << lastHePacketExtensionDurationUs << "us"
           << ", punctureMask=0x" << std::hex << lastHePuncturedSubchannelMask << std::dec
           << ", cf=" << lastHeCenterFrequency
           << ", bw=" << lastHeBandwidth
           << ", power=" << lastHeTransmitPower
           << ", duration=" << lastHeDuration;
    return stream.str();
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

    // Map to LDPC variant if enabled/negotiated
    cModule *nic = getContainingNicModule(this);
    auto mib = nic ? dynamic_cast<const ieee80211::Ieee80211Mib *>(nic->getSubmodule("mib")) : nullptr;
    if (mib != nullptr) {
        Ptr<const ieee80211::Ieee80211MacHeader> macHeader;
        auto frontChunk = packet->peekAtFront();
        if (dynamic_cast<const Ieee80211PhyHeader *>(frontChunk.get()) != nullptr) {
            auto packetCopy = packet->dup();
            packetCopy->popAtFront<Ieee80211PhyHeader>();
            auto innerFront = packetCopy->peekAtFront();
            if (dynamic_cast<const ieee80211::Ieee80211MacHeader *>(innerFront.get()) != nullptr) {
                macHeader = packetCopy->peekAtFront<ieee80211::Ieee80211MacHeader>();
            }
            delete packetCopy;
        }
        else if (dynamic_cast<const ieee80211::Ieee80211MacHeader *>(frontChunk.get()) != nullptr) {
            macHeader = packet->peekAtFront<ieee80211::Ieee80211MacHeader>();
        }
        MacAddress receiverAddress = macHeader != nullptr ? macHeader->getReceiverAddress() : MacAddress::UNSPECIFIED_ADDRESS;

        bool useLdpc = false;
        if (auto htMode = dynamic_cast<const Ieee80211HtMode *>(transmissionMode)) {
            useLdpc = mib->localHtLdpc;
        }
        else if (auto vhtMode = dynamic_cast<const Ieee80211VhtMode *>(transmissionMode)) {
            if (receiverAddress != MacAddress::UNSPECIFIED_ADDRESS && !receiverAddress.isMulticast()) {
                auto negotiatedVht = mib->findNegotiatedVhtCapabilities(receiverAddress);
                useLdpc = negotiatedVht ? negotiatedVht->intersection.ldpc : mib->localVhtCapabilities.ldpc;
            }
            else {
                useLdpc = mib->localVhtCapabilities.ldpc;
            }
        }
        else if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(transmissionMode)) {
            if (receiverAddress != MacAddress::UNSPECIFIED_ADDRESS && !receiverAddress.isMulticast()) {
                auto negotiatedHe = mib->findNegotiatedHeCapabilities(receiverAddress);
                useLdpc = negotiatedHe ? negotiatedHe->intersection.ldpc : mib->localHeCapabilities.ldpc;
            }
            else {
                useLdpc = mib->localHeCapabilities.ldpc;
            }
        }

        if (auto htMode = dynamic_cast<const Ieee80211HtMode *>(transmissionMode)) {
            auto mcs = htMode->getDataMode()->getModulationAndCodingScheme();
            auto preambleFormat = htMode->getPreambleMode()->getPreambleFormat();
            auto gi = htMode->getDataMode()->getGuardIntervalType();
            auto centerFreqMode = htMode->getCenterFrequencyMode();
            transmissionMode = Ieee80211HtCompliantModes::getCompliantMode(mcs, centerFreqMode, preambleFormat, gi, useLdpc);
        }
        else if (auto vhtMode = dynamic_cast<const Ieee80211VhtMode *>(transmissionMode)) {
            auto mcs = vhtMode->getDataMode()->getModulationAndCodingScheme();
            auto preambleFormat = vhtMode->getPreambleMode()->getPreambleFormat();
            auto gi = vhtMode->getDataMode()->getGuardIntervalType();
            auto centerFreqMode = vhtMode->getCenterFrequencyMode();
            transmissionMode = Ieee80211VhtCompliantModes::getCompliantMode(mcs, centerFreqMode, preambleFormat, gi, useLdpc);
        }
        else if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(transmissionMode)) {
            auto mcs = heMode->getDataMode()->getModulationAndCodingScheme();
            auto preambleFormat = heMode->getPreambleMode()->getPreambleFormat();
            auto gi = heMode->getDataMode()->getGuardIntervalType();
            auto centerFreqMode = heMode->getCenterFrequencyMode();
            transmissionMode = Ieee80211HeCompliantModes::getCompliantMode(mcs, centerFreqMode, preambleFormat, gi, useLdpc);
        }
    }

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
    auto heMuHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(phyHeader);
    const IIeee80211Mode *transmissionMode = computeTransmissionMode(packet);
    const Ieee80211Channel *transmissionChannel = computeTransmissionChannel(packet);
    W transmissionPower = computeTransmissionPower(packet);
    Hz transmissionBandwidth = transmissionMode->getDataMode()->getBandwidth();
    int requiredSpatialStreams = transmissionMode->getDataMode()->getNumberOfSpatialStreams();
    if (heMuHeader != nullptr) {
        if (heMuHeader->getMuMimo()) {
            requiredSpatialStreams = heMuHeader->getTotalNsts();
        } else {
            for (unsigned int i = 0; i < heMuHeader->getUsersArraySize(); ++i)
                requiredSpatialStreams = std::max(requiredSpatialStreams,
                        (int)heMuHeader->getUsers(i).numberOfSpatialStreams);
        }
    }
    if (requiredSpatialStreams > transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Number of spatial streams is higher than the number of antennas");
    simtime_t duration = transmissionMode->getDuration(B(phyHeader->getLengthField()));
    simtime_t preambleDuration = transmissionMode->getPreambleMode()->getDuration();
    simtime_t headerDuration = transmissionMode->getHeaderMode()->getDuration();
    Hz transmissionCenterFrequency = centerFrequency;
    std::vector<Ieee80211HeUserPhyParameters> heUserPhyParameters;
    Ieee80211HePpduParameters hePpduParameters;
    if (heMuHeader != nullptr) {
        constexpr double HE_TONE_SPACING = 78125;
        std::vector<Ieee80211HeUserPhyParameters> requestedUsers;
        for (unsigned int i = 0; i < heMuHeader->getUsersArraySize(); ++i) {
            const auto& user = heMuHeader->getUsers(i);
            Ieee80211HeRu ru;
            ru.index = user.ruIndex;
            ru.toneSize = std::max<int>(user.ruToneSize, 26);
            ru.toneOffset = user.ruToneOffset;
            ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * HE_TONE_SPACING);
            Ieee80211HeUserPhyParameters requested;
            requested.ru = ru;
            requested.mcs = user.mcs;
            requested.numberOfSpatialStreams = user.numberOfSpatialStreams;
            requested.dcm = user.dcm;
            requested.coding = static_cast<Ieee80211HeCoding>(heMuHeader->getCoding());
            requested.psduLength = user.psduLength;
            requested.streamStartIndex = user.streamStartIndex;
            requested.staId = user.staId;
            requestedUsers.push_back(requested);
        }
        auto calculation = computeHePpduParameters(requestedUsers, transmissionBandwidth,
                static_cast<Ieee80211HePpduFormat>(heMuHeader->getPpduFormat()),
                static_cast<Ieee80211HeGuardInterval>(heMuHeader->getGuardInterval()), HE_LTF_4X,
                heMuHeader->getPacketExtensionDurationUs());
        if (!calculation)
            throw cRuntimeError("Invalid planned HE MU PPDU: %s", calculation.error.c_str());
        hePpduParameters = calculation.parameters;
        if (heMuHeader->getPpduFormat() == HE_MU_DOWNLINK &&
                heMuHeader->getCommonDuration() > SIMTIME_ZERO &&
                heMuHeader->getCommonDuration() != calculation.parameters.duration)
            throw cRuntimeError("Serialized HE MU duration does not match the resolved PHY parameters");
        duration = std::max(heMuHeader->getCommonDuration(), calculation.parameters.duration);
        hePpduParameters.duration = duration;
        heUserPhyParameters = hePpduParameters.users;
        for (auto& user : heUserPhyParameters)
            user.duration = duration;
        preambleDuration = hePpduParameters.common.commonPreambleDuration;
        headerDuration = SIMTIME_ZERO;
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
    lastHePpdu = heMuHeader != nullptr;
    if (lastHePpdu) {
        lastHePpduFormat = heMuHeader->getPpduFormat();
        lastHeMuMimo = heMuHeader->getMuMimo();
        lastHeUserCount = heMuHeader->getUsersArraySize();
        lastHeTotalNsts = heMuHeader->getTotalNsts();
        lastHePacketExtensionDurationUs = heMuHeader->getPacketExtensionDurationUs();
        lastHePuncturedSubchannelMask = heMuHeader->getPuncturedSubchannelMask();
        lastHeDuration = duration;
        lastHeCenterFrequency = transmissionCenterFrequency;
        lastHeBandwidth = transmissionBandwidth;
        lastHeTransmitPower = transmissionPower;
        lastHeUserPhyParameters = heUserPhyParameters;
    }
    else {
        lastHePpduFormat = -1;
        lastHeMuMimo = false;
        lastHeUserCount = 0;
        lastHeTotalNsts = 0;
        lastHePacketExtensionDurationUs = 0;
        lastHePuncturedSubchannelMask = 0;
        lastHeDuration = duration;
        lastHeCenterFrequency = transmissionCenterFrequency;
        lastHeBandwidth = transmissionBandwidth;
        lastHeTransmitPower = transmissionPower;
        lastHeUserPhyParameters.clear();
    }
    return new Ieee80211Transmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, nullptr, nullptr, nullptr, nullptr, analogModel, transmissionMode, transmissionChannel, heUserPhyParameters, hePpduParameters);
}

} // namespace physicallayer

} // namespace inet
