//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include <algorithm>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixReceptionProcessor.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISpatialSnir.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ReceptionDecision.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduLayout.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"

namespace inet {

namespace physicallayer {

namespace {

std::vector<int> getLegacyDataSubcarrierIndices(bool ht40)
{
    // IEEE Std 802.11-2024 17.3.5.8--17.3.5.10 and 19.3.11.10.
    std::vector<int> result;
    if (!ht40) {
        for (int index = -26; index <= 26; index++)
            if (index != 0 && index != -21 && index != -7 && index != 7 && index != 21)
                result.push_back(index);
    }
    else {
        for (int index = -58; index <= -6; index++)
            if (index != -53 && index != -39 && index != -32 && index != -25 && index != -11)
                result.push_back(index);
        for (int index = 6; index <= 58; index++)
            if (index != 11 && index != 25 && index != 32 && index != 39 && index != 53)
                result.push_back(index);
    }
    return result;
}

std::vector<int> getHtDataSubcarrierIndices(bool ht40)
{
    // IEEE Std 802.11-2024 19.3.11.10, 19.3.11.11.3 (19-58),
    // and 19.3.11.11.4 (19-59).
    const int first = ht40 ? -58 : -28;
    const int last = -first;
    const std::vector<int> pilots = ht40 ?
        std::vector<int>{-53, -25, -11, 11, 25, 53} :
        std::vector<int>{-21, -7, 7, 21};
    std::vector<int> result;
    for (int index = first; index <= last; index++) {
        if (index == 0 || (ht40 && (index == -1 || index == 1)) ||
            std::find(pilots.begin(), pilots.end(), index) != pilots.end())
            continue;
        result.push_back(index);
    }
    return result;
}

std::vector<int> getOfdmDataSubcarrierIndices()
{
    std::vector<int> result;
    for (int index = -26; index <= 26; index++)
        if (index != 0 && index != -21 && index != -7 && index != 7 && index != 21)
            result.push_back(index);
    return result;
}

void appendResources(std::vector<ChannelMatrixResourceCell>& result,
    simtime_t startOffset, simtime_t endOffset, const std::vector<int>& subcarrierIndices,
    IRadioSignal::SignalPart part, Hz fullBandwidth)
{
    const Hz spacing = kHz(312.5);
    if (subcarrierIndices.empty())
        throw cRuntimeError("Cannot normalize an empty OFDM resource set");
    const double psdScale = fullBandwidth.get() /
        (spacing.get() * subcarrierIndices.size());
    for (const int index : subcarrierIndices)
        result.emplace_back(startOffset, endOffset,
            spacing * (index - 0.5), spacing * (index + 0.5), part, psdScale);
}

} // namespace

Define_Module(Ieee80211Receiver);

Ieee80211Receiver::~Ieee80211Receiver()
{
    delete channel;
}

void Ieee80211Receiver::initialize(int stage)
{
    FlatReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        auto processorModule = getSubmodule("channelMatrixReceptionProcessor");
        channelMatrixReceptionProcessor = dynamic_cast<const IChannelMatrixReceptionProcessor *>(processorModule);
        if (processorModule != nullptr && channelMatrixReceptionProcessor == nullptr)
            throw cRuntimeError("Channel-matrix reception processor submodule does not implement its C++ contract");
        if (channelMatrixReceptionProcessor != nullptr) {
            const auto radioModule = getParentModule();
            if (radioModule == nullptr || !radioModule->hasPar("separateReceptionParts"))
                throw cRuntimeError("Channel-matrix receiver cannot resolve the parent radio reception-part policy");
            if (radioModule->par("separateReceptionParts").boolValue())
                throw cRuntimeError("Channel-matrix reception currently requires separateReceptionParts=false; revision-safe cached per-part decisions are not implemented");
        }
        const int configuredMaximumCells = par("maximumMaterializedResourceCells");
        if (configuredMaximumCells <= 0)
            throw cRuntimeError("Maximum materialized resource cells must be positive");
        maximumMaterializedResourceCells = configuredMaximumCells;
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

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (!ieee80211Transmission)
        return false;
    // HT receive authority is the canonical on-air HT-SIG description. Local
    // decoder support is deliberately not checked here so listening filters do
    // not hide the PPDU's physical energy from CCA.
    const bool recognizedMode = ieee80211Transmission->getHtPpduDescription() != nullptr ||
        modeSet->containsMode(ieee80211Transmission->getMode());
    return recognizedMode && NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    if (!ieee80211Transmission || !getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity))
        return false;
    return ieee80211Transmission->getHtPpduDescription() != nullptr ?
        supportsHtReception(ieee80211Transmission, part) : modeSet->containsMode(ieee80211Transmission->getMode());
}

bool Ieee80211Receiver::supportsHtReception(const Ieee80211Transmission *transmission,
    IRadioSignal::SignalPart part) const
{
    const auto& description = transmission->getHtPpduDescription();
    if (!description || !htCapabilities)
        return false;
    // Bounded local support following the canonical receiver-independent
    // HT-SIG interpretation. Valid but unsupported forms remain distinct from
    // malformed fields. IEEE Std 802.11-2024 Tables 19-11 and 19-12.
    if (description->getPreambleFormat() != Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED ||
        description->getFecCoding() ||
        description->getNumberOfExtensionSpatialStreams() != 0)
        return false;
    if ((description->getBandwidth() == MHz(20) && !htCapabilities->getHt20Supported()) ||
        (description->getBandwidth() == MHz(40) && !htCapabilities->getHt40Supported()))
        return false;
    if (part == IRadioSignal::SIGNAL_PART_PREAMBLE || part == IRadioSignal::SIGNAL_PART_HEADER)
        return true;
    if (!htCapabilities->supportsMcs(description->getMcs()) || description->getMcs() > 31 ||
        description->getNss() > htCapabilities->getMaximumSupportedSpatialStreams())
        return false;
    if (description->getStbc() != 0) {
        if (htCapabilities->getRxStbc() < description->getNss() || channelMatrixReceptionProcessor == nullptr)
            return false;
        const auto& plan = transmission->getSpatialTransmissionPlan();
        return plan && plan->getSegments().back().hasSpaceTimeCode();
    }
    return description->getNss() == 1 || channelMatrixReceptionProcessor != nullptr;
}

std::vector<ChannelMatrixResourceCell> Ieee80211Receiver::getChannelMatrixResourceCells(
    const IReception& reception) const
{
    const auto transmission = dynamic_cast<const Ieee80211Transmission *>(reception.getTransmission());
    if (transmission == nullptr)
        return {};
    if (transmission->getHtPpduDescription() != nullptr) {
        const auto& description = *transmission->getHtPpduDescription();
        if (description.getPreambleFormat() != Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED)
            return {};
        return buildHtResourceCells(description, reception.getDuration());
    }
    if (const auto ofdmMode = dynamic_cast<const Ieee80211OfdmMode *>(transmission->getMode())) {
        const auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket());
        return buildLegacyOfdmResourceCells(*ofdmMode, B(phyHeader->getLengthField()));
    }
    return {};
}

std::vector<ChannelMatrixResourceCell> Ieee80211Receiver::buildHtResourceCells(
    const Ieee80211HtPpduDescription& description, simtime_t ppduDuration)
{
    if (description.getPreambleFormat() != Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED)
        throw cRuntimeError("HT resource cells currently support mixed-format PPDUs only");
    const bool ht40 = description.getBandwidth() == MHz(40);
    const Hz fullBandwidth = description.getBandwidth();
    const Ieee80211HtPpduLayout layout(description, ppduDuration);
    const auto legacySubcarriers = getLegacyDataSubcarrierIndices(ht40);
    const auto htSubcarriers = getHtDataSubcarrierIndices(ht40);
    std::vector<ChannelMatrixResourceCell> resources;
    appendResources(resources, SIMTIME_ZERO, layout.getLegacyShortTrainingEnd(),
        legacySubcarriers, IRadioSignal::SIGNAL_PART_PREAMBLE, fullBandwidth);
    appendResources(resources, layout.getLegacyShortTrainingEnd(), layout.getLegacyLongTrainingEnd(),
        legacySubcarriers, IRadioSignal::SIGNAL_PART_PREAMBLE, fullBandwidth);
    appendResources(resources, layout.getLegacyLongTrainingEnd(), layout.getLegacySignalEnd(),
        legacySubcarriers, IRadioSignal::SIGNAL_PART_PREAMBLE, fullBandwidth);
    appendResources(resources, layout.getLegacySignalEnd(), layout.getFirstHtSignalSymbolEnd(),
        legacySubcarriers, IRadioSignal::SIGNAL_PART_HEADER, fullBandwidth);
    appendResources(resources, layout.getFirstHtSignalSymbolEnd(), layout.getRobustMixedPreambleEnd(),
        legacySubcarriers, IRadioSignal::SIGNAL_PART_HEADER, fullBandwidth);
    appendResources(resources, layout.getRobustMixedPreambleEnd(), layout.getHtShortTrainingEnd(),
        htSubcarriers, IRadioSignal::SIGNAL_PART_PREAMBLE, fullBandwidth);
    for (int ltf = 0; ltf < description.getNumberOfDataLongTrainingFields(); ltf++) {
        appendResources(resources, layout.getHtLongTrainingFieldStart(ltf),
            layout.getHtLongTrainingFieldEnd(ltf),
            htSubcarriers, IRadioSignal::SIGNAL_PART_PREAMBLE, fullBandwidth);
    }
    for (int symbol = 0; symbol < layout.getNumberOfDataSymbols(); symbol++) {
        const simtime_t start = layout.getDataStart() + layout.getDataSymbolDuration() * symbol;
        appendResources(resources, start, start + layout.getDataSymbolDuration(), htSubcarriers,
            IRadioSignal::SIGNAL_PART_DATA, fullBandwidth);
    }
    return resources;
}

std::vector<ChannelMatrixResourceCell> Ieee80211Receiver::buildLegacyOfdmResourceCells(
    const Ieee80211OfdmMode& mode, b dataLength)
{
    const auto indices = getOfdmDataSubcarrierIndices();
    const Hz spacing = mode.getDataMode()->getSubcarrierFrequencySpacing();
    const double psdScale = mode.getDataMode()->getBandwidth().get() /
        (spacing.get() * indices.size());
    auto append = [&] (std::vector<ChannelMatrixResourceCell>& result,
        simtime_t start, simtime_t end, IRadioSignal::SignalPart part) {
        for (const int index : indices)
            result.emplace_back(start, end, spacing * (index - 0.5),
                spacing * (index + 0.5), part, psdScale);
    };

    std::vector<ChannelMatrixResourceCell> resources;
    const simtime_t shortTrainingEnd = mode.getPreambleMode()->getShortTrainingSequenceDuration();
    const simtime_t preambleEnd = mode.getPreambleMode()->getDuration();
    const simtime_t headerEnd = preambleEnd + mode.getHeaderMode()->getDuration();
    append(resources, SIMTIME_ZERO, shortTrainingEnd, IRadioSignal::SIGNAL_PART_PREAMBLE);
    append(resources, shortTrainingEnd, preambleEnd, IRadioSignal::SIGNAL_PART_PREAMBLE);
    append(resources, preambleEnd, headerEnd, IRadioSignal::SIGNAL_PART_HEADER);
    const simtime_t symbolDuration = mode.getDataMode()->getSymbolInterval();
    const simtime_t dataEnd = headerEnd + mode.getDataMode()->getDuration(dataLength);
    for (simtime_t start = headerEnd; start < dataEnd; start += symbolDuration)
        append(resources, start, start + symbolDuration, IRadioSignal::SIGNAL_PART_DATA);
    return resources;
}

const IReceptionDecision *Ieee80211Receiver::computeReceptionDecision(
    const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part,
    const IInterference *interference, const ISnir *snir) const
{
    const auto spatialSnir = dynamic_cast<const ISpatialSnir *>(snir);
    const auto transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    if (spatialSnir == nullptr || transmission == nullptr)
        return FlatReceiverBase::computeReceptionDecision(listening, reception, part, interference, snir);

    const bool bandSupported = NarrowbandReceiverBase::computeIsReceptionPossible(listening, reception, part);
    const bool structural = bandSupported && (transmission->getHtPpduDescription() != nullptr ?
        supportsHtReception(transmission, part) : modeSet->containsMode(transmission->getMode()));
    const bool decodedSensitivity = structural &&
        spatialSnir->allRequiredOutputPowersMeet(part, sensitivity);
    const bool possible = structural && decodedSensitivity;
    const bool attempted = possible && computeIsReceptionAttemptedAfterPossibility(
        listening, reception, part, interference);
    const bool successful = attempted && computeIsReceptionSuccessful(
        listening, reception, part, interference, snir);
    return new ReceptionDecision(reception, part, possible, attempted, successful);
}

const IReceptionResult *Ieee80211Receiver::computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
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
