//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

// IEEE 802.11ax HE receiver.
//
// Handles HE MU and HE TB PPDU reception, RU/STA-ID resolution, per-MPDU
// success/failure reporting, and HE spatial reuse (BSS color / OBSS/PD).
// Relevant normative clauses:
//   - Clause 26.4.4: response rules and reception procedures for HE MU/HE TB.
//   - Clause 27.3.2.5: resource indication and user identification in HE MU.
//   - Clause 27.3.4: HE PPDU formats.
//   - Clause 27.3.11.7 and 27.3.11.8: HE-SIG-A/B signaling used to identify users.
//   - Clause 27.3.13: PHY receive procedure.
//   - Clause 26.11: HE spatial reuse.
//
// Approximations / simplifications:
//   - Per-MPDU failure injection inside an unsuccessful HE MU payload is
//     randomized (one random MPDU is marked failed) rather than modeling the
//     exact FCS/delimiter decoding outcome of each subframe.
//   - Concurrent HE TB reception is admitted only when the transmissions share
//     the same Trigger ID.  Real multi-user UL depends on tight timing/frequency
//     synchronization, which is not modeled here.
//   - HE-SIG-A and HE-SIG-B are not decoded with separate SNIR thresholds; the
//     whole PPDU is evaluated using the data-field error model.

#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
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
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Receiver);

static bool parseHeBssColor(const char *token, int& color)
{
    char *end = nullptr;
    auto parsed = strtol(token, &end, 0);
    if (end == token || *end != '\0' || parsed < 1 || parsed > 63)
        return false;
    color = parsed;
    return true;
}

static Ptr<const Ieee80211HeMuPhyHeader> peekHeMuPhyHeader(const ITransmission *transmission)
{
    auto packet = transmission->getPacket();
    return transmission->getPacketProtocol() == &Protocol::ieee80211HePhy && packet != nullptr && packet->hasAtFront<Ieee80211HeMuPhyHeader>()
            ? packet->peekAtFront<Ieee80211HeMuPhyHeader>()
            : nullptr;
}

static bool containsHeMuUser(const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader, uint16_t staId)
{
    for (unsigned int i = 0; i < phyHeader->getUsersArraySize(); ++i)
        if (phyHeader->getUsers(i).staId == staId)
            return true;
    return false;
}

static Ptr<Ieee80211HeMuPhyHeader> copyHeMuPhyHeader(const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader)
{
    return staticPtrCast<Ieee80211HeMuPhyHeader>(phyHeader->dupShared());
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

static bool applyHeMuMpduReceiveOutcomes(Packet *packet,
        const std::vector<const IReceptionDecision *> *decisions, cRNG *rng)
{
    bool commonSuccessful = true;
    bool dataSuccessful = true;
    for (auto decision : *decisions) {
        if (decision->isReceptionSuccessful())
            continue;
        switch (decision->getSignalPart()) {
            case IRadioSignal::SIGNAL_PART_PREAMBLE:
            case IRadioSignal::SIGNAL_PART_HEADER:
            case IRadioSignal::SIGNAL_PART_WHOLE:
                commonSuccessful = false;
                break;
            case IRadioSignal::SIGNAL_PART_DATA:
                dataSuccessful = false;
                break;
            default:
                break;
        }
    }
    auto indication = packet->findTagForUpdate<Ieee80211MpduReceiveInd>();
    if (!commonSuccessful || indication == nullptr)
        return commonSuccessful && dataSuccessful;
    if (dataSuccessful)
        return true;

    int64_t totalBytes = 0;
    for (unsigned int i = 0; i < indication->getResultsArraySize(); ++i) {
        const auto& result = indication->getResults(i);
        if (result.status == MPDU_NOT_EVALUATED)
            totalBytes += std::max<int64_t>(1, result.length.get<B>());
    }
    if (totalBytes == 0)
        return false;

    int64_t failedByte = std::min<int64_t>(totalBytes - 1,
            static_cast<int64_t>(rng->doubleRand() * totalBytes));
    int64_t cumulativeBytes = 0;
    bool failureAssigned = false;
    for (unsigned int i = 0; i < indication->getResultsArraySize(); ++i) {
        auto result = indication->getResults(i);
        if (result.status == MPDU_NOT_EVALUATED) {
            cumulativeBytes += std::max<int64_t>(1, result.length.get<B>());
            if (!failureAssigned && failedByte < cumulativeBytes) {
                result.status = MPDU_FCS_ERROR;
                failureAssigned = true;
            }
            else
                result.status = MPDU_SUCCESS;
            indication->setResults(i, result);
        }
    }
    return true;
}

static Packet *extractHeMuMpdu(const Packet *transmittedPacket, uint16_t staId)
{
    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
            Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED |
            Chunk::PF_ALLOW_REINTERPRETATION;
    auto packetCopy = transmittedPacket->dup();
    packetCopy->popAtFront<Ieee80211HeMuPhyHeader>(b(-1), parsingFlags);
    if (dynamicPtrCast<const ieee80211::Ieee80211MacHeader>(
            packetCopy->peekAtFront(b(-1), parsingFlags)) != nullptr)
        packetCopy->popAtFront<ieee80211::Ieee80211MacHeader>(b(-1), parsingFlags);
    while (packetCopy->getDataLength() > b(0) &&
            dynamicPtrCast<const Ieee80211HeMuRuPayloadHeader>(
                    packetCopy->peekAtFront(b(-1), parsingFlags)) != nullptr) {
        auto payloadHeader = packetCopy->popAtFront<Ieee80211HeMuRuPayloadHeader>(b(-1), parsingFlags);
        if (payloadHeader->getMpduLength() == B(0)) {
            // An NDP carries no MAC payload. Deliver it as a legacy-preamble
            // indication when it is addressed to this STA; otherwise continue
            // parsing the remaining zero-length user descriptors.
            if (payloadHeader->getStaId() == staId) {
                delete packetCopy;
                return nullptr;
            }
            continue;
        }
        if (payloadHeader->getStaId() == staId) {
            auto mpdu = new Packet(transmittedPacket->getName());
            mpdu->insertAtBack(packetCopy->popAtFront(payloadHeader->getMpduLength(), parsingFlags));
            auto indication = mpdu->addTagIfAbsent<Ieee80211MpduReceiveInd>();
            auto parser = mpdu->dup();
            B offset(0);
            while (parser->getDataLength() > b(0) &&
                    dynamicPtrCast<const ieee80211::Ieee80211MpduSubframeHeader>(
                            parser->peekAtFront(b(-1), parsingFlags)) != nullptr) {
                auto delimiter = parser->popAtFront<ieee80211::Ieee80211MpduSubframeHeader>(
                        b(-1), parsingFlags);
                Ieee80211MpduReceiveResult receiveResult;
                receiveResult.offset = offset;
                receiveResult.length = B(delimiter->getLength());
                receiveResult.status = delimiter->isIncorrect() ?
                        MPDU_DELIMITER_ERROR : MPDU_NOT_EVALUATED;
                if (parser->getDataLength() >= receiveResult.length) {
                    auto macHeader = dynamicPtrCast<const ieee80211::Ieee80211DataHeader>(
                            parser->peekAtFront(b(-1), parsingFlags));
                    if (macHeader != nullptr) {
                        receiveResult.sequenceNumber = macHeader->getSequenceNumber().get();
                        receiveResult.fragmentNumber = macHeader->getFragmentNumber();
                        receiveResult.tid = macHeader->getTid();
                    }
                    parser->popAtFront(receiveResult.length, parsingFlags);
                }
                else {
                    receiveResult.status = MPDU_PAYLOAD_ERROR;
                    parser->popAtFront(parser->getDataLength(), parsingFlags);
                }
                indication->appendResults(receiveResult);
                offset += B(4) + receiveResult.length;
                int padding = (4 - (B(4) + receiveResult.length).get<B>() % 4) % 4;
                if (padding > 0 && parser->getDataLength() >= B(padding)) {
                    parser->popAtFront(B(padding), parsingFlags);
                    offset += B(padding);
                }
            }
            delete parser;
            delete packetCopy;
            return mpdu;
        }
        packetCopy->popAtFront(payloadHeader->getMpduLength(), parsingFlags);
    }
    delete packetCopy;
    return nullptr;
}

static Packet *buildHeMuPhyPacket(const Packet *transmittedPacket, const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader, uint16_t staId)
{
    auto packet = extractHeMuMpdu(transmittedPacket, staId);
    if (packet == nullptr)
        return nullptr;
    auto phyHeaderCopy = copyHeMuPhyHeader(phyHeader);
    phyHeaderCopy->setLengthField(B(packet->getDataLength()));
    packet->insertAtFront(phyHeaderCopy);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);
    return packet;
}

static Packet *buildLegacyHeMuPreambleIndication(const Ptr<const Ieee80211HeMuPhyHeader>& phyHeader, const IReception *reception)
{
    auto packet = new Packet("HE-MU-Legacy-Preamble");
    auto phyHeaderCopy = copyHeMuPhyHeader(phyHeader);
    phyHeaderCopy->setLengthField(B(0));
    packet->insertAtFront(phyHeaderCopy);
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
        enableSpatialReuse = par("enableSpatialReuse");
        obssPdThreshold = mW(math::dBmW2mW(par("obssPdThreshold")));
        nonSrgObssPdThreshold = mW(math::dBmW2mW(par("nonSrgObssPdThreshold")));
        srgObssPdThreshold = mW(math::dBmW2mW(par("srgObssPdThreshold")));
        enableNonSrgSpatialReuse = par("enableNonSrgSpatialReuse");
        enableSrgSpatialReuse = par("enableSrgSpatialReuse");
        enableParameterizedSpatialReuse = par("enableParameterizedSpatialReuse");
        obssPdMinThresholdDbm = par("obssPdMinThreshold");
        spatialReusePowerReferenceDbm = par("spatialReusePowerReference");
        cStringTokenizer tokenizer(par("srgBssColors").stringValue(), ", ");
        while (tokenizer.hasMoreTokens()) {
            int color = 0;
            auto token = tokenizer.nextToken();
            if (!parseHeBssColor(token, color))
                throw cRuntimeError("Invalid HE SRG BSS color '%s', expected 1..63", token);
            srgBssColors.insert(color);
        }
        WATCH_PTR(modeSet);
        WATCH_PTR(band);
        WATCH_PTR(channel);
        WATCH(lastHeReception);
        WATCH(lastHePpduFormat);
        WATCH(lastHeUserCount);
        WATCH(lastHeBssColor);
        WATCH(lastHeRuAssigned);
        WATCH(lastSpatialReuseBssType);
        WATCH(lastSpatialReuseEligible);
        WATCH(lastSpatialReuseIgnoredPpdu);
        WATCH_EXPR("lastSpatialReuseObssPdThreshold", lastSpatialReuseObssPdThreshold.str());
        WATCH_EXPR("lastSpatialReuseTransmitPowerLimit", lastSpatialReuseTransmitPowerLimit.str());
        WATCH(lastSpatialReuseReason);
        WATCH_EXPR("lastSpatialReuseBssTypeName", getLastSpatialReuseBssTypeName());
        WATCH_EXPR("lastHeReceptionSummary", getLastHeReceptionSummary());
    }
}

void Ieee80211Receiver::recordHeSpatialReuseDecision(const HeSpatialReuseDecision& decision) const
{
    lastSpatialReuseBssType = (int)decision.bssType;
    lastSpatialReuseEligible = decision.eligible;
    lastSpatialReuseIgnoredPpdu = decision.ignorePpdu;
    lastSpatialReuseObssPdThreshold = decision.obssPdThreshold;
    lastSpatialReuseTransmitPowerLimit = decision.transmitPowerLimit;
    lastSpatialReuseReason = decision.reason == nullptr ? "" : decision.reason;
}

const char *Ieee80211Receiver::getLastSpatialReuseBssTypeName() const
{
    switch ((HeSpatialReuseBssType)lastSpatialReuseBssType) {
        case HeSpatialReuseBssType::UNSPECIFIED: return "UNSPECIFIED";
        case HeSpatialReuseBssType::INTRA_BSS: return "INTRA_BSS";
        case HeSpatialReuseBssType::INTER_BSS_NON_SRG: return "INTER_BSS_NON_SRG";
        case HeSpatialReuseBssType::INTER_BSS_SRG: return "INTER_BSS_SRG";
        default: return "UNKNOWN";
    }
}

std::string Ieee80211Receiver::getLastHeReceptionSummary() const
{
    std::stringstream stream;
    stream << "he=" << (lastHeReception ? "yes" : "no")
           << ", format=" << lastHePpduFormat
           << ", users=" << lastHeUserCount
           << ", bssColor=" << lastHeBssColor
           << ", assignedRu=" << (lastHeRuAssigned ? "yes" : "no")
           << ", spatialReuse=" << getLastSpatialReuseBssTypeName()
           << ", ignored=" << (lastSpatialReuseIgnoredPpdu ? "yes" : "no")
           << ", reason=" << lastSpatialReuseReason;
    return stream.str();
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
    auto heMuPhyHeader = peekHeMuPhyHeader(transmission);
    if (heMuPhyHeader == nullptr)
        return true;
    // HE TB is received by the AP as the addressed receiver of the Trigger
    // exchange; DL HE MU needs the STA-ID match from HE-SIG-B User fields
    // (Clause 27.3.2.5 and Clause 27.3.11.8.4).
    if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK)
        return true;
    auto networkInterface = getContainingNicModule(this);
    auto staId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
    return staId.has_value() && containsHeMuUser(heMuPhyHeader, *staId);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    auto heMuPhyHeader = peekHeMuPhyHeader(transmission);
    if (heMuPhyHeader != nullptr)
        return ieee80211Transmission && heMuPhyHeader->getUsersArraySize() > 0 &&
               NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
    // Non-HE PPDUs use the PHY-specific mode objects annotated in this package
    // (DSSS Clause 15, HR/DSSS Clause 16, OFDM Clause 17, ERP Clause 18,
    // HT Clause 19, VHT Clause 21). Reception is only possible for modes that
    // belong to the configured 802.11 mode set; SNIR/sensitivity is then handled
    // by the common narrowband receiver abstraction.
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) &&
           NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto heMuPhyHeader = peekHeMuPhyHeader(reception->getTransmission());
    if (shouldIgnoreReceptionDueToHeSpatialReuse(listening, reception, true))
        return false;
    if (heMuPhyHeader != nullptr)
        return ieee80211Transmission && heMuPhyHeader->getUsersArraySize() > 0 &&
               getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
    // Same non-HE mode-set gate as above; this path evaluates the concrete
    // reception interval against the receiver sensitivity.
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) &&
           getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
}

bool Ieee80211Receiver::computeIsReceptionAttempted(const IListening *listening, const IReception *reception,
        IRadioSignal::SignalPart part, const IInterference *interference) const
{
    auto heMuPhyHeader = peekHeMuPhyHeader(reception->getTransmission());
    if (shouldIgnoreReceptionDueToHeSpatialReuse(listening, reception, false))
        return false;
    if (heMuPhyHeader == nullptr || heMuPhyHeader->getPpduFormat() != HE_TRIGGER_BASED_UPLINK)
        return FlatReceiverBase::computeIsReceptionAttempted(listening, reception, part, interference);
    if (!computeIsReceptionPossible(listening, reception, part))
        return false;

    // Clause 27.3.4 defines HE TB PPDUs as trigger responses; multiple users
    // may transmit concurrently on different RUs in the same Trigger exchange.
    // Propagation delay makes aligned STA responses arrive a few nanoseconds
    // apart, so ordinary single-reception arbitration would admit only the
    // first RU. Allow concurrent UL-TB reception only within one Trigger
    // exchange; the RU-aware interference model still decides success.
    auto currentTransmission = reception->getReceiverRadio()->getReceptionInProgress();
    if (currentTransmission == nullptr || currentTransmission == reception->getTransmission())
        return true;
    auto currentHeader = peekHeMuPhyHeader(currentTransmission);
    return currentHeader != nullptr &&
           currentHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK &&
           currentHeader->getTriggerId() == heMuPhyHeader->getTriggerId();
}

bool Ieee80211Receiver::shouldIgnoreReceptionDueToHeSpatialReuse(const IListening *listening, const IReception *reception, bool logDecision) const
{
    auto spatialReuseDecision = computeHeSpatialReuseDecision(listening, reception);
    recordHeSpatialReuseDecision(spatialReuseDecision);
    if (!spatialReuseDecision.ignorePpdu)
        return false;
    if (logDecision) {
        EV_DEBUG << "HE spatial reuse ignores PPDU: " << spatialReuseDecision.reason
                 << ", OBSS/PD=" << math::mW2dBmW(spatialReuseDecision.obssPdThreshold.get<mW>()) << " dBm"
                 << ", coupled TX power limit=" << math::mW2dBmW(spatialReuseDecision.transmitPowerLimit.get<mW>()) << " dBm\n";
    }
    return true;
}

Ieee80211Receiver::HeSpatialReuseDecision Ieee80211Receiver::computeHeSpatialReuseDecision(const IListening *listening, const IReception *reception) const
{
    HeSpatialReuseDecision decision;
    auto heMuPhyHeader = peekHeMuPhyHeader(reception->getTransmission());
    if (!enableSpatialReuse) {
        decision.reason = "spatial reuse disabled";
        return decision;
    }
    if (heMuPhyHeader == nullptr) {
        decision.reason = "not an HE PPDU";
        return decision;
    }
    auto receivedBssColor = heMuPhyHeader->getBssColor();
    // Table 27-21/27-22 carries BSS Color in HE-SIG-A. Color 0 disables BSS
    // coloring, so OBSS/PD classification cannot be applied.
    if (receivedBssColor == 0) {
        decision.reason = "received BSS color disabled";
        return decision;
    }
    auto networkInterface = getContainingNicModule(this);
    auto mib = networkInterface ? dynamic_cast<const ieee80211::Ieee80211Mib *>(networkInterface->getSubmodule("mib")) : nullptr;
    if (mib == nullptr || mib->heOperation.bssColor == 0) {
        decision.reason = "local BSS color disabled";
        return decision;
    }
    if (receivedBssColor == mib->heOperation.bssColor) {
        // Clause 26.11 spatial reuse applies to inter-BSS PPDUs; same-color
        // PPDUs remain intra-BSS and are not ignored by OBSS/PD.
        decision.bssType = HeSpatialReuseBssType::INTRA_BSS;
        decision.reason = "intra-BSS PPDU";
        return decision;
    }

    bool isSrg = srgBssColors.find(receivedBssColor) != srgBssColors.end();
    decision.bssType = isSrg ? HeSpatialReuseBssType::INTER_BSS_SRG : HeSpatialReuseBssType::INTER_BSS_NON_SRG;
    if (isSrg) {
        if (!enableSrgSpatialReuse) {
            decision.reason = "SRG OBSS/PD disabled";
            return decision;
        }
        if (heMuPhyHeader->getSrgObssPdDisallowed()) {
            decision.reason = "PPDU disallows SRG OBSS/PD";
            return decision;
        }
        decision.obssPdThreshold = srgObssPdThreshold;
    }
    else {
        if (!enableNonSrgSpatialReuse) {
            decision.reason = "non-SRG OBSS/PD disabled";
            return decision;
        }
        if (heMuPhyHeader->getNonSrgObssPdDisallowed()) {
            decision.reason = "PPDU disallows non-SRG OBSS/PD";
            return decision;
        }
        decision.obssPdThreshold = nonSrgObssPdThreshold;
    }

    if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK) {
        // Table 27-24 defines HE TB Spatial Reuse values. This branch models
        // parameterized spatial reuse only when the PPDU permits it.
        if (!enableParameterizedSpatialReuse) {
            decision.reason = "HE TB PPDU excluded from OBSS/PD";
            return decision;
        }
        if (heMuPhyHeader->getPsrDisallowed() || heMuPhyHeader->getSpatialReuse() == 0) {
            decision.reason = "PSR not permitted by PPDU";
            return decision;
        }
    }

    decision.eligible = true;
    decision.transmitPowerLimit = computeSpatialReuseTransmitPowerLimit(decision.obssPdThreshold);
    decision.ignorePpdu = !getAnalogModel()->computeIsReceptionPossible(listening, reception, decision.obssPdThreshold);
    decision.reason = decision.ignorePpdu ?
            (isSrg ? "inter-BSS SRG PPDU below OBSS/PD" : "inter-BSS non-SRG PPDU below OBSS/PD") :
            (isSrg ? "inter-BSS SRG PPDU at or above OBSS/PD" : "inter-BSS non-SRG PPDU at or above OBSS/PD");
    return decision;
}

W Ieee80211Receiver::computeSpatialReuseTransmitPowerLimit(W threshold) const
{
    auto thresholdDbm = math::mW2dBmW(threshold.get<mW>());
    auto limitDbm = spatialReusePowerReferenceDbm - std::max(0.0, thresholdDbm - obssPdMinThresholdDbm);
    return mW(math::dBmW2mW(limitDbm));
}

const IReceptionResult *Ieee80211Receiver::computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto transmittedPacket = transmission->getPacket();
    auto heMuPhyHeader = peekHeMuPhyHeader(transmission);
    if (heMuPhyHeader != nullptr) {
        lastHeReception = true;
        lastHePpduFormat = heMuPhyHeader->getPpduFormat();
        lastHeUserCount = heMuPhyHeader->getUsersArraySize();
        lastHeBssColor = heMuPhyHeader->getBssColor();
        if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK) {
            lastHeRuAssigned = true;
            auto packet = transmittedPacket->dup();
            if (!isReceptionSuccessful(decisions))
                packet->setBitError(true);
            addReceptionIndications(packet, reception, interference, snir);
            packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
            packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
            return new ReceptionResult(reception, decisions, packet);
        }
        auto networkInterface = getContainingNicModule(this);
        auto myStaId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
        lastHeRuAssigned = myStaId.has_value() && containsHeMuUser(heMuPhyHeader, *myStaId);
        auto packet = myStaId.has_value() && containsHeMuUser(heMuPhyHeader, *myStaId) &&
                modeSet->containsMode(transmission->getMode())
                ? buildHeMuPhyPacket(transmittedPacket, heMuPhyHeader, *myStaId)
                : buildLegacyHeMuPreambleIndication(heMuPhyHeader, reception);
        if (packet == nullptr)
            packet = buildLegacyHeMuPreambleIndication(heMuPhyHeader, reception);
        if (!applyHeMuMpduReceiveOutcomes(packet, decisions, getRNG(0)))
            packet->setBitError(true);
        addReceptionIndications(packet, reception, interference, snir);
        packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
        packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
        return new ReceptionResult(reception, decisions, packet);
    }
    lastHeReception = false;
    lastHePpduFormat = -1;
    lastHeUserCount = 0;
    lastHeBssColor = 0;
    lastHeRuAssigned = false;

    // Non-HE PPDU reception is packet-level: the standard-specific durations,
    // header fields, and padding are established in the mode/radio/transmitter
    // code, while this receiver reports the selected PHY mode and channel with
    // the decoded payload.
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
