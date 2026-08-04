//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"

#include <cmath>
#include <sstream>

// IEEE 802.11ax HE transmitter.
//
// Builds Ieee80211Transmission objects and computes per-user RU parameters for
// HE MU and HE TB PPDUs.  Uses the HE PHY calculator (Ieee80211HePhyCalculator)
// to validate PPDU parameters and determine the common duration.
// Relevant clauses:
//   - Clause 27.3.4: HE PPDU formats.
//   - Clause 27.3.11.7: HE-SIG-A.
//   - Clause 27.3.11.8: HE-SIG-B for HE MU PPDUs.
//   - Clause 27.3.12: modulation and coding for the HE data field.
//
// Approximations / simplifications:
//   - HE-LTF type is hardcoded to 4x for all HE MU/HE TB PPDUs.  The standard
//     permits 1x/2x/4x HE-LTF modes; only 4x is currently supported.
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
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211FecCodingReq.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"

namespace inet {

namespace physicallayer {

static Hz getModeSelectionBandwidth(const Ieee80211ModeSet *modeSet,
        const IIeee80211Band *band, Hz configuredBandwidth)
{
    return modeSet != nullptr ? modeSet->getModeBandwidth(band, configuredBandwidth) : configuredBandwidth;
}

static uint8_t encodeCanonicalHeTxopDuration(const Ieee80211HeCommonPhyParameters& common)
{
    if (common.sigA.txopUnspecified)
        return 127;
    return common.sigA.txopDurationUs < 512 ?
            (common.sigA.txopDurationUs / 8) << 1 :
            1 | ((common.sigA.txopDurationUs - 512) / 128) << 1;
}

static uint8_t encodeCanonicalHeGiLtfSize(const Ieee80211HeCommonPhyParameters& common)
{
    if (common.guardInterval == HE_GI_0_8_US && common.ltfType == HE_LTF_4X)
        return 0;
    if (common.guardInterval == HE_GI_0_8_US && common.ltfType == HE_LTF_2X)
        return 1;
    if (common.guardInterval == HE_GI_1_6_US && common.ltfType == HE_LTF_2X)
        return 2;
    if (common.guardInterval == HE_GI_3_2_US && common.ltfType == HE_LTF_4X)
        return 3;
    throw cRuntimeError("Canonical HE GI/LTF combination is not encodable in HE-SIG-A");
}

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
        const char *bandName = par("bandName");
        setBand(*bandName != '\0' ? Ieee80211CompliantBands::getBand(bandName) : nullptr);
        // The containing radio resolves the band-aware HE profile once at the
        // physical-layer stage. Legacy standalone transmitters retain their
        // local mode-set initialization for compatibility.
        setModeSet(*opMode && strcmp(opMode, "ax") ? Ieee80211ModeSet::getModeSet(opMode) : nullptr);
        auto channelWidth = getModeSelectionBandwidth(modeSet, band, bandwidth);
        if (modeSet != nullptr)
            setMode(bitrate != bps(-1) ? modeSet->getMode(bitrate, channelWidth) : modeSet->getFastestMode(channelWidth));
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
        WATCH_EXPR("lastHeDuration", lastHeDuration.str());
        WATCH_EXPR("lastHeCenterFrequency", lastHeCenterFrequency.str());
        WATCH_EXPR("lastHeBandwidth", lastHeBandwidth.str());
        WATCH_EXPR("lastHeTransmitPower", lastHeTransmitPower.str());
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
    // IEEE Std 802.11-2024 PHY-TXSTART.request carries TXVECTOR parameters
    // such as DATARATE/MCS/FORMAT (Clause 8.3.5.5, with PHY-specific tables in
    // Clauses 15..21). INET maps those requests to a concrete IIeee80211Mode.
    if (modeReq != nullptr) {
        if (modeSet != nullptr && !modeSet->containsMode(modeReq->getMode()))
            throw cRuntimeError("Unsupported mode requested");
        transmissionMode = modeReq->getMode();
    }
    else if (modeSet != nullptr && bitrateReq != nullptr)
        transmissionMode = modeSet->getMode(bitrateReq->getDataBitrate(), getModeSelectionBandwidth(modeSet, band, bandwidth));
    else
        transmissionMode = mode;
    if (transmissionMode == nullptr)
        throw cRuntimeError("Transmission mode is undefined");

    // The MAC supplies only peer-negotiated permission. The PHY remains the
    // authority that maps the selected HT/VHT mode to its BCC or LDPC variant.
    if (auto fecCodingReq = const_cast<Packet *>(packet)->findTag<Ieee80211FecCodingReq>()) {
        bool useLdpc = fecCodingReq->getLdpcAllowed();
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
        else if (auto ehtMode = dynamic_cast<const Ieee80211EhtMode *>(transmissionMode)) {
            auto mcs = ehtMode->getDataMode()->getModulationAndCodingScheme();
            auto preambleFormat = ehtMode->getPreambleMode()->getPreambleFormat();
            auto gi = ehtMode->getDataMode()->getGuardIntervalType();
            auto centerFreqMode = ehtMode->getCenterFrequencyMode();
            transmissionMode = Ieee80211EhtCompliantModes::getCompliantMode(mcs, centerFreqMode, preambleFormat, gi, useLdpc);
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
    this->modeSet = modeSet;
    auto channelWidth = getModeSelectionBandwidth(modeSet, band, bandwidth);
    if (modeSet == nullptr)
        mode = nullptr;
    else if (bitrate != bps(-1))
        mode = modeSet->getMode(bitrate, channelWidth);
    else
        mode = modeSet->getFastestMode(channelWidth);
}

void Ieee80211Transmitter::setModeSetAndMode(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode)
{
    if (modeSet == nullptr || mode == nullptr || !modeSet->containsMode(mode))
        throw cRuntimeError("The explicit 802.11 mode is not part of the target mode profile");
    this->modeSet = modeSet;
    this->mode = mode;
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
        if (channel != nullptr)
            setChannel(new Ieee80211Channel(band, channel->getChannelNumber(), channel->getSecondaryChannelOffset()));
        else
            this->band = band;
    }
}

void Ieee80211Transmitter::setChannel(const Ieee80211Channel *channel)
{
    if (this->channel != channel) {
        auto centerFrequency = channel->getCenterFrequency();
        delete this->channel;
        this->channel = channel;
        this->band = channel->getBand();
        setCenterFrequency(centerFrequency);
    }
}

void Ieee80211Transmitter::setChannelNumber(int channelNumber)
{
    if (channel == nullptr || channelNumber != channel->getChannelNumber())
        setChannel(new Ieee80211Channel(band, channelNumber, channel == nullptr ?
                IEEE80211_SECONDARY_CHANNEL_NONE : channel->getSecondaryChannelOffset()));
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
    auto heMuHeader = dynamicPtrCast<const Ieee80211HePhyHeader>(phyHeader);
    const IIeee80211Mode *transmissionMode = computeTransmissionMode(packet);
    const Ieee80211Channel *transmissionChannel = computeTransmissionChannel(packet);
    W transmissionPower = computeTransmissionPower(packet);
    Hz transmissionBandwidth = transmissionMode->getDataMode()->getBandwidth();
    int requiredSpatialStreams = transmissionMode->getDataMode()->getNumberOfSpatialStreams();
    std::shared_ptr<const Ieee80211HeTxVector> heTxVector;
    std::shared_ptr<const Ieee80211HePpduLayout> hePpduLayout;
    std::shared_ptr<const Ieee80211VhtTxVector> vhtTxVector;
    if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader)) {
        if (vhtHeader->getSignalingValid()) {
            if (auto handoff = packet->findTag<Ieee80211VhtTxVectorReq>()) {
                vhtTxVector = handoff->getTxVector();
                if (vhtTxVector == nullptr || !vhtTxVector->isMu() ||
                        vhtTxVector->getGroupId() != vhtHeader->getGroupId() ||
                        vhtTxVector->getPsduLength() != B(vhtHeader->getLengthField()))
                    throw cRuntimeError("VHT transmission has a mismatched canonical MU TXVECTOR handoff");
                requiredSpatialStreams = 2;
            }
            else {
                auto vhtRequest = packet->findTag<Ieee80211VhtTransmissionTag>();
                auto gainDb = vhtHeader->getBeamformed() && vhtRequest != nullptr ?
                        vhtRequest->getBeamformingGainDb() : 0;
                vhtTxVector = std::make_shared<const Ieee80211VhtTxVector>(
                        transmissionBandwidth,
                        B(vhtHeader->getLengthField()), vhtHeader->getGroupId(),
                        vhtHeader->getNumberOfSpaceTimeStreams(),
                        vhtHeader->getMcs(), vhtHeader->getCoding() != 0,
                        vhtHeader->getLdpcExtraOfdmSymbol(),
                        vhtHeader->getPartialAid(), vhtHeader->getBeamformed(),
                        gainDb);
            }
        }
    }
    if (heMuHeader != nullptr) {
        auto handoff = packet->findTag<Ieee80211HeTxVectorReq>();
        if (handoff == nullptr || !handoff->getTxVector() || !handoff->getPpduLayout())
            throw cRuntimeError("HE transmission is missing its canonical TXVECTOR/PPDU-layout handoff");
        heTxVector = handoff->getTxVector();
        hePpduLayout = handoff->getPpduLayout();
        if (!hePpduLayout->matches(*heTxVector))
            throw cRuntimeError("HE transmission has a mismatched canonical TXVECTOR/PPDU-layout handoff");
        const auto& users = hePpduLayout->getUsers();
        if (hePpduLayout->getPpduFormat() == HE_TRIGGER_BASED_UPLINK) {
            // An HE-TB packet is emitted by one responding STA. The first
            // layout user is that local user; any following zero-PSDU users
            // retain peer stream geometry only for common MU timing. Starting
            // Spatial Stream places the local STSs within the shared RU and
            // does not increase this transmitter's antenna requirement.
            if (!users.empty())
                requiredSpatialStreams = std::max(requiredSpatialStreams,
                        users.front().numberOfSpatialStreams);
        }
        else {
            for (const auto& user : users)
                requiredSpatialStreams = std::max(requiredSpatialStreams,
                        user.streamStartIndex + user.numberOfSpatialStreams);
        }
    }
    if (requiredSpatialStreams > transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Number of spatial streams is higher than the number of antennas");
    simtime_t duration = transmissionMode->getDuration(B(phyHeader->getLengthField()));
    simtime_t preambleDuration = transmissionMode->getPreambleMode()->getDuration();
    simtime_t headerDuration = transmissionMode->getHeaderMode()->getDuration();
    if (vhtTxVector != nullptr) {
        // VHT-SIG-A/B are included in Ieee80211VhtPreambleMode::getDuration().
        // Unlike legacy PHYs, the VHT signal mode is not a separate analog
        // header interval, so subtracting it here would remove DATA airtime.
        headerDuration = SIMTIME_ZERO;
    }
    if (vhtTxVector != nullptr && vhtTxVector->isMu()) {
        duration = vhtTxVector->getCommonDuration();
        preambleDuration = vhtTxVector->getPreambleDuration();
        headerDuration = vhtTxVector->getHeaderDuration();
    }
    if (vhtTxVector != nullptr && vhtTxVector->isNdp()) {
        // IEEE Std 802.11-2024 Figure 21-28: a VHT NDP contains the complete
        // VHT preamble (including VHT-SIG-B) and no Data field.
        duration = preambleDuration;
        headerDuration = SIMTIME_ZERO;
    }
    // For non-HE modes, the mode classes implement the PHY-specific TXTIME
    // formulas: DSSS 15.4.6.7, HR/DSSS 16.3.4, OFDM 17.4.3, ERP 18.5.3.2,
    // HT 19.4.3, and VHT 21.4.3. The analog model then splits the result into
    // preamble, header/signaling, and DATA intervals.
    // IEEE Std 802.11-2024, 19.3.15.4: an HT40 PPDU occupies the primary
    // and secondary 20 MHz channels and is centered halfway between them.
    Hz transmissionCenterFrequency = dynamic_cast<const Ieee80211HtMode *>(transmissionMode) != nullptr &&
            transmissionBandwidth == MHz(40) ? transmissionChannel->getBondedCenterFrequency() :
            transmissionChannel->getCenterFrequency();
    if (heMuHeader != nullptr) {
        if (getIeee80211HePpduFormat(*heMuHeader) != hePpduLayout->getPpduFormat() ||
                heMuHeader->getNdp() != hePpduLayout->isNdp() ||
                heMuHeader->getBssColor() != heTxVector->getCommon().getParameters().sigA.bssColor ||
                heMuHeader->getGuardInterval() != hePpduLayout->getGuardInterval() ||
                heMuHeader->getPacketExtensionDurationUs() != hePpduLayout->getPacketExtensionDurationUs() ||
                heMuHeader->getCommonDuration() != hePpduLayout->getDuration())
            throw cRuntimeError("HE PHY header disagrees with the canonical TXVECTOR/PPDU layout");
        const auto ppduFormat = hePpduLayout->getPpduFormat();
        const auto& canonicalCommon = heTxVector->getCommon().getParameters();
        const Ieee80211HeSuErSignalingFields *signaling = nullptr;
        if (ppduFormat == HE_SINGLE_USER)
            signaling = &dynamicPtrCast<const Ieee80211HeSuPhyHeader>(heMuHeader)->getSignaling();
        else if (ppduFormat == HE_EXTENDED_RANGE_SU)
            signaling = &dynamicPtrCast<const Ieee80211HeErSuPhyHeader>(heMuHeader)->getSignaling();
        if (signaling != nullptr && signaling->signalingValid &&
                (signaling->uplink != canonicalCommon.sigA.uplink ||
                 signaling->bssColor != canonicalCommon.sigA.bssColor ||
                 signaling->doppler != canonicalCommon.sigA.doppler ||
                 signaling->txop != encodeCanonicalHeTxopDuration(canonicalCommon)))
            throw cRuntimeError("HE SU/ER signaling disagrees with the canonical TXVECTOR");
        const auto& canonicalUsers = hePpduLayout->getUsers();
        if (ppduFormat == HE_MU_DOWNLINK) {
            const auto& muSignaling = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(heMuHeader)->getSignaling();
            std::vector<bool> punctured(std::lround(canonicalCommon.channelBandwidth.get() / 20e6), false);
            for (size_t i = 0; i < punctured.size(); i++)
                punctured[i] = canonicalCommon.puncturedSubchannelMask & (uint8_t(1) << i);
            auto bandwidth = encodeHeMuBandwidth(canonicalCommon.channelBandwidth, punctured,
                    canonicalCommon.sigB.compression);
            if (!muSignaling.signalingValid || !bandwidth ||
                    muSignaling.lSigLength != canonicalCommon.lSigLength ||
                    muSignaling.bandwidth != bandwidth.value ||
                    muSignaling.heSigBCompression != canonicalCommon.sigB.compression ||
                    muSignaling.heSigBMcs != canonicalCommon.sigB.mcs ||
                    muSignaling.numberOfHeSigBSymbols != (canonicalCommon.sigB.compression ? 0 : canonicalCommon.sigB.numberOfSymbols) ||
                    muSignaling.numberOfMuMimoUsers != (canonicalCommon.sigB.compression ? canonicalUsers.size() : 0) ||
                    muSignaling.giLtfSize != encodeCanonicalHeGiLtfSize(canonicalCommon) ||
                    muSignaling.doppler != canonicalCommon.sigA.doppler ||
                    muSignaling.midamblePeriodicity != canonicalCommon.sigA.midamblePeriodicity ||
                    muSignaling.txop != encodeCanonicalHeTxopDuration(canonicalCommon) ||
                    muSignaling.numberOfHeLtfSymbols != canonicalCommon.numberOfHeLtfSymbols ||
                    muSignaling.ldpcExtraSymbolSegment != canonicalCommon.ldpcExtraSymbol ||
                    muSignaling.stbc != canonicalCommon.sigA.stbc)
                throw cRuntimeError("HE MU signaling disagrees with the canonical TXVECTOR");
        }
        else if (ppduFormat == HE_TRIGGER_BASED_UPLINK) {
            const auto& tbSignaling = dynamicPtrCast<const Ieee80211HeTbPhyHeader>(heMuHeader)->getSignaling();
            uint8_t bandwidth = canonicalCommon.channelBandwidth == MHz(20) ? 0 :
                    canonicalCommon.channelBandwidth == MHz(40) ? 1 :
                    canonicalCommon.channelBandwidth == MHz(80) ? 2 : 3;
            if (!tbSignaling.signalingValid ||
                    tbSignaling.lSigLength != canonicalCommon.lSigLength ||
                    tbSignaling.bandwidth != bandwidth ||
                    tbSignaling.txop != encodeCanonicalHeTxopDuration(canonicalCommon))
                throw cRuntimeError("HE TB signaling disagrees with the canonical TXVECTOR");
        }
        if (ppduFormat == HE_SINGLE_USER || ppduFormat == HE_EXTENDED_RANGE_SU) {
            if (heMuHeader->getUsersArraySize() != 0)
                throw cRuntimeError("HE SU/ER PHY header must not contain a per-user array");
        }
        else {
            if (heMuHeader->getUsersArraySize() != canonicalUsers.size())
                throw cRuntimeError("HE MU/TB PHY header user count disagrees with the canonical PPDU layout");
            for (size_t i = 0; i < canonicalUsers.size(); i++) {
                const auto& projected = heMuHeader->getUsers(i);
                const auto& canonical = canonicalUsers[i];
                std::vector<const Ieee80211HeUserPhyParameters *> ruUsers;
                for (const auto& candidate : canonicalUsers)
                    if (candidate.ru.toneSize == canonical.ru.toneSize &&
                            candidate.ru.toneOffset == canonical.ru.toneOffset)
                        ruUsers.push_back(&candidate);
                std::sort(ruUsers.begin(), ruUsers.end(), [] (const auto *left, const auto *right) {
                    return left->streamStartIndex < right->streamStartIndex;
                });
                std::vector<int> nsts;
                for (const auto *candidate : ruUsers)
                    nsts.push_back(candidate->numberOfSpatialStreams);
                const bool muMimo = ruUsers.size() > 1;
                const uint8_t spatialConfiguration = ppduFormat == HE_MU_DOWNLINK && muMimo ?
                        encodeHeMuSpatialConfiguration(nsts) : 0;
                if (projected.ruIndex != canonical.ru.index ||
                        projected.ruToneSize != canonical.ru.toneSize ||
                        projected.ruToneOffset != canonical.ru.toneOffset ||
                        projected.staId != canonical.staId || projected.mcs != canonical.mcs ||
                        projected.numberOfSpatialStreams != canonical.numberOfSpatialStreams ||
                        projected.dcm != canonical.dcm || projected.coding != canonical.coding ||
                        projected.psduLength != canonical.psduLength ||
                        projected.duration != hePpduLayout->getDuration() ||
                        projected.streamStartIndex != canonical.streamStartIndex ||
                        projected.muMimo != muMimo ||
                        projected.spatialConfiguration != spatialConfiguration)
                    throw cRuntimeError("HE MU/TB PHY header user %zu disagrees with the canonical PPDU layout", i);
            }
        }
        duration = hePpduLayout->getDuration();
        preambleDuration = hePpduLayout->getCommonPreambleDuration();
        // The packet-level HE preamble duration already includes HE-SIG-A
        // (and its ER-SU repetition), so a separate header interval would
        // count HE signaling twice and shorten the analog DATA interval.
        headerDuration = SIMTIME_ZERO;
        if (hePpduLayout->getPpduFormat() == HE_TRIGGER_BASED_UPLINK &&
                hePpduLayout->getUsers().size() == 1) {
            // HE TB responses occupy the RU assigned by the Trigger frame
            // User Info field (Clause 9.3.1.22 and Clause 27.3.4). The
            // packet-level analog model narrows the transmit center
            // frequency/bandwidth to that RU for single-user UL-TB transmissions.
            const auto& ru = hePpduLayout->getUsers().front().ru;
            transmissionBandwidth = ru.bandwidth;
            transmissionCenterFrequency = ru.centerFrequency;
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
        lastHePpduFormat = getIeee80211HePpduFormat(*heMuHeader);
        lastHeMuMimo = heMuHeader->getMuMimo();
        lastHeUserCount = heMuHeader->getUsersArraySize();
        lastHeTotalNsts = heMuHeader->getTotalNsts();
        lastHePacketExtensionDurationUs = heMuHeader->getPacketExtensionDurationUs();
        lastHePuncturedSubchannelMask = heMuHeader->getPuncturedSubchannelMask();
        lastHeDuration = duration;
        lastHeCenterFrequency = transmissionCenterFrequency;
        lastHeBandwidth = transmissionBandwidth;
        lastHeTransmitPower = transmissionPower;
        lastHeUserPhyParameters = hePpduLayout->getUsers();
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
    auto triggerCorrelation = packet->findTag<Ieee80211HeTriggerCorrelationTag>();
    auto triggerCorrelationId = triggerCorrelation == nullptr ? 0 : triggerCorrelation->getTriggerId();
    return new Ieee80211Transmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, nullptr, nullptr, nullptr, nullptr, analogModel, transmissionMode, transmissionChannel, heTxVector, hePpduLayout, triggerCorrelationId, vhtTxVector);
}

} // namespace physicallayer

} // namespace inet
