//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
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
//   - FEC decoding and individual corrupt bits are not modeled. The HE error
//     model instead provides an analytical PER for every structurally valid
//     MPDU, and the receiver records one deterministic RNG outcome per MPDU.
//   - Concurrent HE TB reception is admitted only when the transmissions share
//     the same Trigger ID.  Real multi-user UL depends on tight timing/frequency
//     synchronization, which is not modeled here.
//   - HE-SIG-A and HE-SIG-B are not decoded with separate SNIR thresholds; the
//     whole PPDU is evaluated using the data-field error model.

#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211VhtPacketRadio.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211HePacketRadio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Protocol.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReceptionDecision.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISnir.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IInterference.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INoise.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ListeningDecision.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ReceptionResult.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandNoiseBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

namespace physicallayer {

simsignal_t Ieee80211Receiver::heSpatialReuseBssTypeSignal = cComponent::registerSignal("heSpatialReuseBssType");
simsignal_t Ieee80211Receiver::heSpatialReuseReceivedBssColorSignal = cComponent::registerSignal("heSpatialReuseReceivedBssColor");
simsignal_t Ieee80211Receiver::heSpatialReuseLocalBssColorSignal = cComponent::registerSignal("heSpatialReuseLocalBssColor");
simsignal_t Ieee80211Receiver::heSpatialReuseReceivedPowerSignal = cComponent::registerSignal("heSpatialReuseReceivedPower");
simsignal_t Ieee80211Receiver::heSpatialReuseEligibleSignal = cComponent::registerSignal("heSpatialReuseEligible");
simsignal_t Ieee80211Receiver::heSpatialReuseIgnoredPpduSignal = cComponent::registerSignal("heSpatialReuseIgnoredPpdu");
simsignal_t Ieee80211Receiver::heSpatialReuseObssPdThresholdSignal = cComponent::registerSignal("heSpatialReuseObssPdThreshold");
simsignal_t Ieee80211Receiver::heSpatialReuseTransmitPowerLimitSignal = cComponent::registerSignal("heSpatialReuseTransmitPowerLimit");
simsignal_t Ieee80211Receiver::heSpatialReuseReasonSignal = cComponent::registerSignal("heSpatialReuseReason");

Define_Module(Ieee80211Receiver);

static bool isReceptionSuccessful(
        const std::vector<const IReceptionDecision *> *decisions);

static bool parseHeBssColor(const char *token, int& color)
{
    char *end = nullptr;
    auto parsed = strtol(token, &end, 0);
    if (end == token || *end != '\0' || parsed < 1 || parsed > 63)
        return false;
    color = parsed;
    return true;
}

static Ptr<const Ieee80211HePhyHeader> peekHePhyHeader(const ITransmission *transmission)
{
    auto packet = transmission->getPacket();
    return transmission->getPacketProtocol() == &Protocol::ieee80211HePhy && packet != nullptr && packet->hasAtFront<Ieee80211HePhyHeader>()
            ? packet->peekAtFront<Ieee80211HePhyHeader>()
            : nullptr;
}

static Ptr<const Ieee80211HePhyHeader> peekHeMuOrTbPhyHeader(const ITransmission *transmission)
{
    auto phyHeader = peekHePhyHeader(transmission);
    return dynamicPtrCast<const Ieee80211HeMuPhyHeader>(phyHeader) != nullptr ||
            dynamicPtrCast<const Ieee80211HeTbPhyHeader>(phyHeader) != nullptr ? phyHeader : nullptr;
}

static W getHeRuAdjustedPowerThreshold(const IReception *reception,
        const Ieee80211Transmission *transmission, W fullChannelThreshold)
{
    auto narrowbandReception = dynamic_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel());
    if (narrowbandReception == nullptr || transmission == nullptr || transmission->getMode() == nullptr)
        return fullChannelThreshold;
    auto channelBandwidth = transmission->getMode()->getDataMode()->getBandwidth();
    return scaleHeRuPowerThreshold(fullChannelThreshold,
            narrowbandReception->getBandwidth(), channelBandwidth);
}

static bool containsHeMuUser(const Ptr<const Ieee80211HePhyHeader>& phyHeader, uint16_t staId)
{
    for (unsigned int i = 0; i < phyHeader->getUsersArraySize(); ++i)
        if (phyHeader->getUsers(i).staId == staId)
            return true;
    return false;
}

static Ptr<Ieee80211HePhyHeader> copyHeMuPhyHeader(const Ptr<const Ieee80211HePhyHeader>& phyHeader)
{
    return staticPtrCast<Ieee80211HePhyHeader>(phyHeader->dupShared());
}

static const Ieee80211VhtMuUser *resolveVhtMuUserForReception(
        const Ieee80211Transmission *transmission,
        const IIeee80211VhtPacketRadio *radio)
{
    const auto& txVector = transmission->getVhtTxVector();
    if (txVector == nullptr || !txVector->isMu() || radio == nullptr)
        return nullptr;
    auto selection = radio->getVhtMuRxSelection();
    if (!selection.active || selection.groupId != txVector->getGroupId() ||
            selection.channelWidth != txVector->getChannelWidth())
        return nullptr;
    return txVector->findMuUser(selection.userPosition);
}

static Packet *extractVhtMuPsdu(const Ieee80211Transmission *transmission,
        const Ieee80211VhtMuUser& user)
{
    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
            Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED |
            Chunk::PF_ALLOW_REINTERPRETATION;
    const auto& txVector = transmission->getVhtTxVector();
    const Ieee80211VhtPsduBitRange *selectedRange = nullptr;
    for (const auto& range : txVector->getPsduBitRanges())
        if (range.userIndex == user.userPosition) {
            if (selectedRange != nullptr)
                return nullptr;
            selectedRange = &range;
        }
    auto transmittedPacket = transmission->getPacket();
    auto copy = transmittedPacket->dup();
    auto phyHeader = copy->popAtFront<Ieee80211VhtPhyHeader>(b(-1), parsingFlags);
    if (selectedRange == nullptr || txVector->getPsduBitRanges().empty() ||
            copy->getDataLength() != txVector->getPsduBitRanges().back().getEndBitOffset()) {
        delete copy;
        return nullptr;
    }
    if (selectedRange->startBitOffset > b(0))
        copy->popAtFront(selectedRange->startBitOffset, parsingFlags);
    auto result = new Packet(transmittedPacket->getName());
    result->insertAtBack(copy->popAtFront(selectedRange->bitLength, parsingFlags));
    auto parser = result->dup();
    auto delimiter = dynamicPtrCast<const ieee80211::Ieee80211MpduSubframeHeader>(
            parser->peekAtFront(b(-1), parsingFlags));
    if (delimiter == nullptr || delimiter->getLength() == 0 ||
            parser->getDataLength() < B(4) + B(delimiter->getLength())) {
        delete parser;
        delete result;
        delete copy;
        return nullptr;
    }
    Ieee80211MpduReceiveResult receiveResult;
    receiveResult.offset = B(0);
    receiveResult.length = B(delimiter->getLength());
    receiveResult.status = delimiter->isIncorrect() ?
            MPDU_DELIMITER_ERROR : MPDU_NOT_EVALUATED;
    result->addTagIfAbsent<Ieee80211MpduReceiveInd>()->appendResults(receiveResult);
    delete parser;
    auto headerCopy = staticPtrCast<Ieee80211VhtPhyHeader>(phyHeader->dupShared());
    headerCopy->setLengthField(user.psduLength);
    result->insertAtFront(headerCopy);
    result->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211VhtPhy);
    delete copy;
    return result;
}

static Packet *buildVhtMuNonmemberIndication(
        const Ieee80211Transmission *transmission)
{
    auto phyHeader = transmission->getPacket()->peekAtFront<Ieee80211VhtPhyHeader>();
    auto result = new Packet("VHT-MU-nonmember",
            staticPtrCast<Ieee80211VhtPhyHeader>(phyHeader->dupShared()));
    result->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211VhtPhy);
    result->setBitError(true);
    return result;
}

bool Ieee80211Receiver::computeIsVhtMuUserReceptionSuccessful(
        const Ieee80211Transmission *transmission,
        const Ieee80211VhtMuUser& user,
        const std::vector<const IReceptionDecision *> *decisions) const
{
    return isReceptionSuccessful(decisions);
}

static void addReceptionIndications(Packet *packet, const IReception *reception, const IInterference *interference, const ISnir *snir)
{
    auto provenance = packet->addTagIfAbsent<Ieee80211PhyProvenanceInd>();
    provenance->setTransmissionId(reception->getTransmission()->getId());
    provenance->setTransmitterRadioId(reception->getTransmission()->getTransmitterRadioId());
    provenance->setStartTime(reception->getStartTime());
    provenance->setEndTime(reception->getEndTime());
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

static bool applyHeMpduReceiveOutcomes(Packet *packet,
        const std::vector<const IReceptionDecision *> *decisions,
        const Ieee80211ErrorModelBase *errorModel, const ISnir *snir,
        size_t userIndex, cRNG *rng)
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

    unsigned int structurallyValidMpdus = 0;
    for (unsigned int i = 0; i < indication->getResultsArraySize(); ++i) {
        auto result = indication->getResults(i);
        if (result.status == MPDU_NOT_EVALUATED) {
            structurallyValidMpdus++;
            if (!dataSuccessful)
                result.status = MPDU_FCS_ERROR;
            else if (errorModel == nullptr)
                result.status = MPDU_SUCCESS;
            else {
                auto errorRate = errorModel->computeHeMpduErrorRate(snir, userIndex,
                        result.length.get<B>() * 8);
                if (!errorRate)
                    throw cRuntimeError("Cannot evaluate HE MPDU error rate: %s",
                            errorRate.error.c_str());
                // Draw order is a reproducibility contract: one draw for each
                // structurally valid delimiter, including PER endpoints.
                double outcome = rng->doubleRand();
                result.status = outcome < errorRate.packetErrorRate ?
                        MPDU_FCS_ERROR : MPDU_SUCCESS;
            }
            indication->setResults(i, result);
        }
    }
    // Physical DATA failure and wholly malformed data remain packet failures.
    // Analytical FCS outcomes remain per MPDU even when every draw fails.
    return dataSuccessful && (structurallyValidMpdus != 0 ||
            indication->getResultsArraySize() == 0);
}

static const Ieee80211HeModelPsduBitRange *findHePsduRange(
        const Ieee80211Transmission *transmission, uint16_t staId)
{
    auto layout = transmission->getHePpduLayout();
    if (!layout)
        return nullptr;
    const Ieee80211HeModelPsduBitRange *result = nullptr;
    for (const auto& range : layout->getPsduBitRanges()) {
        if (range.getStaId() != staId)
            continue;
        if (result != nullptr)
            return nullptr;
        result = &range;
    }
    return result;
}

static Packet *extractHeMuMpdu(const Ieee80211Transmission *transmission,
        uint16_t staId, size_t& selectedUserIndex)
{
    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
            Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED |
            Chunk::PF_ALLOW_REINTERPRETATION;
    auto transmittedPacket = transmission->getPacket();
    auto packetCopy = transmittedPacket->dup();
    packetCopy->popAtFront<Ieee80211HePhyHeader>(b(-1), parsingFlags);
    auto layout = transmission->getHePpduLayout();
    auto range = findHePsduRange(transmission, staId);
    if (!layout || layout->isNdp() || range == nullptr ||
            layout->getPsduBitRanges().empty() ||
            packetCopy->getDataLength() != layout->getPsduBitRanges().back().getEndBitOffset()) {
        delete packetCopy;
        return nullptr;
    }
    selectedUserIndex = range->getUserIndex();
    if (range->getStartBitOffset() > b(0))
        packetCopy->popAtFront(range->getStartBitOffset(), parsingFlags);
    auto mpdu = new Packet(transmittedPacket->getName());
    if (range->getBitLength() > b(0))
        mpdu->insertAtBack(packetCopy->popAtFront(range->getBitLength(), parsingFlags));
    auto indication = mpdu->addTagIfAbsent<Ieee80211MpduReceiveInd>();
    auto parser = mpdu->dup();
    B mpduOffset(0);
    while (parser->getDataLength() > b(0) &&
            dynamicPtrCast<const ieee80211::Ieee80211MpduSubframeHeader>(
                    parser->peekAtFront(b(-1), parsingFlags)) != nullptr) {
        auto delimiter = parser->popAtFront<ieee80211::Ieee80211MpduSubframeHeader>(
                b(-1), parsingFlags);
        // Table 9-659 uses MPDU Length=0 for EOF/null delimiters and A-MPDU
        // padding. It represents no MPDU, so it has no FCS outcome and
        // consumes no RNG draw. NDP remains a separate PPDU-layout property.
        if (delimiter->getLength() == 0) {
            mpduOffset += B(4);
            continue;
        }
        Ieee80211MpduReceiveResult receiveResult;
        receiveResult.offset = mpduOffset;
        receiveResult.length = B(delimiter->getLength());
        // The packet-level model can honor a delimiter already marked
        // incorrect but does not synthesize or decode delimiter CRC bits.
        receiveResult.status = delimiter->isIncorrect() ?
                MPDU_DELIMITER_ERROR : MPDU_NOT_EVALUATED;
        if (parser->getDataLength() >= receiveResult.length) {
            auto macHeader = dynamicPtrCast<const ieee80211::Ieee80211MacHeader>(
                    parser->peekAtFront(b(-1), parsingFlags));
            if (macHeader != nullptr && !macHeader->isIncorrect() &&
                    !macHeader->isIncomplete() && !macHeader->isImproperlyRepresented()) {
                if (auto dataOrMgmtHeader = dynamicPtrCast<const ieee80211::Ieee80211DataOrMgmtHeader>(macHeader)) {
                    receiveResult.sequenceNumber = dataOrMgmtHeader->getSequenceNumber().get();
                    receiveResult.fragmentNumber = dataOrMgmtHeader->getFragmentNumber();
                }
                if (auto dataHeader = dynamicPtrCast<const ieee80211::Ieee80211DataHeader>(macHeader))
                    receiveResult.tid = dataHeader->getTid();
            }
            else if (receiveResult.status == MPDU_NOT_EVALUATED)
                receiveResult.status = MPDU_HEADER_ERROR;
            parser->popAtFront(receiveResult.length, parsingFlags);
        }
        else {
            receiveResult.status = MPDU_PAYLOAD_ERROR;
            parser->popAtFront(parser->getDataLength(), parsingFlags);
        }
        indication->appendResults(receiveResult);
        mpduOffset += B(4) + receiveResult.length;
        int padding = (4 - (B(4) + receiveResult.length).get<B>() % 4) % 4;
        if (padding > 0 && parser->getDataLength() >= B(padding)) {
            parser->popAtFront(B(padding), parsingFlags);
            mpduOffset += B(padding);
        }
    }
    delete parser;
    delete packetCopy;
    return mpdu;
}

static Packet *buildHeMuPhyPacket(const Ieee80211Transmission *transmission,
        const Ptr<const Ieee80211HePhyHeader>& phyHeader, uint16_t staId,
        size_t& selectedUserIndex)
{
    auto packet = extractHeMuMpdu(transmission, staId, selectedUserIndex);
    if (packet == nullptr)
        return nullptr;
    auto phyHeaderCopy = copyHeMuPhyHeader(phyHeader);
    phyHeaderCopy->setLengthField(B(packet->getDataLength()));
    packet->insertAtFront(phyHeaderCopy);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);
    return packet;
}

static void attachHeRxVector(Packet *packet, const Ieee80211Transmission *transmission,
        std::optional<size_t> userIndex, std::optional<uint16_t> staId, B receivedPsduLength)
{
    auto txVector = transmission->getHeTxVector();
    auto layout = transmission->getHePpduLayout();
    if (!txVector || !layout)
        throw cRuntimeError("HE reception is missing its canonical TXVECTOR/PPDU layout");
    Ieee80211HeRxVectorReconstructionRequest request;
    request.selection.userIndex = userIndex;
    request.selection.staId = staId;
    request.receivedPsduLength = receivedPsduLength;
    auto result = Ieee80211HeRxVectorFactory::reconstruct(*txVector, *layout, request);
    if (!result)
        throw cRuntimeError("Cannot reconstruct HE RXVECTOR: %s (%s)",
                result.getContext().fieldName.c_str(), result.getContext().detail.c_str());
    packet->addTag<Ieee80211HeRxVectorInd>()->setRxVector(result.getRxVector());
}

static B getObservedHePsduLength(const Packet *packet)
{
    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
            Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED |
            Chunk::PF_ALLOW_REINTERPRETATION;
    auto packetCopy = packet->dup();
    packetCopy->popAtFront<Ieee80211HePhyHeader>(b(-1), parsingFlags);
    auto bitLength = packetCopy->getDataLength().get<b>();
    delete packetCopy;
    if (bitLength % 8 != 0)
        throw cRuntimeError("Received HE PSDU length is not byte aligned");
    return B(bitLength / 8);
}

static Packet *buildLegacyPreambleIndication(const Ptr<const Ieee80211HePhyHeader>& phyHeader, const IReception *reception)
{
    auto packet = new Packet("HE-MU-Legacy-Preamble");
    auto phyHeaderCopy = copyHeMuPhyHeader(phyHeader);
    phyHeaderCopy->setLengthField(B(0));
    packet->insertAtFront(phyHeaderCopy);
    packet->addTagIfAbsent<Ieee80211LegacyPreambleInd>()->setDurationField(reception->getTransmission()->getDuration());
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
        const char *bandName = par("bandName");
        setBand(*bandName != '\0' ? Ieee80211CompliantBands::getBand(bandName) : nullptr);
        setModeSet(*opMode && strcmp(opMode, "ax") ? Ieee80211ModeSet::getModeSet(opMode) : nullptr);
        int channelNumber = par("channelNumber");
        if (channelNumber != -1)
            setChannelNumber(channelNumber);
        enableSpatialReuse = par("enableSpatialReuse");
        obssPdThreshold = mW(math::dBmW2mW(par("obssPdThreshold")));
        nonSrgObssPdThreshold = mW(math::dBmW2mW(par("nonSrgObssPdThreshold")));
        srgObssPdThreshold = mW(math::dBmW2mW(par("srgObssPdThreshold")));
        htCca20Sensitivity = mW(math::dBmW2mW(par("htCca20Sensitivity")));
        htCca40Sensitivity = mW(math::dBmW2mW(par("htCca40Sensitivity")));
        htCcaEnergyDetection = mW(math::dBmW2mW(par("htCcaEnergyDetection")));
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

void Ieee80211Receiver::recordHeSpatialReuseDecision(const HeSpatialReuseDecision& decision, bool emitSignals) const
{
    lastSpatialReuseBssType = (int)decision.bssType;
    lastSpatialReuseEligible = decision.eligible;
    lastSpatialReuseIgnoredPpdu = decision.ignorePpdu;
    lastSpatialReuseObssPdThreshold = decision.obssPdThreshold;
    lastSpatialReuseTransmitPowerLimit = decision.transmitPowerLimit;
    lastSpatialReuseReason = decision.reason == nullptr ? "" : decision.reason;
    if (emitSignals) {
        auto self = const_cast<Ieee80211Receiver *>(this);
        self->emit(heSpatialReuseBssTypeSignal, (long)decision.bssType);
        self->emit(heSpatialReuseReceivedBssColorSignal, (long)decision.receivedBssColor);
        self->emit(heSpatialReuseLocalBssColorSignal, (long)decision.localBssColor);
        self->emit(heSpatialReuseReceivedPowerSignal,
            std::isnan(decision.receivedPower.get()) ? NaN : math::mW2dBmW(decision.receivedPower.get<mW>()));
        self->emit(heSpatialReuseEligibleSignal, decision.eligible ? 1L : 0L);
        self->emit(heSpatialReuseIgnoredPpduSignal, decision.ignorePpdu ? 1L : 0L);
        self->emit(heSpatialReuseObssPdThresholdSignal,
                std::isnan(decision.obssPdThreshold.get()) ? NaN : math::mW2dBmW(decision.obssPdThreshold.get<mW>()));
        self->emit(heSpatialReuseTransmitPowerLimitSignal,
                std::isnan(decision.transmitPowerLimit.get()) ? NaN : math::mW2dBmW(decision.transmitPowerLimit.get<mW>()));
        self->emit(heSpatialReuseReasonSignal, (long)decision.reasonCode);
    }
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
    auto allocationPhyHeader = peekHeMuOrTbPhyHeader(transmission);
    if (allocationPhyHeader == nullptr)
        return true;
    // HE TB is received by the AP as the addressed receiver of the Trigger
    // exchange; DL HE MU needs the STA-ID match from HE-SIG-B User fields
    // (Clause 27.3.2.5 and Clause 27.3.11.8.4).
    if (dynamicPtrCast<const Ieee80211HeTbPhyHeader>(allocationPhyHeader) != nullptr)
        return true;
    auto networkInterface = getContainingNicModule(this);
    auto staId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
    return staId.has_value() && containsHeMuUser(allocationPhyHeader, *staId);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    auto allocationPhyHeader = peekHeMuOrTbPhyHeader(transmission);
    if (allocationPhyHeader != nullptr)
        return ieee80211Transmission && allocationPhyHeader->getUsersArraySize() > 0 &&
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
    auto allocationPhyHeader = peekHeMuOrTbPhyHeader(reception->getTransmission());
    if (shouldIgnoreReceptionDueToHeSpatialReuse(listening, reception, true))
        return false;
    if (allocationPhyHeader != nullptr) {
        auto ruSensitivity = getHeRuAdjustedPowerThreshold(reception, ieee80211Transmission, sensitivity);
        return ieee80211Transmission && allocationPhyHeader->getUsersArraySize() > 0 &&
               getAnalogModel()->computeIsReceptionPossible(listening, reception, ruSensitivity);
    }
    // Same non-HE mode-set gate as above; this path evaluates the concrete
    // reception interval against the receiver sensitivity.
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) &&
           getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
}

bool Ieee80211Receiver::computeIsReceptionAttempted(const IListening *listening, const IReception *reception,
        IRadioSignal::SignalPart part, const IInterference *interference) const
{
    auto allocationPhyHeader = peekHeMuOrTbPhyHeader(reception->getTransmission());
    if (shouldIgnoreReceptionDueToHeSpatialReuse(listening, reception, false))
        return false;
    if (dynamicPtrCast<const Ieee80211HeTbPhyHeader>(allocationPhyHeader) == nullptr)
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
    auto currentHeader = peekHeMuOrTbPhyHeader(currentTransmission);
    return dynamicPtrCast<const Ieee80211HeTbPhyHeader>(currentHeader) != nullptr &&
           currentHeader->getTriggerId() == allocationPhyHeader->getTriggerId();
}

namespace {

class FilteredInterferenceView : public IInterference
{
  protected:
    const INoise *backgroundNoise;
    const std::vector<const IReception *> *interferingReceptions;

  public:
    FilteredInterferenceView(const INoise *backgroundNoise, const std::vector<const IReception *> *interferingReceptions) :
        backgroundNoise(backgroundNoise), interferingReceptions(interferingReceptions) {}

    virtual const INoise *getBackgroundNoise() const override { return backgroundNoise; }
    virtual const std::vector<const IReception *> *getInterferingReceptions() const override { return interferingReceptions; }
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override
    {
        return stream << "FilteredInterferenceView";
    }
};

} // namespace

const IListeningDecision *Ieee80211Receiver::computeListeningDecision(const IListening *listening, const IInterference *interference) const
{
    // Clause 26.11 permits an HE STA to reset PHY-CCA for an eligible
    // inter-BSS PPDU below its OBSS/PD threshold. Candidate-reception
    // filtering alone is insufficient: such a PPDU normally contributes to
    // the radio's aggregate listening interference instead of becoming the
    // selected reception. Remove only eligible ignored PPDUs from this CCA
    // view; they remain in reception interference and can still corrupt a
    // concurrent local transmission.
    std::vector<const IReception *> ccaReceptions;
    for (auto reception : *interference->getInterferingReceptions()) {
        auto decision = computeHeSpatialReuseDecision(listening, reception);
        recordHeSpatialReuseDecision(decision, true);
        if (!decision.ignorePpdu)
            ccaReceptions.push_back(reception);
    }
    FilteredInterferenceView ccaInterference(interference->getBackgroundNoise(), &ccaReceptions);
    if (isHtCcaOperation() && dynamic_cast<const BandListening *>(listening) != nullptr &&
            dynamic_cast<const BandListening *>(listening)->getBandwidth() == MHz(20))
        return new ListeningDecision(listening, computeHtCcaBusy(listening, &ccaInterference));
    return FlatReceiverBase::computeListeningDecision(listening, &ccaInterference);
}

bool Ieee80211Receiver::isHtCcaOperation() const
{
    return modeSet != nullptr && Ieee80211ModeSet::isHtProfileName(modeSet->getProfileName()) &&
            channel != nullptr && (bandwidth == MHz(20) ||
            (bandwidth == MHz(40) && channel->getSecondaryChannelOffset() != IEEE80211_SECONDARY_CHANNEL_NONE));
}

static bool isBandOverlapping(const BandListening *listening, const INarrowbandSignalAnalogModel *signal)
{
    auto listeningMin = listening->getCenterFrequency() - listening->getBandwidth() / 2;
    auto listeningMax = listening->getCenterFrequency() + listening->getBandwidth() / 2;
    auto signalMin = signal->getCenterFrequency() - signal->getBandwidth() / 2;
    auto signalMax = signal->getCenterFrequency() + signal->getBandwidth() / 2;
    return signalMin <= listeningMax && signalMax >= listeningMin;
}

static bool isPrimaryChannel(const Ieee80211Channel *channel, const BandListening *listening)
{
    return channel != nullptr && listening->getCenterFrequency() == channel->getCenterFrequency();
}

static bool isSecondaryChannel(const Ieee80211Channel *channel, const BandListening *listening)
{
    return channel != nullptr && channel->getSecondaryChannelOffset() != IEEE80211_SECONDARY_CHANNEL_NONE &&
            listening->getCenterFrequency() == channel->getSecondaryCenterFrequency();
}

static bool isHt40SignalOccupyingChannel(const Ieee80211Channel *channel, const INarrowbandSignalAnalogModel *signal)
{
    return channel != nullptr && signal->getBandwidth() == MHz(40) &&
            signal->getCenterFrequency() == channel->getBondedCenterFrequency();
}

bool Ieee80211Receiver::computeHtCcaBusy(const IListening *listening, const IInterference *interference) const
{
    const auto *bandListening = check_and_cast<const BandListening *>(listening);
    const auto *mediumAnalogModel = listening->getReceiverRadio()->getMedium()->getAnalogModel();
    const INoise *noise = mediumAnalogModel->computeNoise(listening, interference);
    bool busy = noise->computeMaxPower(listening->getStartTime(), listening->getEndTime()) >= htCcaEnergyDetection;
    delete noise;
    if (busy)
        return true;

    const bool primary = isPrimaryChannel(channel, bandListening);
    const bool secondary = isSecondaryChannel(channel, bandListening);
    if (!primary && !secondary)
        return false;

    for (auto reception : *interference->getInterferingReceptions()) {
        const auto *transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
        const auto *signal = dynamic_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel());
        if (transmission == nullptr || transmission->getMode() == nullptr || signal == nullptr ||
                !modeSet->containsMode(transmission->getMode()) || !isBandOverlapping(bandListening, signal))
            continue;

        const W signalPower = signal->computeMinPower(reception->getStartTime(), reception->getEndTime());
        const auto family = modeSet->getPhyFamily(transmission->getMode());
        const Hz signalBandwidth = transmission->getMode()->getDataMode()->getBandwidth();
        if (family == Ieee80211PhyFamily::HT) {
            // IEEE Std 802.11-2024, 19.3.19.6.4 and 19.3.19.6.5:
            // a 20 MHz HT signal is detected on the primary channel at
            // -82 dBm; a 40 MHz HT signal is detected on each occupied
            // channel at -79 dBm. The comparison uses the received signal
            // level over the PPDU bandwidth, not the power apportioned to a
            // 20 MHz slice by the analog interference model.
            if (signalBandwidth == MHz(40) && isHtCcaOperation() &&
                    isHt40SignalOccupyingChannel(channel, signal) && signalPower >= htCca40Sensitivity)
                return true;
            if (signalBandwidth == MHz(20) && primary && signalPower >= htCca20Sensitivity)
                return true;
        }
        else if (primary && (family == Ieee80211PhyFamily::OFDM || family == Ieee80211PhyFamily::ERP_OFDM) &&
                signalPower >= htCca20Sensitivity) {
            // Clause 19.3.19.6.3 delegates non-HT CCA to the OFDM/ERP-OFDM
            // preamble-detection requirement, which uses the 20 MHz
            // sensitivity threshold.
            return true;
        }
    }
    return false;
}

bool Ieee80211Receiver::computeIsReceptionSuccessful(const IListening *listening,
        const IReception *reception, IRadioSignal::SignalPart part,
        const IInterference *interference, const ISnir *snir) const
{
    auto transmission = dynamic_cast<const Ieee80211Transmission *>(
            reception == nullptr ? nullptr : reception->getTransmission());
    auto layout = transmission == nullptr ? nullptr : transmission->getHePpduLayout();
    if (part == IRadioSignal::SIGNAL_PART_DATA &&
            peekHeMuOrTbPhyHeader(reception->getTransmission()) != nullptr &&
            layout != nullptr && !layout->isNdp() &&
            dynamic_cast<const Ieee80211ErrorModelBase *>(errorModel) != nullptr)
        // The common receiver still applies the deterministic physical SNIR
        // gate. Analytical HE data errors are resolved once per MPDU later,
        // avoiding an aggregate packet-error draw followed by post-hoc failure
        // placement.
        return SnirReceiverBase::computeIsReceptionSuccessful(
                listening, reception, part, interference, snir);
    return FlatReceiverBase::computeIsReceptionSuccessful(
            listening, reception, part, interference, snir);
}

bool Ieee80211Receiver::shouldIgnoreReceptionDueToHeSpatialReuse(const IListening *listening, const IReception *reception, bool logDecision) const
{
    auto spatialReuseDecision = computeHeSpatialReuseDecision(listening, reception);
    recordHeSpatialReuseDecision(spatialReuseDecision, logDecision);
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
    if (auto narrowbandReception = dynamic_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel()))
        decision.receivedPower = narrowbandReception->computeMinPower(reception->getStartTime(), reception->getEndTime());
    auto hePhyHeader = peekHePhyHeader(reception->getTransmission());
    if (!enableSpatialReuse) {
        decision.reason = "spatial reuse disabled";
        return decision;
    }
    if (hePhyHeader == nullptr) {
        decision.reasonCode = HeSpatialReuseReason::NOT_HE_PPDU;
        decision.reason = "not an HE PPDU";
        return decision;
    }
    auto receivedBssColor = hePhyHeader->getBssColor();
    decision.receivedBssColor = receivedBssColor;
    // Table 27-21/27-22 carries BSS Color in HE-SIG-A. Color 0 disables BSS
    // coloring, so OBSS/PD classification cannot be applied.
    if (receivedBssColor == 0) {
        decision.reasonCode = HeSpatialReuseReason::RECEIVED_COLOR_DISABLED;
        decision.reason = "received BSS color disabled";
        return decision;
    }
    auto radio = dynamic_cast<const IIeee80211HePacketRadio *>(getParentModule());
    if (radio != nullptr)
        decision.localBssColor = radio->getHeBssColor();
    if (radio == nullptr || radio->getHeBssColor() == 0) {
        decision.reasonCode = HeSpatialReuseReason::LOCAL_COLOR_DISABLED;
        decision.reason = "local BSS color disabled";
        return decision;
    }
    if (receivedBssColor == radio->getHeBssColor()) {
        // Clause 26.11 spatial reuse applies to inter-BSS PPDUs; same-color
        // PPDUs remain intra-BSS and are not ignored by OBSS/PD.
        decision.bssType = HeSpatialReuseBssType::INTRA_BSS;
        decision.reasonCode = HeSpatialReuseReason::INTRA_BSS_PPDU;
        decision.reason = "intra-BSS PPDU";
        return decision;
    }

    bool isSrg = srgBssColors.find(receivedBssColor) != srgBssColors.end();
    decision.bssType = isSrg ? HeSpatialReuseBssType::INTER_BSS_SRG : HeSpatialReuseBssType::INTER_BSS_NON_SRG;
    if (isSrg) {
        if (!enableSrgSpatialReuse) {
            decision.reasonCode = HeSpatialReuseReason::SRG_DISABLED;
            decision.reason = "SRG OBSS/PD disabled";
            return decision;
        }
        if (hePhyHeader->getSrgObssPdDisallowed()) {
            decision.reasonCode = HeSpatialReuseReason::SRG_DISALLOWED;
            decision.reason = "PPDU disallows SRG OBSS/PD";
            return decision;
        }
        decision.obssPdThreshold = srgObssPdThreshold;
    }
    else {
        if (!enableNonSrgSpatialReuse) {
            decision.reasonCode = HeSpatialReuseReason::NON_SRG_DISABLED;
            decision.reason = "non-SRG OBSS/PD disabled";
            return decision;
        }
        if (hePhyHeader->getNonSrgObssPdDisallowed()) {
            decision.reasonCode = HeSpatialReuseReason::NON_SRG_DISALLOWED;
            decision.reason = "PPDU disallows non-SRG OBSS/PD";
            return decision;
        }
        decision.obssPdThreshold = nonSrgObssPdThreshold;
    }

    if (dynamicPtrCast<const Ieee80211HeTbPhyHeader>(hePhyHeader) != nullptr) {
        // Table 27-24 defines HE TB Spatial Reuse values. This branch models
        // parameterized spatial reuse only when the PPDU permits it.
        if (!enableParameterizedSpatialReuse) {
            decision.reasonCode = HeSpatialReuseReason::TB_EXCLUDED;
            decision.reason = "HE TB PPDU excluded from OBSS/PD";
            return decision;
        }
        if (hePhyHeader->getPsrDisallowed() || hePhyHeader->getSpatialReuse() == 0) {
            decision.reasonCode = HeSpatialReuseReason::PSR_NOT_PERMITTED;
            decision.reason = "PSR not permitted by PPDU";
            return decision;
        }
    }

    decision.eligible = true;
    decision.transmitPowerLimit = computeSpatialReuseTransmitPowerLimit(decision.obssPdThreshold);
    auto transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto ruObssPdThreshold = getHeRuAdjustedPowerThreshold(reception, transmission, decision.obssPdThreshold);
    decision.ignorePpdu = !getAnalogModel()->computeIsReceptionPossible(listening, reception, ruObssPdThreshold);
    decision.reason = decision.ignorePpdu ?
            (isSrg ? "inter-BSS SRG PPDU below OBSS/PD" : "inter-BSS non-SRG PPDU below OBSS/PD") :
            (isSrg ? "inter-BSS SRG PPDU at or above OBSS/PD" : "inter-BSS non-SRG PPDU at or above OBSS/PD");
    decision.reasonCode = decision.ignorePpdu ? HeSpatialReuseReason::INTER_BSS_BELOW_OBSS_PD :
            HeSpatialReuseReason::INTER_BSS_AT_OR_ABOVE_OBSS_PD;
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
    auto hePhyHeader = peekHePhyHeader(transmission);
    const auto& vhtTxVector = transmission->getVhtTxVector();
    if (hePhyHeader != nullptr) {
        lastHeReception = true;
        lastHePpduFormat = getIeee80211HePpduFormat(*hePhyHeader);
        lastHeUserCount = hePhyHeader->getUsersArraySize();
        lastHeBssColor = hePhyHeader->getBssColor();
        lastHeRuAssigned = false;
    }
    else {
        lastHeReception = false;
        lastHePpduFormat = -1;
        lastHeUserCount = 0;
        lastHeBssColor = 0;
        lastHeRuAssigned = false;
    }

    if (vhtTxVector != nullptr && vhtTxVector->isMu()) {
        auto radio = dynamic_cast<const IIeee80211VhtPacketRadio *>(getParentModule());
        auto user = resolveVhtMuUserForReception(transmission, radio);
        auto packet = user == nullptr ? buildVhtMuNonmemberIndication(transmission) :
                extractVhtMuPsdu(transmission, *user);
        if (packet == nullptr) {
            packet = buildVhtMuNonmemberIndication(transmission);
            user = nullptr;
        }
        if (user != nullptr && !computeIsVhtMuUserReceptionSuccessful(
                transmission, *user, decisions))
            packet->setBitError(true);
        addReceptionIndications(packet, reception, interference, snir);
        packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
        packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
        return new ReceptionResult(reception, decisions, packet);
    }

    auto allocationPhyHeader = peekHeMuOrTbPhyHeader(transmission);
    if (allocationPhyHeader != nullptr) {
        auto layout = transmission->getHePpduLayout();
        auto heErrorModel = dynamic_cast<const Ieee80211ErrorModelBase *>(errorModel);
        if (layout != nullptr && !layout->isNdp() && errorModel != nullptr && heErrorModel == nullptr)
            throw cRuntimeError("Configured IEEE 802.11 error model does not support per-MPDU HE outcomes");
        if (dynamicPtrCast<const Ieee80211HeTbPhyHeader>(allocationPhyHeader) != nullptr) {
            lastHeRuAssigned = true;
            size_t selectedUserIndex = 0;
            if (!layout)
                throw cRuntimeError("Packet-level HE TB reception requires a canonical layout");
            if (layout->isNdp()) {
                if (layout->getUsers().size() != 1)
                    throw cRuntimeError("Packet-level HE TB feedback NDP requires exactly one canonical user");
            }
            else {
                std::optional<size_t> activeUserIndex;
                for (const auto& range : layout->getPsduBitRanges()) {
                    if (range.getBitLength() == b(0))
                        continue;
                    if (activeUserIndex)
                        throw cRuntimeError("One HE TB transmission cannot carry multiple active PSDUs");
                    activeUserIndex = range.getUserIndex();
                }
                if (!activeUserIndex)
                    throw cRuntimeError("HE TB data transmission has no active PSDU user");
                selectedUserIndex = *activeUserIndex;
            }
            const auto& activeUser = layout->getUsers().at(selectedUserIndex);
            auto packet = layout->isNdp() ? transmittedPacket->dup() :
                    buildHeMuPhyPacket(transmission, allocationPhyHeader,
                            activeUser.staId, selectedUserIndex);
            bool decodedPsdu = packet != nullptr;
            if (packet == nullptr) {
                packet = transmittedPacket->dup();
                packet->clearTags();
                packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);
            }
            else if (layout->isNdp()) {
                packet->clearTags();
                packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);
            }
            bool successful = !decodedPsdu ? false : layout->isNdp() ? isReceptionSuccessful(decisions) :
                    applyHeMpduReceiveOutcomes(packet, decisions, heErrorModel, snir,
                            selectedUserIndex, getRNG(0));
            if (!successful)
                packet->setBitError(true);
            addReceptionIndications(packet, reception, interference, snir);
            packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
            packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
            if (transmission->getHeTriggerCorrelationId() != 0)
                packet->addTag<Ieee80211HeTriggerCorrelationTag>()->
                        setTriggerId(transmission->getHeTriggerCorrelationId());
            attachHeRxVector(packet, transmission, selectedUserIndex, activeUser.staId,
                    getObservedHePsduLength(packet));
            auto recipientParameters = std::shared_ptr<const Ieee80211HeUserPhyParameters>(
                    layout, &activeUser);
            auto recipientContext = packet->addTag<Ieee80211HeTbRecipientContextInd>();
            if (allocationPhyHeader->getTriggerId() != 0)
                recipientContext->setTriggerId(allocationPhyHeader->getTriggerId());
            recipientContext->setRecipientParameters(std::move(recipientParameters));
            return new ReceptionResult(reception, decisions, packet);
        }
        auto networkInterface = getContainingNicModule(this);
        auto myStaId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
        lastHeRuAssigned = myStaId.has_value() && containsHeMuUser(allocationPhyHeader, *myStaId);
        size_t selectedUserIndex = 0;
        bool decodedPsdu = myStaId.has_value() && findHePsduRange(transmission, *myStaId) != nullptr &&
                modeSet->containsMode(transmission->getMode());
        auto packet = decodedPsdu
                ? buildHeMuPhyPacket(transmission, allocationPhyHeader, *myStaId, selectedUserIndex)
                : buildLegacyPreambleIndication(allocationPhyHeader, reception);
        if (packet == nullptr) {
            decodedPsdu = false;
            packet = buildLegacyPreambleIndication(allocationPhyHeader, reception);
        }
        if (decodedPsdu)
            attachHeRxVector(packet, transmission, selectedUserIndex, myStaId,
                    getObservedHePsduLength(packet));
        if (!applyHeMpduReceiveOutcomes(packet, decisions, heErrorModel, snir,
                selectedUserIndex, getRNG(0)))
            packet->setBitError(true);
        addReceptionIndications(packet, reception, interference, snir);
        packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
        packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
        if (transmission->getHeTriggerCorrelationId() != 0)
            packet->addTag<Ieee80211HeTriggerCorrelationTag>()->
                    setTriggerId(transmission->getHeTriggerCorrelationId());
        return new ReceptionResult(reception, decisions, packet);
    }

    // Single-user and non-HE PPDU reception is packet-level: the standard-specific durations,
    // header fields, and padding are established in the mode/radio/transmitter
    // code, while this receiver reports the selected PHY mode and channel with
    // the decoded payload.
    auto receptionResult = FlatReceiverBase::computeReceptionResult(listening, reception, interference, snir, decisions);
    auto packet = const_cast<Packet *>(receptionResult->getPacket());
    addReceptionIndications(packet, reception, interference, snir);
    packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
    packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
    if (transmission->getHeTriggerCorrelationId() != 0)
        packet->addTag<Ieee80211HeTriggerCorrelationTag>()->
                setTriggerId(transmission->getHeTriggerCorrelationId());
    if (hePhyHeader != nullptr)
        attachHeRxVector(packet, transmission, {}, {}, getObservedHePsduLength(packet));
    return receptionResult;
}

void Ieee80211Receiver::setModeSet(const Ieee80211ModeSet *modeSet)
{
    this->modeSet = modeSet;
}

void Ieee80211Receiver::setBand(const IIeee80211Band *band)
{
    if (this->band != band) {
        if (channel != nullptr)
            setChannel(new Ieee80211Channel(band, channel->getChannelNumber(), channel->getSecondaryChannelOffset()));
        else
            this->band = band;
    }
}

void Ieee80211Receiver::setChannel(const Ieee80211Channel *channel)
{
    if (this->channel != channel) {
        // IEEE Std 802.11-2024, 19.3.15.4 and 19.3.19.6.5: HT40 listening
        // spans both 20 MHz channels around their bonded center.
        auto centerFrequency = channel->getSecondaryChannelOffset() == IEEE80211_SECONDARY_CHANNEL_NONE ?
                channel->getCenterFrequency() : channel->getBondedCenterFrequency();
        delete this->channel;
        this->channel = channel;
        this->band = channel->getBand();
        setCenterFrequency(centerFrequency);
    }
}

void Ieee80211Receiver::setChannelNumber(int channelNumber)
{
    if (channel == nullptr || channelNumber != channel->getChannelNumber())
        setChannel(new Ieee80211Channel(band, channelNumber, channel == nullptr ?
                IEEE80211_SECONDARY_CHANNEL_NONE : channel->getSecondaryChannelOffset()));
}

} // namespace physicallayer

} // namespace inet
