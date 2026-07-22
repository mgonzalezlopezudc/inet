//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

#include "inet/common/packet/chunk/BitCountChunk.h"

// IEEE 802.11 radio with HE MU/HE TB header handling.
//
// Encapsulates upper-layer packets with the appropriate IEEE 802.11 PHY header
// and decapsulates received packets.  For HE MU it collects per-user RU info,
// fills the HE MU PHY header (BSS color, PPDU format, Trigger ID, GI/coding,
// spatial reuse), computes the common PPDU duration, and exposes allocation
// info on reception via Ieee80211HeMuRxTag.
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
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Radio);

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
    if (auto request = packet->findTag<Ieee80211HeMuReq>())
        return request->getPpduFormat();
    auto front = packet->peekAtFront();
    if (dynamicPtrCast<const Ieee80211HeSuPhyHeader>(front)) return HE_SINGLE_USER;
    if (dynamicPtrCast<const Ieee80211HeErSuPhyHeader>(front)) return HE_EXTENDED_RANGE_SU;
    if (dynamicPtrCast<const Ieee80211HeMuPhyHeader>(front)) return HE_MU_DOWNLINK;
    if (dynamicPtrCast<const Ieee80211HeTbPhyHeader>(front)) return HE_TRIGGER_BASED_UPLINK;
    return -1;
}

static std::vector<Ieee80211HeMuUserInfo> collectHeMuUsers(const Packet *packet)
{
    std::vector<Ieee80211HeMuUserInfo> users;
    if (auto request = packet->findTag<Ieee80211HeMuReq>()) {
        // A direct HE MU/TB request already contains the TXVECTOR-like user
        // parameters from Clause 27.3.11.7/27.3.11.8, so it is converted into
        // one HE-SIG-B User field model entry.
        Ieee80211HeMuUserInfo user;
        user.ruIndex = request->getRuIndex();
        user.ruToneSize = request->getRuToneSize();
        user.ruToneOffset = request->getRuToneOffset();
        user.staId = request->getStaId();
        user.mcs = request->getMcs();
        user.numberOfSpatialStreams = request->getNumberOfSpatialStreams();
        user.streamStartIndex = request->getStreamStartIndex();
        user.dcm = request->getDcm();
        user.ndpFeedbackReport = request->getNdpFeedbackReport();
        user.ndpFeedbackStatus = request->getNdpFeedbackStatus();
        user.ndpRuToneSetIndex = request->getNdpRuToneSetIndex();
        user.ndpStartingStsNumber = request->getNdpStartingStsNumber();
        user.psduLength = request->getPsduLength() != B(-1) ? request->getPsduLength() : B(packet->getDataLength());
        Ieee80211HeRu ru;
        ru.index = user.ruIndex;
        ru.toneSize = std::max<int>(user.ruToneSize, 26);
        ru.toneOffset = user.ruToneOffset;
        ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
        ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
        ru.bandwidth = Hz(ru.toneSize * 78125.0);
        user.duration = computeHeUserPhyParameters(user.psduLength, ru, user.mcs,
                user.numberOfSpatialStreams, user.dcm,
                static_cast<Ieee80211HeGuardInterval>(request->getGuardInterval()),
                static_cast<Ieee80211HeCoding>(request->getCoding())).duration;
        users.push_back(user);
        return users;
    }
    if (auto txTag = packet->findTag<Ieee80211HeMuTxTag>()) {
        // The tag is TXVECTOR-like local metadata. The packet data itself is
        // only the ordered concatenation of the standard per-user PSDUs.
        for (unsigned int i = 0; i < txTag->getAllocationsArraySize(); ++i) {
            const auto& allocation = txTag->getAllocations(i);
        Ieee80211HeMuUserInfo user;
            user.ruIndex = allocation.ruIndex;
            user.ruToneSize = allocation.ruToneSize;
            user.ruToneOffset = allocation.ruToneOffset;
            user.staId = allocation.staId;
            user.mcs = allocation.mcs;
            user.numberOfSpatialStreams = allocation.numberOfSpatialStreams;
            user.dcm = allocation.dcm;
            user.ndpFeedbackReport = allocation.ndpFeedbackReport;
            user.ndpFeedbackStatus = allocation.ndpFeedbackStatus;
            user.ndpRuToneSetIndex = allocation.ndpRuToneSetIndex;
            user.ndpStartingStsNumber = allocation.ndpStartingStsNumber;
            user.psduLength = allocation.psduLength;
            user.streamStartIndex = allocation.streamStartIndex;
            user.leakageSum = allocation.leakageSum;
        user.duration = estimateHeMuUserDuration(user.psduLength, user.ruToneSize, user.mcs);
        users.push_back(user);
        }
    }
    return users;
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

static Ieee80211HeSuBandwidth getHeSuBandwidth(uint8_t value)
{
    switch (value) {
        case 0: return Ieee80211HeSuBandwidth::MHZ_20;
        case 1: return Ieee80211HeSuBandwidth::MHZ_40;
        case 2: return Ieee80211HeSuBandwidth::MHZ_80;
        case 3: return Ieee80211HeSuBandwidth::MHZ_160;
        case 4: return Ieee80211HeSuBandwidth::MHZ_80P80;
        default: return Ieee80211HeSuBandwidth::UNKNOWN;
    }
}

static Hz getHeSuBandwidthValue(Ieee80211HeSuBandwidth bandwidth)
{
    switch (bandwidth) {
        case Ieee80211HeSuBandwidth::MHZ_20: return MHz(20);
        case Ieee80211HeSuBandwidth::MHZ_40: return MHz(40);
        case Ieee80211HeSuBandwidth::MHZ_80: return MHz(80);
        case Ieee80211HeSuBandwidth::MHZ_160: return MHz(160);
        default: return Hz(NaN);
    }
}

static Ieee80211HeErSuRuMode getHeErSuRuMode(uint8_t value)
{
    switch (value) {
        case 0: return Ieee80211HeErSuRuMode::PRIMARY_242_TONE;
        case 1: return Ieee80211HeErSuRuMode::PRIMARY_UPPER_106_TONE;
        default: return Ieee80211HeErSuRuMode::UNKNOWN;
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
        const Ieee80211HeMode *mode, const Ptr<const Ieee80211HeSuErTxVectorReq>& request,
        B packetLength, const Ieee80211HeCommonPhyParameters& common)
{
    if (!request->getComplete())
        throw cRuntimeError("Incomplete HE SU/ER TXVECTOR request");
    auto ppduFormat = getIeee80211HePpduFormat(*phyHeader);
    if (request->getPpduFormat() != ppduFormat || request->getPsduLength() != packetLength ||
            request->getMcs() != mode->getDataMode()->getMcsIndex() ||
            request->getNumberOfSpaceTimeStreams() != mode->getDataMode()->getNumberOfSpatialStreams() ||
            request->getGuardInterval() != getHeGuardInterval(mode) ||
            request->getCoding() != (mode->getDataMode()->isLdpc() ? HE_CODING_LDPC : HE_CODING_BCC))
        throw cRuntimeError("HE SU/ER TXVECTOR does not match the selected mode or PSDU");
    if (request->getTxTime() <= SIMTIME_ZERO || request->getTxTime() < mode->getPreambleMode()->getDuration())
        throw cRuntimeError("HE SU/ER TXVECTOR has an invalid total TXTIME");
    if (request->getBssColor() != phyHeader->getBssColor())
        throw cRuntimeError("HE SU/ER TXVECTOR BSS color does not match the active BSS");
    if (request->getNominalPacketPaddingDurationUs() != 0 &&
            request->getNominalPacketPaddingDurationUs() != 8 &&
            request->getNominalPacketPaddingDurationUs() != 16)
        throw cRuntimeError("HE SU/ER TXVECTOR has an invalid nominal packet padding duration");
    if (request->getPreFecPaddingFactor() != common.preFecPaddingFactor)
        throw cRuntimeError("HE SU/ER TXVECTOR Pre-FEC Padding Factor disagrees with the canonical calculation");
    if (request->getDcm() || request->getStbc())
        throw cRuntimeError("Ordinary HE SU/ER mode objects do not model DCM or STBC data processing");

    Ieee80211HeSuErSigASemantics semantics;
    semantics.txTimeNs = request->getTxTime().inUnit(SIMTIME_NS);
    if (SimTime(semantics.txTimeNs, SIMTIME_NS) != request->getTxTime())
        throw cRuntimeError("HE SU/ER TXTIME is not representable as exact integer nanoseconds");
    semantics.operatingBand = getHeOperatingBand(mode);
    semantics.noSignalExtension = request->getNoSignalExtension();
    semantics.beamChange = request->getBeamChange();
    semantics.uplink = request->getUplink();
    semantics.mcs = request->getMcs();
    semantics.dcmApplied = request->getDcm();
    semantics.bssColor = request->getBssColor();
    semantics.spatialReuse = request->getSpatialReuse();
    semantics.guardInterval = static_cast<Ieee80211HeGuardInterval>(request->getGuardInterval());
    semantics.ltfType = static_cast<Ieee80211HeLtfType>(request->getLtfType());
    semantics.numberOfSpaceTimeStreams = request->getNumberOfSpaceTimeStreams();
    semantics.stbcApplied = request->getStbc();
    if (request->getDoppler()) {
        if (request->getMidamblePeriodicity() == 10)
            semantics.midamblePeriodicity = Ieee80211HeMidamblePeriodicity::SYMBOLS_10;
        else if (request->getMidamblePeriodicity() == 20)
            semantics.midamblePeriodicity = Ieee80211HeMidamblePeriodicity::SYMBOLS_20;
        else
            throw cRuntimeError("HE SU/ER Doppler signaling requires a 10- or 20-symbol periodicity");
    }
    else if (request->getMidamblePeriodicity() != 0)
        throw cRuntimeError("HE SU/ER midamble periodicity is present without Doppler signaling");
    semantics.txopDuration = Ieee80211HeTxopDuration{request->getTxopUnspecified(), request->getTxopDurationUs()};
    Ieee80211HeFecOutcome fec;
    fec.coding = static_cast<Ieee80211HeCoding>(request->getCoding());
    if (request->getHasLdpcExtraSymbolSegment())
        fec.ldpcExtraSymbolSegment = request->getLdpcExtraSymbolSegment();
    semantics.fec = fec;
    semantics.beamformed = request->getBeamformed();
    semantics.preFecPaddingFactor = common.preFecPaddingFactor;
    semantics.packetExtensionNs = common.packetExtensionDurationUs * 1000;

    if (ppduFormat == HE_SINGLE_USER) {
        auto bandwidth = getHeSuBandwidth(request->getSuBandwidth());
        if (bandwidth == Ieee80211HeSuBandwidth::UNKNOWN ||
                getHeSuBandwidthValue(bandwidth) != mode->getDataMode()->getBandwidth())
            throw cRuntimeError("HE SU TXVECTOR bandwidth does not match the selected mode");
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
        signalingRequest.ruMode = getHeErSuRuMode(request->getErSuRuMode());
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

void Ieee80211Radio::initialize(int stage)
{
    FlatRadioBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        const char *fcsModeString = par("fcsMode");
        fcsMode = parseFcsMode(fcsModeString, true);
        opMode = par("opMode").stringValue();
    }
    if (stage == INITSTAGE_PHYSICAL_LAYER) {
        const char *bandName = par("bandName");
        setBand(*bandName ? Ieee80211CompliantBands::getBand(bandName) : nullptr);
        setModeSet(*opMode.c_str() ? Ieee80211ModeSet::getModeSet(opMode.c_str(), band) : nullptr);
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
                if (targetChannelNumber != -1 && (targetBand == nullptr || targetChannelNumber < 0 || targetChannelNumber >= targetBand->getNumChannels()))
                    throw cRuntimeError("Invalid target 802.11 channel number %d", targetChannelNumber);
                if (targetChannelNumber != -1)
                    (void)Ieee80211Channel(targetBand, targetChannelNumber).getCenterFrequency();

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

                publishModeSet = targetModeSet != this->modeSet || targetBand != this->band ||
                        !std::isnan(newBandwidth.get()) || !std::isnan(newBitrate.get()) || *requestedOpMode;
                configurationUpdateInProgress = true;
                configurationStarted = true;
                if (!std::isnan(newBandwidth.get()))
                    setBandwidth(newBandwidth);
                if (!std::isnan(newBitrate.get()))
                    setBitrate(newBitrate);
                if (targetChannelNumber != -1 &&
                        (currentChannel == nullptr || targetBand != this->band || targetChannelNumber != currentChannel->getChannelNumber()))
                    setChannel(new Ieee80211Channel(targetBand, targetChannelNumber));
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
    ieee80211Receiver->setChannel(new Ieee80211Channel(channel->getBand(), channel->getChannelNumber()));
    band = channel->getBand();
    EV << "Changing radio channel to " << channel->getChannelNumber() << endl;
    receptionTimer = nullptr;
    notifyChannelChanged(channel->getChannelNumber());
}

void Ieee80211Radio::setChannelNumber(int newChannelNumber)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    ieee80211Transmitter->setChannelNumber(newChannelNumber);
    ieee80211Receiver->setChannelNumber(newChannelNumber);
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
    auto heMuUsers = collectHeMuUsers(packet);
    if (!heMuUsers.empty()) {
        auto request = packet->findTag<Ieee80211HeMuReq>();
        auto commonRequest = packet->findTag<Ieee80211HeMuCommonReq>();
        self->emit(hePuncturedSubchannelMaskSignal, (long)(request != nullptr ? request->getPuncturedSubchannelMask() :
                commonRequest != nullptr ? commonRequest->getPuncturedSubchannelMask() : 0));
    }
    // An HE TB NDP (preamble-only) has no PSDU and therefore no MAC header chunk.
    // Guard the peek so it is not attempted on an empty packet.
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
    auto request = packet->findTag<Ieee80211HeMuReq>();
    auto txTag = packet->findTag<Ieee80211HeMuTxTag>();
    const bool soundingNdp = request == nullptr && txTag != nullptr && txTag->getNdp();
    auto heMode = dynamic_cast<const Ieee80211HeMode *>(mode);
    Ptr<Ieee80211PhyHeader> phyHeader;
    if (request != nullptr)
        phyHeader = createIeee80211HePhyHeader(static_cast<Ieee80211HePpduFormat>(request->getPpduFormat()));
    else if (soundingNdp)
        // Clause 27.3.17 defines an HE sounding NDP as an HE SU PPDU.
        // The preceding NDPA, not HE-SIG-B user fields, identifies beamformees.
        phyHeader = createIeee80211HePhyHeader(HE_SINGLE_USER);
    else if (!heMuUsers.empty())
        phyHeader = createIeee80211HePhyHeader(HE_MU_DOWNLINK);
    else if (heMode != nullptr)
        phyHeader = createIeee80211HePhyHeader(getHePpduFormat(heMode));
    else
        phyHeader = mode->getHeaderMode()->createHeader();
    if (auto hePhyHeader = dynamicPtrCast<Ieee80211HePhyHeader>(phyHeader)) {
        auto suErRequest = packet->findTag<Ieee80211HeSuErTxVectorReq>();
        auto networkInterface = getContainingNicModule(this);
        auto mib = networkInterface ? dynamic_cast<const ieee80211::Ieee80211Mib *>(networkInterface->getSubmodule("mib")) : nullptr;
        if (mib != nullptr)
            hePhyHeader->setBssColor(mib->heOperation.bssColor);
        hePhyHeader->setTriggerId(request == nullptr ? 0 : request->getTriggerId());
        auto commonRequest = packet->findTag<Ieee80211HeMuCommonReq>();
        hePhyHeader->setGuardInterval(request != nullptr ? request->getGuardInterval() :
                commonRequest != nullptr ? commonRequest->getGuardInterval() :
                heMode != nullptr ? getHeGuardInterval(heMode) : HE_GI_3_2_US);
        hePhyHeader->setCoding(request != nullptr ? request->getCoding() :
                commonRequest != nullptr ? commonRequest->getCoding() :
                heMode != nullptr && heMode->getDataMode()->isLdpc() ? HE_CODING_LDPC : HE_CODING_BCC);
        hePhyHeader->setPacketExtensionDurationUs(suErRequest != nullptr ? suErRequest->getPacketExtensionDurationUs() :
                soundingNdp ? 4 :
                request != nullptr ? request->getPacketExtensionDurationUs() :
                commonRequest != nullptr ? commonRequest->getPacketExtensionDurationUs() : 0);
        hePhyHeader->setPuncturedSubchannelMask(request != nullptr ? request->getPuncturedSubchannelMask() :
                commonRequest != nullptr ? commonRequest->getPuncturedSubchannelMask() : 0);
        hePhyHeader->setSpatialReuse(soundingNdp ? 15 :
                suErRequest != nullptr ? suErRequest->getSpatialReuse() :
                request != nullptr ? request->getSpatialReuse() :
                commonRequest != nullptr ? commonRequest->getSpatialReuse() : 0);
        hePhyHeader->setNonSrgObssPdDisallowed(request != nullptr ? request->getNonSrgObssPdDisallowed() :
                commonRequest != nullptr ? commonRequest->getNonSrgObssPdDisallowed() : false);
        hePhyHeader->setSrgObssPdDisallowed(request != nullptr ? request->getSrgObssPdDisallowed() :
                commonRequest != nullptr ? commonRequest->getSrgObssPdDisallowed() : false);
        hePhyHeader->setPsrDisallowed(request != nullptr ? request->getPsrDisallowed() :
                commonRequest != nullptr ? commonRequest->getPsrDisallowed() : false);
        auto ppduFormat = getIeee80211HePpduFormat(*hePhyHeader);
        auto guardInterval = static_cast<Ieee80211HeGuardInterval>(hePhyHeader->getGuardInterval());
        Ieee80211HeTxVectorRequest canonicalRequest;
        canonicalRequest.centerFrequency = ieee80211Transmitter->computeTransmissionChannel(packet)->getCenterFrequency();
        canonicalRequest.ppduFormat = ppduFormat;
        canonicalRequest.channelBandwidth = ppduFormat == HE_TRIGGER_BASED_UPLINK && request != nullptr ?
                Hz(request->getChannelBandwidthMhz() * 1e6) : mode->getDataMode()->getBandwidth();
        canonicalRequest.puncturedSubchannelMask = hePhyHeader->getPuncturedSubchannelMask();
        canonicalRequest.lSigLength = request != nullptr ? request->getLSigLength() : 0;
        canonicalRequest.noSignalExtension = request != nullptr ? request->getNoSignalExtension() :
                suErRequest != nullptr && suErRequest->getNoSignalExtension();
        canonicalRequest.requestedTxTime = request != nullptr ? request->getCommonDuration() : SIMTIME_ZERO;
        canonicalRequest.requestedTxTimeExact = request != nullptr && request->getCommonDurationExact();
        canonicalRequest.ldpcExtraSymbolSegment = request != nullptr && request->getLdpcExtraSymbolSegment();
        canonicalRequest.preFecPaddingFactor = request != nullptr ? request->getPreFecPaddingFactor() : 0;
        canonicalRequest.peDisambiguity = request != nullptr && request->getPeDisambiguity();
        canonicalRequest.numberOfHeLtfSymbols = request != nullptr ? request->getNumberOfHeLtfSymbols() : 0;
        canonicalRequest.triggerMethod = request != nullptr ?
                static_cast<Ieee80211HeTriggerMethod>(request->getTriggerMethod()) :
                Ieee80211HeTriggerMethod::NONE;
        canonicalRequest.ndp = txTag != nullptr && txTag->getNdp();
        canonicalRequest.bssColor = hePhyHeader->getBssColor();
        canonicalRequest.uplink = suErRequest != nullptr ? suErRequest->getUplink() :
                ppduFormat == HE_TRIGGER_BASED_UPLINK;
        canonicalRequest.txopDuration = suErRequest != nullptr ?
                Ieee80211HeTxopDuration{suErRequest->getTxopUnspecified(),
                        suErRequest->getTxopDurationUs()} :
                request != nullptr ? Ieee80211HeTxopDuration{request->getTxopUnspecified(), request->getTxopDurationUs()} :
                commonRequest != nullptr ? Ieee80211HeTxopDuration{commonRequest->getTxopUnspecified(), commonRequest->getTxopDurationUs()} :
                Ieee80211HeTxopDuration{};
        canonicalRequest.doppler = suErRequest != nullptr ? suErRequest->getDoppler() :
                request != nullptr ? request->getDoppler() :
                commonRequest != nullptr && commonRequest->getDoppler();
        canonicalRequest.midamblePeriodicity = suErRequest != nullptr ? suErRequest->getMidamblePeriodicity() :
                request != nullptr ? request->getMidamblePeriodicity() :
                commonRequest != nullptr ? commonRequest->getMidamblePeriodicity() : 0;
        if (suErRequest != nullptr && suErRequest->getStbc())
            throw cRuntimeError("Ordinary HE SU/ER mode objects do not model STBC data processing");
        canonicalRequest.guardInterval = guardInterval;
        canonicalRequest.ltfType = suErRequest != nullptr ?
                static_cast<Ieee80211HeLtfType>(suErRequest->getLtfType()) :
                ppduFormat == HE_TRIGGER_BASED_UPLINK && request != nullptr ?
                static_cast<Ieee80211HeLtfType>(request->getLtfType()) :
                getHeDefaultLtfType(guardInterval);
        canonicalRequest.packetExtensionDurationUs = hePhyHeader->getPacketExtensionDurationUs();
        if (soundingNdp) {
            Ieee80211HeUserTxVectorRequest user;
            user.ru = getHeEqualRuLayout(canonicalRequest.centerFrequency,
                    canonicalRequest.channelBandwidth, 1).front();
            user.mcs = 0;
            // All beamformees observe the same HE SU sounding NDP. NUM_STS is
            // the largest requested sounding dimension, not a sum across the
            // target list carried by the preceding NDPA.
            user.numberOfSpatialStreams = 2;
            for (const auto& target : heMuUsers)
                user.numberOfSpatialStreams = std::max(user.numberOfSpatialStreams,
                        static_cast<int>(target.numberOfSpatialStreams));
            user.dcm = false;
            user.coding = static_cast<Ieee80211HeCoding>(hePhyHeader->getCoding());
            user.psduLength = B(0);
            canonicalRequest.users.push_back(user);
        }
        else if (heMuUsers.empty() && (ppduFormat == HE_SINGLE_USER || ppduFormat == HE_EXTENDED_RANGE_SU)) {
            Ieee80211HeUserTxVectorRequest user;
            if (ppduFormat == HE_EXTENDED_RANGE_SU && suErRequest != nullptr &&
                    suErRequest->getErSuRuMode() == 1) {
                auto catalog = getHeRuAllocationCatalog(canonicalRequest.centerFrequency,
                        canonicalRequest.channelBandwidth);
                auto ru = std::max_element(catalog.begin(), catalog.end(),
                        [] (const auto& left, const auto& right) {
                            const bool left106 = left.toneSize == 106;
                            const bool right106 = right.toneSize == 106;
                            return left106 != right106 ? !left106 : left.toneOffset < right.toneOffset;
                        });
                if (ru == catalog.end() || ru->toneSize != 106)
                    throw cRuntimeError("Cannot resolve the requested HE ER SU 106-tone RU");
                user.ru = *ru;
            }
            else
                user.ru = getHeEqualRuLayout(canonicalRequest.centerFrequency,
                        canonicalRequest.channelBandwidth, 1).front();
            user.mcs = heMode->getDataMode()->getMcsIndex();
            user.numberOfSpatialStreams = heMode->getDataMode()->getNumberOfSpatialStreams();
            user.dcm = suErRequest != nullptr && suErRequest->getDcm();
            user.coding = heMode->getDataMode()->isLdpc() ? HE_CODING_LDPC : HE_CODING_BCC;
            user.psduLength = B((packet->getDataLength().get<b>() + 7) / 8);
            user.nominalPacketPaddingDurationUs = suErRequest != nullptr ?
                    suErRequest->getNominalPacketPaddingDurationUs() : 0;
            canonicalRequest.users.push_back(user);
        }
        else {
            for (const auto& modelUser : heMuUsers) {
                Ieee80211HeUserTxVectorRequest user;
                user.ru.index = modelUser.ruIndex;
                user.ru.toneSize = std::max<int>(modelUser.ruToneSize, 26);
                user.ru.toneOffset = modelUser.ruToneOffset;
                user.mcs = modelUser.mcs;
                user.numberOfSpatialStreams = modelUser.numberOfSpatialStreams;
                user.streamStartIndex = modelUser.streamStartIndex;
                user.dcm = modelUser.dcm;
                user.ndpFeedbackReport = modelUser.ndpFeedbackReport;
                user.ndpFeedbackStatus = modelUser.ndpFeedbackStatus;
                user.ndpRuToneSetIndex = modelUser.ndpRuToneSetIndex;
                user.ndpStartingStsNumber = modelUser.ndpStartingStsNumber;
                user.coding = static_cast<Ieee80211HeCoding>(hePhyHeader->getCoding());
                user.psduLength = modelUser.psduLength;
                user.nominalPacketPaddingDurationUs = request != nullptr ?
                        request->getNominalPacketPaddingDurationUs() :
                        commonRequest != nullptr ? commonRequest->getNominalPacketPaddingDurationUs() : 0;
                user.staId = modelUser.staId;
                canonicalRequest.users.push_back(user);
            }
        }
        auto canonicalResult = Ieee80211HeTxVectorFactory::create(canonicalRequest);
        if (!canonicalResult)
            throw cRuntimeError("Cannot construct canonical HE TXVECTOR: %s (%s)",
                    canonicalResult.getContext().fieldName.c_str(), canonicalResult.getContext().detail.c_str());
        const auto& txVector = canonicalResult.getTxVector();
        const auto& ppduLayout = canonicalResult.getPpduLayout();
        const auto& common = ppduLayout->getCommon();
        if (txTag != nullptr && txTag->getNdp() != ppduLayout->isNdp())
            throw cRuntimeError("Explicit HE NDP state disagrees with the canonical TXVECTOR");
        auto handoff = packet->addTag<Ieee80211HeTxVectorReq>();
        handoff->setCanonicalPair(txVector, ppduLayout);
        const auto& canonicalUsers = ppduLayout->getUsers();
        hePhyHeader->setNdp(ppduLayout->isNdp());
        hePhyHeader->setGuardInterval(ppduLayout->getGuardInterval());
        hePhyHeader->setPacketExtensionDurationUs(ppduLayout->getPacketExtensionDurationUs());
        simtime_t commonDuration = ppduLayout->getDuration();
        if (request != nullptr && request->getPpduFormat() == HE_MU_DOWNLINK &&
                request->getCommonDuration() > SIMTIME_ZERO &&
                request->getCommonDuration() != commonDuration)
            throw cRuntimeError("Planned HE MU PPDU duration does not match the resolved PHY parameters");
        if (suErRequest != nullptr) {
            if ((ppduFormat != HE_SINGLE_USER && ppduFormat != HE_EXTENDED_RANGE_SU) || heMode == nullptr)
                throw cRuntimeError("HE SU/ER TXVECTOR is attached to a non-SU transmission");
            populateHeSuErSignaling(hePhyHeader, heMode, suErRequest,
                    B((packet->getDataLength().get<b>() + 7) / 8), common);
            if (suErRequest->getTxTime() != commonDuration)
                throw cRuntimeError("HE SU/ER TXTIME disagrees with the canonical PPDU layout");
        }
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
        // Detect MU-MIMO
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
        if (request != nullptr && request->getMuMimo()) {
            isMuMimo = true;
            hePhyHeader->setMuMimo(true);
            maxTotalNsts = request->getTotalNsts();
        }
        if (txTag != nullptr) {
            for (unsigned int i = 0; i < txTag->getAllocationsArraySize(); ++i) {
                const auto& allocation = txTag->getAllocations(i);
                if (allocation.muMimo) {
                    isMuMimo = true;
                    hePhyHeader->setMuMimo(true);
                    maxTotalNsts = std::max(maxTotalNsts, (int)allocation.totalNsts);
                    hePhyHeader->setSpatialConfiguration(allocation.spatialConfiguration);
                }
            }
        }
        hePhyHeader->setTotalNsts(maxTotalNsts);
        hePhyHeader->setCommonDuration(commonDuration);
        if (ppduFormat == HE_MU_DOWNLINK) {
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
            if (request == nullptr)
                throw cRuntimeError("HE TB signaling requires Trigger-derived request context");
            auto header = dynamicPtrCast<Ieee80211HeTbPhyHeader>(phyHeader);
            Ieee80211HeTbSignalingFields signaling;
            signaling.signalingValid = true;
            signaling.lSigLength = common.lSigLength;
            signaling.spatialReuse1 = signaling.spatialReuse2 =
                    signaling.spatialReuse3 = signaling.spatialReuse4 = hePhyHeader->getSpatialReuse();
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
            if (vhtMode->getDataMode()->getCode()) {
                vhtPhyHeader->setCoding(vhtMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
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

    if (auto hePhyHeader = dynamicPtrCast<const Ieee80211HePhyHeader>(phyHeader)) {
        if (auto decoded = packet->findTag<Ieee80211HeRxVectorInd>()) {
            auto rxVector = decoded->getRxVector();
            if (!rxVector)
                throw cRuntimeError("Empty decoded HE RXVECTOR indication");
            const auto& common = rxVector->getCommon();
            const auto& user = rxVector->getUser();
            auto tag = packet->addTagIfAbsent<Ieee80211HeMuRxTag>();
            tag->setPpduFormat(common.getPpduFormat());
            tag->setTriggerId(hePhyHeader->getTriggerId());
            tag->setGuardInterval(common.getGuardInterval());
            tag->setPacketExtensionDurationUs(hePhyHeader->getPacketExtensionDurationUs());
            tag->setPuncturedSubchannelMask(hePhyHeader->getPuncturedSubchannelMask());
            tag->setPuncturedSubchannelMaskKnown(hePhyHeader->getPuncturedSubchannelMaskKnown());
            tag->setRuIndex(-1);
            if (common.getPpduFormat() == HE_TRIGGER_BASED_UPLINK) {
                auto context = packet->findTag<Ieee80211HeTbRecipientContextInd>();
                if (context == nullptr || !context->getRecipientParameters())
                    throw cRuntimeError("HE TB reception is missing its Trigger recipient context");
                const auto& parameters = *context->getRecipientParameters();
                tag->setCoding(parameters.coding);
                Ieee80211HeMuRxAllocationInfo info;
                info.ruIndex = parameters.ru.index;
                info.staId = parameters.staId;
                info.ruToneSize = parameters.ru.toneSize;
                info.ruToneOffset = parameters.ru.toneOffset;
                info.mcs = parameters.mcs;
                info.numberOfSpatialStreams = parameters.numberOfSpatialStreams;
                info.dcm = parameters.dcm;
                info.ndpFeedbackReport = parameters.ndpFeedbackReport;
                info.ndpFeedbackStatus = parameters.ndpFeedbackStatus;
                info.ndpRuToneSetIndex = parameters.ndpRuToneSetIndex;
                info.ndpStartingStsNumber = parameters.ndpStartingStsNumber;
                tag->appendAllocations(info);
                tag->setRuIndex(info.ruIndex);
            }
            else {
                tag->setCoding(user.getCoding().has_value() ? *user.getCoding() : HE_CODING_BCC);
            }
            if (common.getPpduFormat() != HE_TRIGGER_BASED_UPLINK &&
                    user.getStaId() && user.getRuAllocation() && user.getMcs() &&
                    user.getNumberOfSpaceTimeStreams() && user.getDcm() && user.getCoding()) {
                Ieee80211HeMuRxAllocationInfo info;
                info.ruIndex = user.getRuAllocation()->index;
                info.staId = *user.getStaId();
                info.ruToneSize = user.getRuAllocation()->toneSize;
                info.ruToneOffset = user.getRuAllocation()->toneOffset;
                info.mcs = *user.getMcs();
                info.numberOfSpatialStreams = *user.getNumberOfSpaceTimeStreams();
                info.dcm = *user.getDcm();
                tag->appendAllocations(info);
                tag->setRuIndex(info.ruIndex);
            }
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
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

} // namespace physicallayer

} // namespace inet
