//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"

#include "inet/common/packet/chunk/BitCountChunk.h"

// IEEE 802.11 radio with HE MU/HE TB header handling.
//
// Encapsulates upper-layer packets with the appropriate IEEE 802.11 PHY header
// and decapsulates received packets.  For HE MU it collects per-user RU info,
// fills the HE MU PHY header (BSS color, PPDU format, Trigger ID, GI/coding,
// spatial reuse), computes the common PPDU duration, and exposes allocation
// reception facts via Ieee80211HeRxVectorInd and, for HE TB, separate
// Trigger-derived Ieee80211HeTbRecipientContextInd metadata.
// Relevant clauses:
//   - Clause 27.3.4: HE PPDU formats.
//   - Clause 27.3.11.7: HE-SIG-A fields.
//   - Clause 27.3.11.8: HE-SIG-B fields for HE MU PPDUs.
//
// Approximations / simplifications:
//   - DL MU user durations are first estimated with estimateHeMuUserDuration()
//     and later recomputed with computeHeUserPhyParameters(); the two paths may
//     differ slightly.
//   - HE MU PHY header length is estimated rather than bit-exact (see
//     Ieee80211Transmitter for the same approximation).
//   - MU-MIMO grouping uses the same simplified same-RU-index heuristic as the
//     transmitter; full validation is delegated to Ieee80211HePhyCalculator.
//   - FCS handling for HE MU relies on higher-layer bit-error flags rather than
//     a per-MPDU FCS field in the PHY header.
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Simsignals.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ErpOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211FhssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HrDsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211IrMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211EhtPhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtPreamblePuncturing.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtTxVector.h"

namespace inet {

namespace physicallayer {

simsignal_t IIeee80211CcaProvider::ccaStateChangedSignal = cComponent::registerSignal("ccaStateChanged");

Define_Module(Ieee80211Radio);

int Ieee80211Radio::getVhtAntennaCount() const
{
    if (antenna != nullptr)
        return antenna->getNumAntennas();
    auto antennaModule = getSubmodule("antenna");
    return antennaModule == nullptr ? 0 : antennaModule->par("numAntennas").intValue();
}

Hz Ieee80211Radio::getVhtChannelWidth() const
{
    if (transmitter != nullptr)
        return getChannelWidth();
    auto configuredBand = Ieee80211CompliantBands::getBand(par("bandName").stringValue());
    return Ieee80211ModeSet::getChannelWidth(configuredBand, Hz(par("bandwidth").doubleValue()));
}

simsignal_t Ieee80211Radio::radioChannelChangedSignal = cComponent::registerSignal("radioChannelChanged");
simsignal_t Ieee80211Radio::heRuIndexSignal = cComponent::registerSignal("heRuIndex");
simsignal_t Ieee80211Radio::heRuToneSizeSignal = cComponent::registerSignal("heRuToneSize");
simsignal_t Ieee80211Radio::heRuToneOffsetSignal = cComponent::registerSignal("heRuToneOffset");
simsignal_t Ieee80211Radio::heStaIdSignal = cComponent::registerSignal("heStaId");
simsignal_t Ieee80211Radio::heSpatialStreamsSignal = cComponent::registerSignal("heSpatialStreams");
simsignal_t Ieee80211Radio::heStreamStartIndexSignal = cComponent::registerSignal("heStreamStartIndex");
simsignal_t Ieee80211Radio::heScheduledPsduBytesSignal = cComponent::registerSignal("heScheduledPsduBytes");
simsignal_t Ieee80211Radio::heUserPpduDurationSignal = cComponent::registerSignal("heUserPpduDuration");
simsignal_t Ieee80211Radio::hePuncturedSubchannelMaskSignal = cComponent::registerSignal("hePuncturedSubchannelMask");
simsignal_t Ieee80211Radio::acknowledgmentFrameTypeSignal = cComponent::registerSignal("acknowledgmentFrameType");
simsignal_t Ieee80211Radio::acknowledgmentAirtimeSignal = cComponent::registerSignal("acknowledgmentAirtime");

static int resolveHePpduFormatForSerialization(const Packet *packet)
{
    if (auto indication = packet->findTag<Ieee80211HeRxVectorInd>()) {
        if (auto rxVector = indication->getRxVector())
            return rxVector->getCommon().getPpduFormat();
    }
    if (auto request = packet->findTag<Ieee80211HeTxVectorReq>()) {
        if (auto txVector = request->getTxVector())
            return txVector->getCommon().getParameters().ppduFormat;
    }
    auto front = packet->peekAtFront();
    if (dynamicPtrCast<const Ieee80211HeSuPhyHeader>(front)) return HE_SINGLE_USER;
    if (dynamicPtrCast<const Ieee80211HeErSuPhyHeader>(front)) return HE_EXTENDED_RANGE_SU;
    if (dynamicPtrCast<const Ieee80211HeMuPhyHeader>(front)) return HE_MU_DOWNLINK;
    if (dynamicPtrCast<const Ieee80211HeTbPhyHeader>(front)) return HE_TRIGGER_BASED_UPLINK;
    return -1;
}

static Ieee80211HeGuardInterval getHeGuardInterval(const Ieee80211HeMode *mode)
{
    switch (mode->getDataMode()->getGuardIntervalType()) {
        case Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT: return HE_GI_0_8_US;
        case Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM: return HE_GI_1_6_US;
        case Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG: return HE_GI_3_2_US;
        default: throw cRuntimeError("Unknown HE guard interval");
    }
}

static Ieee80211HePpduFormat getHePpduFormat(const Ieee80211HeMode *mode)
{
    switch (mode->getPreambleMode()->getPreambleFormat()) {
        case Ieee80211HePreambleMode::HE_PREAMBLE_SU:
            return HE_SINGLE_USER;
        case Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU:
            return HE_EXTENDED_RANGE_SU;
        case Ieee80211HePreambleMode::HE_PREAMBLE_MU:
            return HE_MU_DOWNLINK;
        default:
            throw cRuntimeError("Unknown HE preamble format");
    }
}

static Ieee80211HeOperatingBand getHeOperatingBand(const Ieee80211HeMode *mode)
{
    switch (mode->getCenterFrequencyMode()) {
        case Ieee80211HeMode::BAND_2_4GHZ: return Ieee80211HeOperatingBand::BAND_2_4_GHZ;
        case Ieee80211HeMode::BAND_5GHZ: return Ieee80211HeOperatingBand::BAND_5_GHZ;
        case Ieee80211HeMode::BAND_6GHZ: return Ieee80211HeOperatingBand::BAND_6_GHZ;
        default: throw cRuntimeError("Unknown HE operating band");
    }
}

static uint8_t encodeHeGiLtfSize(Ieee80211HeGuardInterval guardInterval, Ieee80211HeLtfType ltfType)
{
    if (ltfType == HE_LTF_4X && guardInterval == HE_GI_0_8_US) return 0;
    if (ltfType == HE_LTF_2X && guardInterval == HE_GI_0_8_US) return 1;
    if (ltfType == HE_LTF_2X && guardInterval == HE_GI_1_6_US) return 2;
    if (ltfType == HE_LTF_4X && guardInterval == HE_GI_3_2_US) return 3;
    throw cRuntimeError("HE GI/HE-LTF combination has no HE-SIG-A encoding");
}

static uint8_t encodeHeTxop(const Ieee80211HeSigAFields& sigA)
{
    if (sigA.txopUnspecified)
        return 127;
    return sigA.txopDurationUs < 512 ? (sigA.txopDurationUs / 8) << 1 :
            1 | ((sigA.txopDurationUs - 512) / 128) << 1;
}

template<typename Fields>
static Ieee80211HeSuErSignalingFields makeHeSuErSignalingFields(const Ieee80211HeLSig& lSig,
        const Fields& sigA)
{
    Ieee80211HeSuErSignalingFields fields;
    fields.signalingValid = true;
    fields.lSigLength = lSig.length;
    fields.beamChange = sigA.beamChange;
    fields.uplink = sigA.uplink;
    fields.mcs = sigA.mcs;
    fields.dcm = sigA.dcm;
    fields.bssColor = sigA.bssColor;
    fields.spatialReuse = sigA.spatialReuse;
    fields.bandwidth = sigA.bandwidth;
    fields.giLtfSize = sigA.giLtfSize;
    fields.numberOfSpaceTimeStreams = sigA.numberOfSpaceTimeStreams;
    fields.midamblePeriodicity = sigA.midamblePeriodicity;
    fields.txop = sigA.txop;
    fields.ldpcCoding = sigA.ldpcCoding;
    fields.ldpcExtraSymbolSegment = sigA.ldpcExtraSymbolSegment;
    fields.stbc = sigA.stbc;
    fields.beamformed = sigA.beamformed;
    fields.preFecPaddingFactor = sigA.preFecPaddingFactor;
    fields.peDisambiguity = sigA.peDisambiguity;
    fields.doppler = sigA.doppler;
    return fields;
}

static void populateHeSuErSignaling(const Ptr<Ieee80211HePhyHeader>& phyHeader,
        const Ieee80211HeMode *mode, const Ieee80211HePpduLayout& layout)
{
    auto ppduFormat = getIeee80211HePpduFormat(*phyHeader);
    const auto& common = layout.getCommon();
    if ((ppduFormat != HE_SINGLE_USER && ppduFormat != HE_EXTENDED_RANGE_SU) ||
            layout.getUsers().size() != 1)
        throw cRuntimeError("Canonical HE SU/ER layout has an invalid format or user count");
    const auto& user = layout.getUsers().front();

    Ieee80211HeSuErSigASemantics semantics;
    semantics.txTimeNs = layout.getDuration().inUnit(SIMTIME_NS);
    if (SimTime(semantics.txTimeNs, SIMTIME_NS) != layout.getDuration())
        throw cRuntimeError("HE SU/ER TXTIME is not representable as exact integer nanoseconds");
    semantics.operatingBand = getHeOperatingBand(mode);
    semantics.noSignalExtension = common.noSignalExtension;
    semantics.beamChange = false;
    semantics.uplink = common.sigA.uplink;
    semantics.mcs = user.mcs;
    semantics.dcmApplied = user.dcm;
    semantics.bssColor = common.sigA.bssColor;
    semantics.spatialReuse = common.sigA.spatialReuse.front();
    semantics.guardInterval = common.guardInterval;
    semantics.ltfType = common.ltfType;
    semantics.numberOfSpaceTimeStreams = user.numberOfSpatialStreams;
    semantics.stbcApplied = common.sigA.stbc;
    if (common.sigA.doppler) {
        if (common.sigA.midamblePeriodicity == 10)
            semantics.midamblePeriodicity = Ieee80211HeMidamblePeriodicity::SYMBOLS_10;
        else if (common.sigA.midamblePeriodicity == 20)
            semantics.midamblePeriodicity = Ieee80211HeMidamblePeriodicity::SYMBOLS_20;
        else
            throw cRuntimeError("HE SU/ER Doppler signaling requires a 10- or 20-symbol periodicity");
    }
    else if (common.sigA.midamblePeriodicity != 0)
        throw cRuntimeError("HE SU/ER midamble periodicity is present without Doppler signaling");
    semantics.txopDuration = Ieee80211HeTxopDuration{common.sigA.txopUnspecified,
            static_cast<uint16_t>(common.sigA.txopDurationUs)};
    Ieee80211HeFecOutcome fec;
    fec.coding = user.coding;
    if (user.coding == HE_CODING_LDPC)
        fec.ldpcExtraSymbolSegment = common.ldpcExtraSymbol;
    semantics.fec = fec;
    semantics.beamformed = false;
    semantics.preFecPaddingFactor = common.preFecPaddingFactor == 0 ?
            4 : common.preFecPaddingFactor;
    semantics.packetExtensionNs = common.packetExtensionDurationUs * 1000;

    if (ppduFormat == HE_SINGLE_USER) {
        auto bandwidth = common.channelBandwidth == MHz(20) ? Ieee80211HeSuBandwidth::MHZ_20 :
                common.channelBandwidth == MHz(40) ? Ieee80211HeSuBandwidth::MHZ_40 :
                common.channelBandwidth == MHz(80) ? Ieee80211HeSuBandwidth::MHZ_80 :
                common.channelBandwidth == MHz(160) ? Ieee80211HeSuBandwidth::MHZ_160 :
                Ieee80211HeSuBandwidth::UNKNOWN;
        if (bandwidth == Ieee80211HeSuBandwidth::UNKNOWN)
            throw cRuntimeError("Canonical HE SU bandwidth is not signalable");
        Ieee80211HeSuSignalingRequest signalingRequest;
        signalingRequest.common = semantics;
        signalingRequest.bandwidth = bandwidth;
        auto result = buildIeee80211HeSuSignaling(signalingRequest);
        if (!result)
            throw cRuntimeError("Invalid HE SU TXVECTOR signaling: %s", result.error.c_str());
        auto header = dynamicPtrCast<Ieee80211HeSuPhyHeader>(phyHeader);
        header->setSignaling(makeHeSuErSignalingFields(result.value.lSig, result.value.sigA));
    }
    else if (ppduFormat == HE_EXTENDED_RANGE_SU) {
        if (mode->getDataMode()->getBandwidth() != MHz(20))
            throw cRuntimeError("HE ER SU TXVECTOR requires a 20 MHz selected mode");
        Ieee80211HeErSuSignalingRequest signalingRequest;
        signalingRequest.common = semantics;
        signalingRequest.ruMode = user.ru.toneSize == 106 ?
                Ieee80211HeErSuRuMode::PRIMARY_UPPER_106_TONE :
                Ieee80211HeErSuRuMode::PRIMARY_242_TONE;
        auto result = buildIeee80211HeErSuSignaling(signalingRequest);
        if (!result)
            throw cRuntimeError("Invalid HE ER SU TXVECTOR signaling: %s", result.error.c_str());
        auto header = dynamicPtrCast<Ieee80211HeErSuPhyHeader>(phyHeader);
        header->setSignaling(makeHeSuErSignalingFields(result.value.lSig, result.value.sigA));
    }
    else
        throw cRuntimeError("HE SU/ER TXVECTOR is attached to a non-SU PPDU");
}

Ieee80211Radio::Ieee80211Radio() :
    FlatRadioBase()
{
}

const Ieee80211Channel *Ieee80211Radio::getChannel() const
{
    auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
    return ieee80211Transmitter->getChannel();
}

bool Ieee80211Radio::supportsParallelReception(const ITransmission *transmission) const
{
    auto packet = transmission == nullptr ? nullptr : transmission->getPacket();
    // HE TB PPDUs are trigger responses that may arrive concurrently on
    // distinct RUs; non-TB formats use the normal single-reception path.
    if (packet == nullptr || transmission->getPacketProtocol() != &Protocol::ieee80211HePhy ||
            !packet->hasAtFront<Ieee80211HePhyHeader>())
        return false;
    auto phyHeader = packet->peekAtFront<Ieee80211HePhyHeader>();
    return dynamicPtrCast<const Ieee80211HeTbPhyHeader>(phyHeader) != nullptr;
}

bool Ieee80211Radio::computeIsBandBusy(Hz centerFrequency) const
{
    const simtime_t now = simTime();
    const Coord& position = antenna->getMobility()->getCurrentPosition();
    BandListening listening(this, now, now + SimTime::fromRaw(1), position, position,
            centerFrequency, MHz(20));
    const IListeningDecision *decision = medium->listenOnMedium(this, &listening);
    bool busy = decision->isListeningPossible();
    delete decision;
    return busy;
}

void Ieee80211Radio::updateCcaState()
{
    auto ieee80211Receiver = dynamic_cast<const Ieee80211Receiver *>(receiver);
    auto channel = ieee80211Receiver == nullptr ? nullptr : ieee80211Receiver->getChannel();
    // IEEE Std 802.11-2024, 8.3.5.12/Table 8-5 and 27.3.22.6.5: for operating
    // channels wider than 20 MHz, preserve the CCA state of each 20 MHz
    // subchannel (the PHY-CCA.indication per20bitmap parameter).
    bool wideCca = channel != nullptr && channel->getNumSubchannels() > 1 && isReceiverMode(radioMode);
    int numSubchannels = 1;
    int primarySubchannelIndex = 0;
    uint32_t busySubchannelMask = 0;
    if (wideCca) {
        numSubchannels = channel->getNumSubchannels();
        primarySubchannelIndex = channel->getPrimarySubchannelIndex();
        for (int i = 0; i < numSubchannels; i++)
            if (computeIsBandBusy(channel->getSubchannelCenterFrequency(i)))
                busySubchannelMask |= 1u << i;
    }
    if (ccaSnapshot->getNumSubchannels() != numSubchannels ||
            ccaSnapshot->getPrimarySubchannelIndex() != primarySubchannelIndex ||
            ccaSnapshot->getBusySubchannelMask() != busySubchannelMask) {
        ccaSnapshot = std::make_unique<Ieee80211CcaSnapshot>(numSubchannels, primarySubchannelIndex, busySubchannelMask);
        emit(IIeee80211CcaProvider::ccaStateChangedSignal, ccaSnapshot.get());
    }
}

void Ieee80211Radio::validateChannelPlan(const IIeee80211Band *band, Hz channelWidth) const
{
    if (channelWidth > MHz(40) && band != nullptr && band->getBandFamily() != Ieee80211BandFamily::BAND_5_GHZ &&
            band->getBandFamily() != Ieee80211BandFamily::BAND_6_GHZ)
        throw cRuntimeError("IEEE 802.11 operation with a channel width greater than 40 MHz requires a 5 GHz or 6 GHz band");
    if (channelWidth == MHz(320) && (band == nullptr || band->getBandFamily() != Ieee80211BandFamily::BAND_6_GHZ))
        throw cRuntimeError("IEEE 802.11 320 MHz channel operation requires the 6 GHz band");
    if (htSecondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE && channelWidth < MHz(40))
        throw cRuntimeError("htSecondaryChannelOffset above/below requires at least 40 MHz operation");
}

Ieee80211SecondaryChannelOffset Ieee80211Radio::resolveSecondaryChannelOffset(const IIeee80211Band *band, Hz channelWidth, int channelNumber) const
{
    if (band != nullptr && band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS)
        return IEEE80211_SECONDARY_CHANNEL_NONE;
    if (htSecondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE || channelWidth < MHz(40))
        return htSecondaryChannelOffset;
    if (primary20SubchannelIndex >= 0)
        return (primary20SubchannelIndex & 1) == 0 ? IEEE80211_SECONDARY_CHANNEL_ABOVE : IEEE80211_SECONDARY_CHANNEL_BELOW;
    if (band != nullptr && band->getChannelTopology() == Ieee80211ChannelTopology::CONTIGUOUS &&
            band->getChannelWidth() == channelWidth) {
        int index = 0;
        int numSubchannels = (int)std::round(channelWidth.get<MHz>() / 20.0);
        if (numSubchannels == 16 && primary160ChannelPosition == Ieee80211Primary80ChannelPosition::UPPER)
            index += 8;
        if (numSubchannels >= 8 && primary80ChannelPosition == Ieee80211Primary80ChannelPosition::UPPER)
            index += 4;
        return (index & 1) == 0 ? IEEE80211_SECONDARY_CHANNEL_ABOVE : IEEE80211_SECONDARY_CHANNEL_BELOW;
    }
    // IEEE Std 802.11-2024, 21.2.2: for VHT/HE/EHT operation the secondary
    // 20 MHz channel is the other 20 MHz half of the primary 40 MHz channel,
    // so the offset follows from the primary channel position on the grid.
    if (band == nullptr || band->getSpacing() != MHz(5) ||
            (band->getBandFamily() != Ieee80211BandFamily::BAND_5_GHZ && band->getBandFamily() != Ieee80211BandFamily::BAND_6_GHZ))
        throw cRuntimeError("Cannot derive the secondary channel offset for %g MHz operation on band '%s'; set the htSecondaryChannelOffset parameter",
                channelWidth.get<MHz>(), band != nullptr ? band->getName() : "<unspecified>");
    Hz anchor = band->getCenterFrequency(band->getBandFamily() == Ieee80211BandFamily::BAND_5_GHZ ? 36 : 1);
    int index = (int)std::round((band->getCenterFrequency(channelNumber) - anchor).get<MHz>() / 20.0);
    return (index & 1) == 0 ? IEEE80211_SECONDARY_CHANNEL_ABOVE : IEEE80211_SECONDARY_CHANNEL_BELOW;
}

const Ieee80211Channel *Ieee80211Radio::createChannel(const IIeee80211Band *band, int channelNumber, Hz channelWidth) const
{
    auto secondaryChannelOffset = resolveSecondaryChannelOffset(band, channelWidth, channelNumber);
    return new Ieee80211Channel(band, channelNumber, secondaryChannelOffset, channelWidth,
            primary80ChannelPosition, primary160ChannelPosition, primary20SubchannelIndex);
}

void Ieee80211Radio::updateTransceiverState()
{
    FlatRadioBase::updateTransceiverState();
    updateCcaState();
}

void Ieee80211Radio::initialize(int stage)
{
    FlatRadioBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        const char *primary80 = par("primary80ChannelPosition");
        primary80ChannelPosition = !strcmp(primary80, "lower") ? Ieee80211Primary80ChannelPosition::LOWER :
                !strcmp(primary80, "upper") ? Ieee80211Primary80ChannelPosition::UPPER :
                Ieee80211Primary80ChannelPosition::UNSPECIFIED;
        const char *primary160 = par("primary160ChannelPosition");
        primary160ChannelPosition = !strcmp(primary160, "lower") ? Ieee80211Primary80ChannelPosition::LOWER :
                !strcmp(primary160, "upper") ? Ieee80211Primary80ChannelPosition::UPPER :
                Ieee80211Primary80ChannelPosition::UNSPECIFIED;
        primary20SubchannelIndex = par("primary20SubchannelIndex").intValue();
        if (primary20SubchannelIndex < -1 || primary20SubchannelIndex > 15)
            throw cRuntimeError("primary20SubchannelIndex must be -1 or between 0 and 15");
        const char *fcsModeString = par("fcsMode");
        fcsMode = parseFcsMode(fcsModeString, true);
        opMode = par("opMode").stringValue();
    }
    if (stage == INITSTAGE_PHYSICAL_LAYER) {
        const char *bandName = par("bandName");
        setBand(*bandName ? Ieee80211CompliantBands::getBand(bandName) : nullptr);
        setModeSet(*opMode.c_str() ? Ieee80211ModeSet::getModeSet(opMode.c_str(), band) : nullptr);
        htSecondaryChannelOffset = Ieee80211Channel::parseSecondaryChannelOffset(par("htSecondaryChannelOffset"));
        Hz channelWidth = getChannelWidth();
        // IEEE Std 802.11-2024, 19.2.3 (HT), 21.2.2 (VHT), 27.2.2 (HE): the
        // secondary 20 MHz channel offset is an operating-channel property of
        // 40 MHz and wider operation, not an HT40-only property.
        if (modeSet != nullptr && Ieee80211ModeSet::isHtProfileName(modeSet->getProfileName()) &&
                channelWidth == MHz(40) && htSecondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE)
            throw cRuntimeError("HT 40 MHz operation requires a secondary channel offset of above or below");
        validateChannelPlan(band, channelWidth);
        int channelNumber = par("channelNumber");
        if (channelNumber != -1)
            setChannelNumber(channelNumber);
    }
}

void Ieee80211Radio::handleUpperCommand(cMessage *message)
{
    bool configurationStarted = false;
    bool publishModeSet = false;
    try {
        if (message->getKind() == RADIO_C_CONFIGURE) {
            Ieee80211ConfigureRadioCommand *configureCommand = dynamic_cast<Ieee80211ConfigureRadioCommand *>(message->getControlInfo());
            if (configureCommand != nullptr) {
                const char *requestedOpMode = configureCommand->getOpMode();
                const Ieee80211ModeSet *requestedModeSet = configureCommand->getModeSet();
                const IIeee80211Band *requestedBand = configureCommand->getBand();
                const Ieee80211Channel *channel = configureCommand->getChannel();
                Hz newBandwidth = configureCommand->getBandwidth();
                auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
                Hz targetBandwidth = std::isnan(newBandwidth.get()) ? ieee80211Transmitter->getBandwidth() : newBandwidth;
                const IIeee80211Band *targetBand = channel != nullptr ? channel->getBand() :
                        requestedBand != nullptr ? requestedBand : this->band;
                std::string targetOpMode = *requestedOpMode ? requestedOpMode : this->opMode;
                const Ieee80211ModeSet *targetModeSet = requestedModeSet != nullptr ? requestedModeSet :
                        (*requestedOpMode || targetBand != this->band ? Ieee80211ModeSet::getModeSet(targetOpMode.c_str(), targetBand) : this->modeSet);
                if (targetModeSet != nullptr && targetModeSet->isBandAware())
                    targetModeSet->validateChannel(targetBand, targetBandwidth);
                const IIeee80211Mode *requestedMode = configureCommand->getMode();
                if (requestedMode != nullptr && (targetModeSet == nullptr || !targetModeSet->containsMode(requestedMode)))
                    throw cRuntimeError("The requested 802.11 mode is not part of the target mode profile");
                int newChannelNumber = configureCommand->getChannelNumber();
                const Ieee80211Channel *currentChannel = ieee80211Transmitter->getChannel();
                int targetChannelNumber = channel != nullptr ? channel->getChannelNumber() :
                        newChannelNumber != -1 ? newChannelNumber :
                        currentChannel != nullptr ? currentChannel->getChannelNumber() : -1;
                auto targetSecondaryChannelOffset = channel != nullptr ? channel->getSecondaryChannelOffset() :
                        htSecondaryChannelOffset;
                if (targetChannelNumber != -1 && (targetBand == nullptr || targetChannelNumber < 0 || targetChannelNumber >= targetBand->getNumChannels()))
                    throw cRuntimeError("Invalid target 802.11 channel number %d", targetChannelNumber);
                Hz targetChannelWidth = Ieee80211ModeSet::getChannelWidth(targetBand, targetBandwidth);
                if (targetChannelNumber != -1) {
                    validateChannelPlan(targetBand, targetChannelWidth);
                    if (targetModeSet != nullptr && Ieee80211ModeSet::isHtProfileName(targetModeSet->getProfileName()) &&
                            targetChannelWidth == MHz(40) && targetSecondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE)
                        throw cRuntimeError("HT 40 MHz operation requires a secondary channel offset of above or below");
                    targetSecondaryChannelOffset = targetSecondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE ?
                            targetSecondaryChannelOffset : resolveSecondaryChannelOffset(targetBand, targetChannelWidth, targetChannelNumber);
                    (void)Ieee80211Channel(targetBand, targetChannelNumber, targetSecondaryChannelOffset, targetChannelWidth,
                            primary80ChannelPosition, primary160ChannelPosition, primary20SubchannelIndex);
                }

                bps newBitrate = configureCommand->getBitrate();
                bps targetBitrate = std::isnan(newBitrate.get()) ? ieee80211Transmitter->getBitrate() : newBitrate;
                if (requestedMode != nullptr && !std::isnan(newBitrate.get()) &&
                        requestedMode->getDataMode()->getNetBitrate() != newBitrate)
                    throw cRuntimeError("The requested 802.11 mode and bitrate are inconsistent");
                Hz targetModeBandwidth = targetModeSet != nullptr ?
                        targetModeSet->getModeBandwidth(targetBand, targetBandwidth) : Hz(NaN);
                const IIeee80211Mode *resolvedMode = requestedMode;
                if (resolvedMode == nullptr && targetModeSet != nullptr)
                    resolvedMode = targetBitrate != bps(-1) ?
                            targetModeSet->getMode(targetBitrate, targetModeBandwidth) :
                            targetModeSet->getFastestMode(targetModeBandwidth);
                if (targetModeSet != nullptr && Ieee80211ModeSet::isHtProfileName(targetModeSet->getProfileName()) &&
                        ((targetModeBandwidth == MHz(40)) ||
                         (resolvedMode != nullptr && dynamic_cast<const Ieee80211HtMode *>(resolvedMode) != nullptr &&
                          resolvedMode->getDataMode()->getBandwidth() == MHz(40))) &&
                        targetSecondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE)
                    throw cRuntimeError("HT 40 MHz operation requires a secondary channel offset of above or below");

                publishModeSet = targetModeSet != this->modeSet || targetBand != this->band ||
                        !std::isnan(newBandwidth.get()) || !std::isnan(newBitrate.get()) || *requestedOpMode;
                configurationUpdateInProgress = true;
                configurationStarted = true;
                if (!std::isnan(newBandwidth.get()))
                    setBandwidth(newBandwidth);
                if (!std::isnan(newBitrate.get()))
                    setBitrate(newBitrate);
                if (targetChannelNumber != -1 &&
                        (currentChannel == nullptr || targetBand != this->band || targetChannelNumber != currentChannel->getChannelNumber() ||
                         targetSecondaryChannelOffset != currentChannel->getSecondaryChannelOffset() ||
                         targetChannelWidth != currentChannel->getOperatingChannelWidth()))
                    setChannel(new Ieee80211Channel(targetBand, targetChannelNumber, targetSecondaryChannelOffset, targetChannelWidth,
                            primary80ChannelPosition, primary160ChannelPosition, primary20SubchannelIndex));
                else if (targetBand != this->band)
                    setBand(targetBand);
                this->opMode = targetOpMode;
                if (requestedMode != nullptr)
                    setModeSetAndMode(targetModeSet, resolvedMode);
                else if (targetModeSet != this->modeSet || publishModeSet)
                    setModeSet(targetModeSet);
            }
        }
        FlatRadioBase::handleUpperCommand(message);
        if (configurationStarted)
            finishConfigurationUpdate(publishModeSet);
    }
    catch (...) {
        if (configurationStarted)
            cancelConfigurationUpdate();
        throw;
    }
}

void Ieee80211Radio::setModeSet(const Ieee80211ModeSet *modeSet)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    if (modeSet != nullptr && modeSet->isBandAware())
        modeSet->validateChannel(band, ieee80211Transmitter->getBandwidth());
    ieee80211Transmitter->setModeSet(modeSet);
    ieee80211Receiver->setModeSet(modeSet);
    this->modeSet = modeSet;
    EV << "Changing radio mode set to " << modeSet << endl;
    receptionTimer = nullptr;
    notifyListeningChanged();
}

void Ieee80211Radio::setModeSetAndMode(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    if (modeSet != nullptr && modeSet->isBandAware())
        modeSet->validateChannel(band, ieee80211Transmitter->getBandwidth());
    ieee80211Transmitter->setModeSetAndMode(modeSet, mode);
    ieee80211Receiver->setModeSet(modeSet);
    this->modeSet = modeSet;
    EV << "Changing radio mode set to " << modeSet << " with explicit mode " << mode << endl;
    receptionTimer = nullptr;
    notifyListeningChanged();
}

void Ieee80211Radio::setMode(const IIeee80211Mode *mode)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    ieee80211Transmitter->setMode(mode);
    EV << "Changing radio mode to " << mode << endl;
    receptionTimer = nullptr;
    notifyListeningChanged();
}

void Ieee80211Radio::setBand(const IIeee80211Band *band)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    ieee80211Transmitter->setBand(band);
    ieee80211Receiver->setBand(band);
    this->band = band;
    EV << "Changing radio band to " << band << endl;
    receptionTimer = nullptr;
    notifyListeningChanged();
}

void Ieee80211Radio::setChannel(const Ieee80211Channel *channel)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    ieee80211Transmitter->setChannel(channel);
    ieee80211Receiver->setChannel(new Ieee80211Channel(*channel));
    band = channel->getBand();
    htSecondaryChannelOffset = channel->getSecondaryChannelOffset();
    primary20SubchannelIndex = channel->getPrimary20SubchannelIndex();
    primary80ChannelPosition = channel->getPrimary80ChannelPosition();
    primary160ChannelPosition = channel->getPrimary160ChannelPosition();
    EV << "Changing radio channel to " << channel->getChannelNumber() << endl;
    receptionTimer = nullptr;
    notifyChannelChanged(channel->getChannelNumber());
}

void Ieee80211Radio::setChannelNumber(int newChannelNumber)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    Hz channelWidth = getChannelWidth();
    validateChannelPlan(band, channelWidth);
    ieee80211Transmitter->setChannel(createChannel(band, newChannelNumber, channelWidth));
    ieee80211Receiver->setChannel(createChannel(band, newChannelNumber, channelWidth));
    EV << "Changing radio channel to " << newChannelNumber << ".\n";
    receptionTimer = nullptr;
    notifyChannelChanged(newChannelNumber);
}

void Ieee80211Radio::notifyListeningChanged()
{
    if (configurationUpdateInProgress)
        listeningChangePending = true;
    else
        emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::notifyChannelChanged(int channelNumber)
{
    if (configurationUpdateInProgress) {
        channelChangePending = true;
        pendingChannelNumber = channelNumber;
        listeningChangePending = true;
    }
    else {
        emit(radioChannelChangedSignal, channelNumber);
        emit(listeningChangedSignal, 0);
    }
}

void Ieee80211Radio::finishConfigurationUpdate(bool publishModeSet)
{
    configurationUpdateInProgress = false;
    if (channelChangePending)
        emit(radioChannelChangedSignal, pendingChannelNumber);
    if (listeningChangePending)
        emit(listeningChangedSignal, 0);
    if (publishModeSet && modeSet != nullptr)
        emit(modesetChangedSignal, const_cast<Ieee80211ModeSet *>(modeSet));
    channelChangePending = false;
    listeningChangePending = false;
    pendingChannelNumber = -1;
}

void Ieee80211Radio::cancelConfigurationUpdate()
{
    configurationUpdateInProgress = false;
    channelChangePending = false;
    listeningChangePending = false;
    pendingChannelNumber = -1;
}

Hz Ieee80211Radio::getChannelWidth() const
{
    auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
    return Ieee80211ModeSet::getChannelWidth(band, ieee80211Transmitter->getBandwidth());
}

Hz Ieee80211Radio::getModeBandwidth() const
{
    auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
    return modeSet != nullptr ? modeSet->getModeBandwidth(band, ieee80211Transmitter->getBandwidth()) : Hz(NaN);
}

void Ieee80211Radio::insertFcs(const Ptr<Ieee80211PhyHeader>& phyHeader) const
{
    if (auto header = dynamic_cast<Ieee80211FhssPhyHeader *>(phyHeader.get())) {
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamic_cast<Ieee80211IrPhyHeader *>(phyHeader.get())) {
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamic_cast<Ieee80211DsssPhyHeader *>(phyHeader.get())) {
        // IEEE Std 802.11-2024 15.3.3.7 and 16.2.3.7 specify CRC-16 over the
        // SIGNAL/SERVICE/LENGTH fields for DSSS and HR/DSSS. The current
        // packet-level model records declared correctness instead of computing
        // the bit-level CRC.
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
}

bool Ieee80211Radio::verifyFcs(const Ptr<const Ieee80211PhyHeader>& phyHeader) const
{
    if (auto header = dynamicPtrCast<const Ieee80211FhssPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamicPtrCast<const Ieee80211IrPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamicPtrCast<const Ieee80211DsssPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else
        return true;
}

void Ieee80211Radio::encapsulate(Packet *packet) const
{
    auto self = const_cast<Ieee80211Radio *>(this);
    auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
    auto mode = ieee80211Transmitter->computeTransmissionMode(packet);
    if (packet->getDataLength() > b(0)) {
        auto frontChunk = packet->peekAtFront<Chunk>();
        if (auto macHeader = dynamicPtrCast<const ieee80211::Ieee80211MacHeader>(frontChunk)) {
            bool acknowledgment = dynamicPtrCast<const ieee80211::Ieee80211AckFrame>(macHeader) != nullptr ||
                    dynamicPtrCast<const ieee80211::Ieee80211BlockAckReq>(macHeader) != nullptr ||
                    dynamicPtrCast<const ieee80211::Ieee80211BlockAck>(macHeader) != nullptr;
            if (acknowledgment) {
                self->emit(acknowledgmentFrameTypeSignal, (long)macHeader->getType());
                self->emit(acknowledgmentAirtimeSignal,
                        SimTime(mode->getDuration(packet->getDataLength()).dbl()));
            }
        }
    }
    auto heMode = dynamic_cast<const Ieee80211HeMode *>(mode);
    auto handoff = packet->findTag<Ieee80211HeTxVectorReq>();
    Ptr<Ieee80211PhyHeader> phyHeader;
    if (handoff != nullptr && handoff->getTxVector() != nullptr)
        phyHeader = createIeee80211HePhyHeader(
                handoff->getTxVector()->getCommon().getParameters().ppduFormat);
    else if (heMode != nullptr)
        phyHeader = createIeee80211HePhyHeader(getHePpduFormat(heMode));
    else
        phyHeader = mode->getHeaderMode()->createHeader();
    if (auto hePhyHeader = dynamicPtrCast<Ieee80211HePhyHeader>(phyHeader)) {
        if (handoff == nullptr) {
            auto ppduFormat = getIeee80211HePpduFormat(*hePhyHeader);
            if (heMode == nullptr ||
                    (ppduFormat != HE_SINGLE_USER && ppduFormat != HE_EXTENDED_RANGE_SU))
                throw cRuntimeError("HE MU/TB transmissions require a producer-supplied canonical TXVECTOR");
            Ieee80211HeTxVectorRequest canonicalRequest;
            canonicalRequest.centerFrequency =
                    ieee80211Transmitter->computeTransmissionChannel(packet)->getCenterFrequency();
            canonicalRequest.channelBandwidth = mode->getDataMode()->getBandwidth();
            canonicalRequest.ppduFormat = ppduFormat;
            auto networkInterface = getContainingNicModule(this);
            auto mib = networkInterface == nullptr ? nullptr :
                    dynamic_cast<const ieee80211::Ieee80211Mib *>(
                            networkInterface->getSubmodule("mib"));
            canonicalRequest.bssColor = mib == nullptr ? 0 : mib->heOperation.bssColor;
            canonicalRequest.guardInterval = getHeGuardInterval(heMode);
            canonicalRequest.ltfType = getHeDefaultLtfType(canonicalRequest.guardInterval);
            Ieee80211HeUserTxVectorRequest user;
            user.ru = getHeEqualRuLayout(canonicalRequest.centerFrequency,
                    canonicalRequest.channelBandwidth, 1).front();
            user.mcs = heMode->getDataMode()->getMcsIndex();
            user.numberOfSpatialStreams = heMode->getDataMode()->getNumberOfSpatialStreams();
            user.coding = heMode->getDataMode()->isLdpc() ? HE_CODING_LDPC : HE_CODING_BCC;
            user.psduLength = B((packet->getDataLength().get<b>() + 7) / 8);
            canonicalRequest.users.push_back(user);
            auto canonicalResult = Ieee80211HeTxVectorFactory::create(canonicalRequest);
            if (!canonicalResult)
                throw cRuntimeError("Cannot construct canonical HE SU/ER TXVECTOR: %s (%s)",
                        canonicalResult.getContext().fieldName.c_str(),
                        canonicalResult.getContext().detail.c_str());
            auto mutableHandoff = packet->addTag<Ieee80211HeTxVectorReq>();
            mutableHandoff->setCanonicalPair(canonicalResult.getTxVector(),
                    canonicalResult.getPpduLayout());
            handoff = mutableHandoff;
        }
        const auto& txVector = handoff->getTxVector();
        const auto& ppduLayout = handoff->getPpduLayout();
        if (!txVector || !ppduLayout || !ppduLayout->matches(*txVector))
            throw cRuntimeError("Invalid canonical HE TXVECTOR/PPDU-layout handoff");
        const auto& psduBitRanges = ppduLayout->getPsduBitRanges();
        if ((ppduLayout->isNdp() && packet->getDataLength() != b(0)) ||
                (!ppduLayout->isNdp() &&
                 (psduBitRanges.empty() ||
                  psduBitRanges.back().getEndBitOffset() != packet->getDataLength())))
            throw cRuntimeError("Canonical HE PPDU-layout PSDU ranges disagree with the packet DATA container");
        const auto& common = ppduLayout->getCommon();
        const auto& canonicalUsers = ppduLayout->getUsers();
        auto ppduFormat = ppduLayout->getPpduFormat();
        hePhyHeader->setBssColor(common.sigA.bssColor);
        auto correlation = packet->findTag<Ieee80211HeTriggerCorrelationTag>();
        hePhyHeader->setTriggerId(correlation == nullptr ? 0 : correlation->getTriggerId());
        hePhyHeader->setNdp(ppduLayout->isNdp());
        hePhyHeader->setGuardInterval(ppduLayout->getGuardInterval());
        hePhyHeader->setCoding(canonicalUsers.empty() ? HE_CODING_BCC :
                canonicalUsers.front().coding);
        hePhyHeader->setPacketExtensionDurationUs(ppduLayout->getPacketExtensionDurationUs());
        hePhyHeader->setPuncturedSubchannelMask(common.puncturedSubchannelMask);
        hePhyHeader->setSpatialReuse(common.sigA.spatialReuse.front());
        self->emit(hePuncturedSubchannelMaskSignal,
                (long)common.puncturedSubchannelMask);
        simtime_t commonDuration = ppduLayout->getDuration();
        if (ppduFormat == HE_MU_DOWNLINK || ppduFormat == HE_TRIGGER_BASED_UPLINK) {
            for (const auto& canonicalUser : canonicalUsers) {
                Ieee80211HeMuUserInfo user;
                user.ruIndex = canonicalUser.ru.index;
                user.ruToneSize = canonicalUser.ru.toneSize;
                user.ruToneOffset = canonicalUser.ru.toneOffset;
                user.staId = canonicalUser.staId;
                user.mcs = canonicalUser.mcs;
                user.numberOfSpatialStreams = canonicalUser.numberOfSpatialStreams;
                user.streamStartIndex = canonicalUser.streamStartIndex;
                user.dcm = canonicalUser.dcm;
                user.coding = canonicalUser.coding;
                user.ndpFeedbackReport = canonicalUser.ndpFeedbackReport;
                user.ndpFeedbackStatus = canonicalUser.ndpFeedbackStatus;
                user.ndpRuToneSetIndex = canonicalUser.ndpRuToneSetIndex;
                user.ndpStartingStsNumber = canonicalUser.ndpStartingStsNumber;
                std::vector<const Ieee80211HeUserPhyParameters *> ruUsers;
                for (const auto& candidate : canonicalUsers)
                    if (candidate.ru.toneSize == canonicalUser.ru.toneSize &&
                            candidate.ru.toneOffset == canonicalUser.ru.toneOffset)
                        ruUsers.push_back(&candidate);
                if (ruUsers.size() > 1) {
                    std::sort(ruUsers.begin(), ruUsers.end(), [] (const auto *left, const auto *right) {
                        return left->streamStartIndex < right->streamStartIndex;
                    });
                    std::vector<int> nsts;
                    for (const auto *candidate : ruUsers)
                        nsts.push_back(candidate->numberOfSpatialStreams);
                    user.muMimo = true;
                    // HE-SIG-B Spatial Configuration (Table 27-31) exists
                    // only in an HE MU downlink PPDU. HE-TB uplink users get
                    // their NSS and starting stream directly from the Trigger
                    // User Info SS Allocation field.
                    if (ppduFormat == HE_MU_DOWNLINK)
                        user.spatialConfiguration = encodeHeMuSpatialConfiguration(nsts);
                }
                user.psduLength = canonicalUser.psduLength;
                user.duration = ppduLayout->getDuration();
                hePhyHeader->appendUsers(user);
                // These vectors are emitted in a fixed per-user order at the same
                // simulation time, so STA ID, RU, stream, PSDU size, and airtime
                // can be joined without relying on broadcast MAC addresses.
                self->emit(heRuIndexSignal, (long)user.ruIndex);
                self->emit(heRuToneSizeSignal, (long)user.ruToneSize);
                self->emit(heRuToneOffsetSignal, (long)user.ruToneOffset);
                self->emit(heStaIdSignal, (long)user.staId);
                self->emit(heSpatialStreamsSignal, (long)user.numberOfSpatialStreams);
                self->emit(heStreamStartIndexSignal, (long)user.streamStartIndex);
                self->emit(heScheduledPsduBytesSignal, (long)user.psduLength.get<B>());
                self->emit(heUserPpduDurationSignal, user.duration);
            }
        }
        std::map<int, std::vector<size_t>> ruUserIndices;
        for (size_t i = 0; i < canonicalUsers.size(); ++i) {
            ruUserIndices[canonicalUsers[i].ru.index].push_back(i);
        }
        bool isMuMimo = false;
        int maxTotalNsts = 0;
        for (const auto& user : canonicalUsers)
            maxTotalNsts = std::max(maxTotalNsts,
                    user.streamStartIndex + user.numberOfSpatialStreams);
        for (const auto& pair : ruUserIndices) {
            if (pair.second.size() > 1) {
                isMuMimo = true;
                int groupNsts = 0;
                for (size_t idx : pair.second) {
                    groupNsts += canonicalUsers[idx].numberOfSpatialStreams;
                }
                if (groupNsts > maxTotalNsts)
                    maxTotalNsts = groupNsts;
            }
        }
        hePhyHeader->setMuMimo(isMuMimo);
        hePhyHeader->setTotalNsts(maxTotalNsts);
        hePhyHeader->setCommonDuration(commonDuration);
        if (ppduFormat == HE_SINGLE_USER || ppduFormat == HE_EXTENDED_RANGE_SU) {
            if (heMode == nullptr)
                throw cRuntimeError("Canonical HE SU/ER transmission lacks a selected HE mode");
            populateHeSuErSignaling(hePhyHeader, heMode, *ppduLayout);
        }
        else if (ppduFormat == HE_MU_DOWNLINK) {
            auto header = dynamicPtrCast<Ieee80211HeMuPhyHeader>(phyHeader);
            Ieee80211HeMuSignalingFields signaling;
            std::vector<bool> punctured(std::lround(common.channelBandwidth.get() / 20e6), false);
            for (size_t i = 0; i < punctured.size(); ++i)
                punctured[i] = common.puncturedSubchannelMask & (uint8_t(1) << i);
            auto bandwidth = encodeHeMuBandwidth(common.channelBandwidth, punctured,
                    common.sigB.compression);
            if (!bandwidth)
                throw cRuntimeError("Cannot encode HE MU bandwidth: %s", bandwidth.error.c_str());
            signaling.signalingValid = true;
            signaling.lSigLength = common.lSigLength;
            signaling.heSigBMcs = common.sigB.mcs;
            signaling.bandwidth = bandwidth.value;
            signaling.heSigBCompression = common.sigB.compression;
            signaling.numberOfHeSigBSymbols = common.sigB.compression ? 0 : common.sigB.numberOfSymbols;
            signaling.numberOfHeSigBSymbolsIsSaturated = !common.sigB.compression && common.sigB.numberOfSymbols == 16;
            signaling.numberOfMuMimoUsers = common.sigB.compression ? canonicalUsers.size() : 0;
            signaling.giLtfSize = encodeHeGiLtfSize(common.guardInterval, common.ltfType);
            signaling.doppler = common.sigA.doppler;
            signaling.midamblePeriodicity = common.sigA.midamblePeriodicity;
            signaling.txop = encodeHeTxop(common.sigA);
            signaling.numberOfHeLtfSymbols = common.numberOfHeLtfSymbols;
            signaling.ldpcExtraSymbolSegment = common.ldpcExtraSymbol;
            signaling.preFecPaddingFactor = common.preFecPaddingFactor % 4;
            header->setSignaling(signaling);
        }
        else if (ppduFormat == HE_TRIGGER_BASED_UPLINK) {
            auto header = dynamicPtrCast<Ieee80211HeTbPhyHeader>(phyHeader);
            Ieee80211HeTbSignalingFields signaling;
            signaling.signalingValid = true;
            signaling.lSigLength = common.lSigLength;
            signaling.spatialReuse1 = common.sigA.spatialReuse[0];
            signaling.spatialReuse2 = common.sigA.spatialReuse[1];
            signaling.spatialReuse3 = common.sigA.spatialReuse[2];
            signaling.spatialReuse4 = common.sigA.spatialReuse[3];
            signaling.bandwidth = common.channelBandwidth == MHz(20) ? 0 :
                    common.channelBandwidth == MHz(40) ? 1 : common.channelBandwidth == MHz(80) ? 2 : 3;
            signaling.txop = encodeHeTxop(common.sigA);
            header->setSignaling(signaling);
        }
        int64_t totalBits = 100;
        if (auto muHeader = dynamicPtrCast<Ieee80211HeMuPhyHeader>(phyHeader)) {
            const auto& signaling = muHeader->getSignaling();
            static const int noDcm[] = {26, 52, 78, 104, 156, 208};
            static const int withDcm[] = {13, 26, 0, 52, 78, 0};
            int dataBitsPerSymbol = signaling.heSigBDcm ? withDcm[signaling.heSigBMcs] : noDcm[signaling.heSigBMcs];
            int numberOfSymbols = signaling.heSigBCompression ?
                    getHeSigBSymbolCount(common.channelBandwidth, signaling.numberOfMuMimoUsers,
                            true, signaling.heSigBMcs, signaling.heSigBDcm) :
                    signaling.numberOfHeSigBSymbols;
            totalBits += getHeSigBContentChannelCount(common.channelBandwidth) *
                    numberOfSymbols * dataBitsPerSymbol;
        }
        phyHeader->setChunkLength(b(totalBits));
    }
    else if (auto ehtPhyHeader = dynamicPtrCast<Ieee80211EhtPhyHeader>(phyHeader)) {
        auto ehtMode = dynamic_cast<const Ieee80211EhtMode *>(mode);
        if (ehtMode == nullptr)
            throw cRuntimeError("EHT PHY header requires an EHT transmission mode");
        auto dataMode = ehtMode->getDataMode();
        Ieee80211EhtUSigFields uSig;
        uSig.signalingValid = true;
        uSig.ppduTypeAndCompressionMode = 1; // Table 36-28: EHT SU in an EHT MU PPDU
        ehtPhyHeader->setUSig(uSig);
        ehtPhyHeader->setPpduFormat(EHT_MU);
        ehtPhyHeader->setGuardInterval(dataMode->getGuardIntervalType());
        ehtPhyHeader->setCoding(dataMode->isLdpc() ? HE_CODING_LDPC : HE_CODING_BCC);
        ehtPhyHeader->setCommonDuration(mode->getDuration(packet->getDataLength()));
        auto networkInterface = getContainingNicModule(this);
        auto mib = networkInterface == nullptr ? nullptr :
                dynamic_cast<const ieee80211::Ieee80211Mib *>(networkInterface->getSubmodule("mib"));
        uint16_t puncturedSubchannelMask = 0;
        if (mib != nullptr) {
            puncturedSubchannelMask = mib->ehtOperation.disabledSubchannelBitmap;
            if (puncturedSubchannelMask != 0 && !mib->localEhtCapabilities.preamblePuncturing)
                throw cRuntimeError("EHT puncturing is configured but the local EHT capability is disabled");
            if (puncturedSubchannelMask != 0 &&
                    mib->ehtOperation.operatingChannelWidth != dataMode->getBandwidth())
                throw cRuntimeError("EHT Disabled Subchannel Bitmap width does not match the selected EHT mode");
        }
        auto transmissionChannel = ieee80211Transmitter->computeTransmissionChannel(packet);
        int primary20SubchannelIndex = transmissionChannel->getPrimary20SubchannelIndex();
        if (!isValidIeee80211EhtPreamblePuncturing(puncturedSubchannelMask,
                dataMode->getBandwidth(), primary20SubchannelIndex,
                Ieee80211EhtPreamblePuncturingMode::NON_OFDMA))
            throw cRuntimeError("Illegal EHT non-OFDMA preamble puncturing mask 0x%04x for %g MHz channel",
                    puncturedSubchannelMask, dataMode->getBandwidth().get() / 1e6);
        ehtPhyHeader->setPuncturedSubchannelMask(puncturedSubchannelMask);
        ehtPhyHeader->setBssColor(mib == nullptr ? 0 : mib->heOperation.bssColor);
        Ieee80211EhtUserInfo user;
        user.mruIndex = 0;
        user.mruToneOffset = 0;
        Hz modeBandwidth = dataMode->getBandwidth();
        user.mruToneSize = modeBandwidth == MHz(20) ? 242 : modeBandwidth == MHz(40) ? 484 :
                modeBandwidth == MHz(80) ? 996 : modeBandwidth == MHz(160) ? 1992 : 3984;
        user.mcs = dataMode->getMcsIndex();
        user.numberOfSpatialStreams = dataMode->getNumberOfSpatialStreams();
        user.psduLength = B((packet->getDataLength().get<b>() + 7) / 8);
        user.duration = mode->getDuration(packet->getDataLength());
        ehtPhyHeader->appendUsers(user);
        if (user.mcs == 14 && (ehtMode->getCenterFrequencyMode() != Ieee80211EhtMode::BAND_6GHZ ||
                !dataMode->isLdpc() || user.numberOfSpatialStreams != 1 ||
                (modeBandwidth != MHz(80) && modeBandwidth != MHz(160) && modeBandwidth != MHz(320))))
            throw cRuntimeError("EHT-MCS 14 requires 6 GHz, LDPC, NSS=1, and 80/160/320 MHz full-channel EHT SU operation");
        phyHeader->setChunkLength(B(12));
    }
    else
        phyHeader->setChunkLength(b(mode->getHeaderMode()->getLength()));

    if (auto htPhyHeader = dynamicPtrCast<Ieee80211HtPhyHeader>(phyHeader)) {
        if (auto htMode = dynamic_cast<const Ieee80211HtMode *>(mode)) {
            if (htMode->getDataMode()->getCode()) {
                htPhyHeader->setCoding(htMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
        else if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(mode)) {
            if (heMode->getDataMode()->getCode()) {
                htPhyHeader->setCoding(heMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
        else if (auto ehtMode = dynamic_cast<const Ieee80211EhtMode *>(mode)) {
            if (ehtMode->getDataMode()->getCode()) {
                htPhyHeader->setCoding(ehtMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
    }
    else if (auto vhtPhyHeader = dynamicPtrCast<Ieee80211VhtPhyHeader>(phyHeader)) {
        if (auto vhtMode = dynamic_cast<const Ieee80211VhtMode *>(mode)) {
            auto vhtRequest = packet->findTag<Ieee80211VhtTransmissionTag>();
            auto vhtMuHandoff = packet->findTag<Ieee80211VhtTxVectorReq>();
            const bool ndp = packet->getDataLength() == b(0);
            auto channelWidth = vhtMode->getDataMode()->getBandwidth();
            if (vhtRequest != nullptr && vhtRequest->getNdp() && !ndp)
                throw cRuntimeError("VHT NDP request must have zero APEP/PSDU length");
            if (vhtMuHandoff != nullptr) {
                const auto& txVector = vhtMuHandoff->getTxVector();
                if (txVector == nullptr || !txVector->isMu() ||
                        txVector->getChannelWidth() != channelWidth ||
                        txVector->getPsduLength() != B(packet->getDataLength()) ||
                        txVector->getUsers().size() < 2 || txVector->getUsers().size() > 4 ||
                        txVector->getNumberOfSpaceTimeStreams() == 0 ||
                        txVector->getNumberOfSpaceTimeStreams() > 8)
                    throw cRuntimeError("Invalid canonical VHT MU TXVECTOR handoff");
                vhtPhyHeader->setChunkLength(getIeee80211VhtMuPhyHeaderLength(
                        channelWidth, txVector->getUsers().size()));
                vhtPhyHeader->setSignalingValid(true);
                vhtPhyHeader->setNdp(false);
                vhtPhyHeader->setStbc(false);
                vhtPhyHeader->setBandwidth(getIeee80211VhtBandwidthCode(channelWidth));
                vhtPhyHeader->setGroupId(txVector->getGroupId());
                vhtPhyHeader->setNumberOfSpaceTimeStreams(
                        txVector->getNumberOfSpaceTimeStreams());
                vhtPhyHeader->setPartialAid(0);
                vhtPhyHeader->setMcs(0);
                vhtPhyHeader->setCoding(0);
                vhtPhyHeader->setShortGi(txVector->hasShortGuardInterval());
                vhtPhyHeader->setLdpcExtraOfdmSymbol(txVector->hasLdpcExtraOfdmSymbol());
                vhtPhyHeader->setBeamformed(true);
                vhtPhyHeader->setCommonDuration(txVector->getCommonDuration());
                vhtPhyHeader->setUsersArraySize(txVector->getUsers().size());
                for (size_t i = 0; i < txVector->getUsers().size(); ++i) {
                    const auto& canonical = txVector->getUsers()[i];
                    Ieee80211VhtMuUserInfo user;
                    user.associationId = canonical.associationId;
                    user.userPosition = canonical.userPosition;
                    user.numberOfSpatialStreams = canonical.numberOfSpatialStreams;
                    user.mcs = canonical.mcs;
                    user.coding = canonical.ldpcCoding ? 1 : 0;
                    user.psduLength = canonical.psduLength;
                    user.duration = canonical.duration;
                    vhtPhyHeader->setUsers(i, user);
                }
            }
            else if (vhtRequest != nullptr && vhtRequest->getMuMimo())
                throw cRuntimeError("VHT MU-MIMO requires a canonical TXVECTOR handoff");
            if (vhtMuHandoff == nullptr) {
            vhtPhyHeader->setSignalingValid(true);
            vhtPhyHeader->setNdp(ndp);
            vhtPhyHeader->setStbc(false);
            vhtPhyHeader->setBandwidth(getIeee80211VhtBandwidthCode(channelWidth));
            vhtPhyHeader->setGroupId(vhtRequest == nullptr ? 63 :
                    vhtRequest->getGroupId());
            vhtPhyHeader->setNumberOfSpaceTimeStreams(
                    vhtMode->getDataMode()->getNumberOfSpatialStreams());
            vhtPhyHeader->setPartialAid(vhtRequest == nullptr ? 0 :
                    vhtRequest->getPartialAid());
            vhtPhyHeader->setMcs(vhtPhyHeader->getNdp() ? 0 :
                    vhtMode->getDataMode()->getMcsIndex());
            vhtPhyHeader->setShortGi(!vhtPhyHeader->getNdp() &&
                    vhtMode->getDataMode()->getGuardIntervalType() ==
                    Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT);
            vhtPhyHeader->setBeamformed(vhtRequest != nullptr &&
                    vhtRequest->getBeamformed());
            if (vhtMode->getDataMode()->getCode()) {
                vhtPhyHeader->setCoding(vhtPhyHeader->getNdp() ? 0 :
                        vhtMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
                vhtPhyHeader->setLdpcExtraOfdmSymbol(
                        !vhtPhyHeader->getNdp() &&
                        vhtMode->getDataMode()->getLdpcExtraOfdmSymbol(packet->getDataLength()));
            }
            if (vhtPhyHeader->getNdp() && packet->getDataLength() != b(0))
                throw cRuntimeError("VHT NDP must not contain a PSDU");
            if (vhtPhyHeader->getNdp() &&
                    vhtPhyHeader->getNumberOfSpaceTimeStreams() < 2)
                throw cRuntimeError("VHT NDP requires at least two space-time streams");
            }
        }
    }
    else if (auto ehtPhyHeader = dynamicPtrCast<Ieee80211EhtPhyHeader>(phyHeader)) {
        if (auto ehtMode = dynamic_cast<const Ieee80211EhtMode *>(mode))
            ehtPhyHeader->setCoding(ehtMode->getDataMode()->isLdpc() ? HE_CODING_LDPC : HE_CODING_BCC);
    }

    phyHeader->setLengthField(B((packet->getDataLength().get<b>() + 7) / 8));
    // The inherited lengthField stores TXVECTOR LENGTH in octets for duration
    // calculations. IEEE Std 802.11-2024 17.3.4.3 uses the same unit for
    // OFDM/ERP L-SIG, while 15.3.3.6 and 16.2.3.6 convert DSSS/HR-DSSS PLCP
    // LENGTH to microseconds; that conversion is not represented here.
    insertFcs(phyHeader);
    packet->insertAtFront(phyHeader);

    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    auto paddingLength = mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0)) {
        // IEEE Std 802.11-2024 17.3.5.3 and 17.3.5.4: non-HT OFDM DATA
        // materializes six tail bits plus pad bits. HT/VHT padding is accounted
        // for in airtime calculations, not as explicit packet trailer bits.
        const auto& phyTrailer = makeShared<BitCountChunk>(tailLength + paddingLength);
        packet->insertAtBack(phyTrailer);
    }
    const Protocol *protocol = nullptr;
    if (dynamic_cast<Ieee80211FhssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211FhssPhy;
    else if (dynamic_cast<Ieee80211IrPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211IrPhy;
    else if (dynamic_cast<Ieee80211DsssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211DsssPhy;
    else if (dynamic_cast<Ieee80211HrDsssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HrDsssPhy;
    else if (dynamic_cast<Ieee80211OfdmPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211OfdmPhy;
    else if (dynamic_cast<Ieee80211ErpOfdmPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211ErpOfdmPhy;
    else if (dynamic_cast<Ieee80211HtPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HtPhy;
    else if (dynamic_cast<Ieee80211VhtPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211VhtPhy;
    else if (dynamic_cast<Ieee80211HePhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HePhy;
    else if (dynamic_cast<Ieee80211EhtPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211EhtPhy;
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY header type.");
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(protocol);
}

void Ieee80211Radio::decapsulate(Packet *packet) const
{
    auto mode = packet->getTag<Ieee80211ModeInd>()->getMode();
    const auto& phyHeader = popIeee80211PhyHeaderAtFront(packet, b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
    if (phyHeader->isIncorrect() || phyHeader->isIncomplete() || phyHeader->isImproperlyRepresented() || !verifyFcs(phyHeader))
        packet->setBitError(true);

    if (!packet->hasBitError() && packet->getDataLength() == b(0)) {
        if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader);
                vhtHeader != nullptr && vhtHeader->getNdp()) {
            auto indication = packet->addTagIfAbsent<Ieee80211NdpInd>();
            indication->setPhyFormat(IEEE80211_NDP_PHY_VHT);
            indication->setChannelWidth(mode->getDataMode()->getBandwidth().get());
            indication->setNumberOfSpaceTimeStreams(vhtHeader->getNumberOfSpaceTimeStreams());
            indication->setNumberOfLtfSymbols(vhtHeader->getNumberOfSpaceTimeStreams());
            indication->setSounding(true);
        }
        else if (auto heHeader = dynamicPtrCast<const Ieee80211HePhyHeader>(phyHeader);
                heHeader != nullptr && heHeader->getNdp()) {
            auto indication = packet->addTagIfAbsent<Ieee80211NdpInd>();
            indication->setPhyFormat(IEEE80211_NDP_PHY_HE_SU);
            indication->setChannelWidth(mode->getDataMode()->getBandwidth().get());
            indication->setNumberOfSpaceTimeStreams(1);
            indication->setNumberOfLtfSymbols(1);
            indication->setSounding(true);
        }
    }

    if (auto indication = packet->findTagForUpdate<Ieee80211MpduReceiveInd>()) {
        for (unsigned int i = 0; i < indication->getResultsArraySize(); ++i) {
            auto result = indication->getResults(i);
            // A common/header/physical packet failure does not establish any
            // individual MPDU's FCS result. Preserve NOT_EVALUATED in that
            // case; only a structurally decoded packet may use the legacy
            // all-success fallback. HE MU/TB reception normally resolves all
            // valid MPDUs explicitly before reaching this boundary.
            if (result.status == MPDU_NOT_EVALUATED && !packet->hasBitError())
                result.status = MPDU_SUCCESS;
            indication->setResults(i, result);
        }
    }

    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    auto paddingLength = mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0))
        packet->popAtBack(tailLength + paddingLength, Chunk::PF_ALLOW_INCORRECT);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211Mac);
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::popIeee80211PhyHeaderAtFront(Packet *packet, b length, int flags)
{
    auto protocolTag = packet->findTag<PacketProtocolTag>();
    int id = protocolTag != nullptr && protocolTag->getProtocol() != nullptr ? protocolTag->getProtocol()->getId() : -1;
    if (id == Protocol::ieee80211FhssPhy.getId() || dynamicPtrCast<const Ieee80211FhssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId() || dynamicPtrCast<const Ieee80211IrPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId() || dynamicPtrCast<const Ieee80211DsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId() || dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId() || dynamicPtrCast<const Ieee80211OfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId() || dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId() || dynamicPtrCast<const Ieee80211HtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId() || dynamicPtrCast<const Ieee80211VhtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HePhy.getId() || dynamicPtrCast<const Ieee80211HePhyHeader>(packet->peekAtFront()) != nullptr) {
        switch (resolveHePpduFormatForSerialization(packet)) {
            case HE_SINGLE_USER: return packet->popAtFront<Ieee80211HeSuPhyHeader>(length, flags);
            case HE_EXTENDED_RANGE_SU: return packet->popAtFront<Ieee80211HeErSuPhyHeader>(length, flags);
            case HE_MU_DOWNLINK: return packet->popAtFront<Ieee80211HeMuPhyHeader>(length, flags);
            case HE_TRIGGER_BASED_UPLINK: return packet->popAtFront<Ieee80211HeTbPhyHeader>(length, flags);
            default: return packet->popAtFront<Ieee80211HePhyHeader>(length, flags);
        }
    }
    else if (id == Protocol::ieee80211EhtPhy.getId() || dynamicPtrCast<const Ieee80211EhtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211EhtPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::peekIeee80211PhyHeaderAtFront(const Packet *packet, b length, int flags)
{
    auto protocolTag = const_cast<Packet *>(packet)->findTag<PacketProtocolTag>();
    int id = protocolTag != nullptr && protocolTag->getProtocol() != nullptr ? protocolTag->getProtocol()->getId() : -1;
    if (id == Protocol::ieee80211FhssPhy.getId() || dynamicPtrCast<const Ieee80211FhssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId() || dynamicPtrCast<const Ieee80211IrPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId() || dynamicPtrCast<const Ieee80211DsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId() || dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId() || dynamicPtrCast<const Ieee80211OfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId() || dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId() || dynamicPtrCast<const Ieee80211HtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId() || dynamicPtrCast<const Ieee80211VhtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HePhy.getId() || dynamicPtrCast<const Ieee80211HePhyHeader>(packet->peekAtFront()) != nullptr) {
        switch (resolveHePpduFormatForSerialization(packet)) {
            case HE_SINGLE_USER: return packet->peekAtFront<Ieee80211HeSuPhyHeader>(length, flags);
            case HE_EXTENDED_RANGE_SU: return packet->peekAtFront<Ieee80211HeErSuPhyHeader>(length, flags);
            case HE_MU_DOWNLINK: return packet->peekAtFront<Ieee80211HeMuPhyHeader>(length, flags);
            case HE_TRIGGER_BASED_UPLINK: return packet->peekAtFront<Ieee80211HeTbPhyHeader>(length, flags);
            default: return packet->peekAtFront<Ieee80211HePhyHeader>(length, flags);
        }
    }
    else if (id == Protocol::ieee80211EhtPhy.getId() || dynamicPtrCast<const Ieee80211EhtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211EhtPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

} // namespace physicallayer

} // namespace inet
