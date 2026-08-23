//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211LdpcSoftReceiver.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "inet/common/packet/chunk/BitsChunk.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ReceptionDecision.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ReceivedDataEncodingPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211LdpcSoftReceiver);

simsignal_t Ieee80211LdpcSoftReceiver::ldpcDataDecodeAttemptedSignal =
        cComponent::registerSignal("ldpcDataDecodeAttempted");
simsignal_t Ieee80211LdpcSoftReceiver::ldpcDataDecodeSucceededSignal =
        cComponent::registerSignal("ldpcDataDecodeSucceeded");
simsignal_t Ieee80211LdpcSoftReceiver::ldpcDataDecodeIterationsSignal =
        cComponent::registerSignal("ldpcDataDecodeIterations");

namespace {

bool isMixedHtPreamble(const IIeee80211Mode *mode)
{
    auto preamble = dynamic_cast<const Ieee80211HtPreambleMode *>(mode->getPreambleMode());
    return preamble != nullptr && preamble->getPreambleFormat() == Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED;
}

bool isMixedVhtPreamble(const IIeee80211Mode *mode)
{
    auto preamble = dynamic_cast<const Ieee80211VhtPreambleMode *>(mode->getPreambleMode());
    return preamble != nullptr && preamble->getPreambleFormat() == Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED;
}

int getBandwidthMhz(Hz bandwidth)
{
    int bandwidthMhz = static_cast<int>(std::llround(bandwidth.get() / 1e6));
    if (std::fabs(bandwidth.get() - bandwidthMhz * 1e6) > 0.5)
        throw cRuntimeError("IEEE 802.11 LDPC soft receiver requires an integral MHz bandwidth");
    return bandwidthMhz;
}

Hz getVhtBandwidth(unsigned int bandwidthCode)
{
    switch (bandwidthCode) {
        case 0: return MHz(20);
        case 1: return MHz(40);
        case 2: return MHz(80);
        case 3: return MHz(160);
        default: throw cRuntimeError("Invalid VHT-SIG-A bandwidth code %u", bandwidthCode);
    }
}

std::vector<const ApskModulationBase *> getStreamSubcarrierModulations(
        const IIeee80211DataMode *dataMode)
{
    std::vector<const Ieee80211OfdmModulation *> ofdmModulations;
    if (auto htDataMode = dynamic_cast<const Ieee80211HtDataMode *>(dataMode)) {
        const auto *mcs = htDataMode->getModulationAndCodingScheme();
        ofdmModulations = {mcs->getModulation(), mcs->getStreamExtension1Modulation(),
                mcs->getStreamExtension2Modulation(), mcs->getStreamExtension3Modulation()};
    }
    else if (auto vhtDataMode = dynamic_cast<const Ieee80211VhtDataMode *>(dataMode)) {
        const auto *mcs = vhtDataMode->getModulationAndCodingScheme();
        ofdmModulations = {mcs->getModulation(), mcs->getStreamExtension1Modulation(),
                mcs->getStreamExtension2Modulation(), mcs->getStreamExtension3Modulation(),
                mcs->getStreamExtension4Modulation(), mcs->getStreamExtension5Modulation(),
                mcs->getStreamExtension6Modulation(), mcs->getStreamExtension7Modulation()};
    }
    else
        throw cRuntimeError("IEEE 802.11 LDPC soft receiver requires an HT or VHT data mode");

    int numberOfStreams = dataMode->getNumberOfSpatialStreams();
    if (numberOfStreams < 1 || numberOfStreams > static_cast<int>(ofdmModulations.size()))
        throw cRuntimeError("Invalid IEEE 802.11 LDPC spatial-stream count %d", numberOfStreams);
    std::vector<const ApskModulationBase *> result;
    result.reserve(numberOfStreams);
    for (int stream = 0; stream < numberOfStreams; stream++) {
        if (ofdmModulations[stream] == nullptr || ofdmModulations[stream]->getSubcarrierModulation() == nullptr)
            throw cRuntimeError("IEEE 802.11 LDPC mode has no modulation for spatial stream %d", stream + 1);
        result.push_back(ofdmModulations[stream]->getSubcarrierModulation());
    }
    return result;
}

bool appendProtectedVhtSigBBits(const Ieee80211VhtPhyHeader *header, std::vector<bool>& bits)
{
    auto layout = getVhtSuSigBLayout(header->getBandwidth());
    if (header->getVhtSigBLength() >= (1U << layout.lengthFieldWidth) ||
        header->getVhtSigBReserved() >= (1U << layout.reservedFieldWidth))
        return false;
    for (int i = 0; i < layout.lengthFieldWidth; i++)
        bits.push_back((header->getVhtSigBLength() >> i) & 1);
    for (int i = 0; i < layout.reservedFieldWidth; i++)
        bits.push_back((header->getVhtSigBReserved() >> i) & 1);
    return true;
}

double clampLlr(double value, double maximumLlr)
{
    if (!std::isfinite(value))
        return value < 0 ? -maximumLlr : maximumLlr;
    return std::max(-maximumLlr, std::min(maximumLlr, value));
}

} // namespace

void Ieee80211LdpcSoftReceiver::initialize(int stage)
{
    Ieee80211Receiver::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        const char *algorithm = par("decoderAlgorithm");
        if (!strcmp(algorithm, "sumProduct"))
            decodingAlgorithm = LdpcDecodingAlgorithm::SUM_PRODUCT;
        else if (!strcmp(algorithm, "normalizedMinSum"))
            decodingAlgorithm = LdpcDecodingAlgorithm::NORMALIZED_MIN_SUM;
        else
            throw cRuntimeError("Unknown IEEE 802.11 LDPC decoder algorithm '%s'", algorithm);
        maxIterations = par("maxIterations");
        normalizedMinSumFactor = par("normalizedMinSumFactor");
        maximumLlr = par("maximumLlr");
        if (maxIterations <= 0 || !(normalizedMinSumFactor > 0 && normalizedMinSumFactor <= 1) ||
            !(maximumLlr > 0) || !std::isfinite(normalizedMinSumFactor) || !std::isfinite(maximumLlr))
            throw cRuntimeError("Invalid IEEE 802.11 LDPC soft decoder parameters");
    }
}

uint8_t Ieee80211LdpcSoftReceiver::computeVhtSigBCrc(const Ieee80211VhtPhyHeader *header)
{
    std::vector<bool> protectedBits;
    if (!appendProtectedVhtSigBBits(header, protectedBits))
        throw cRuntimeError("Invalid VHT-SIG-B protected fields");
    return Ieee80211LdpcDataPipeline::computeVhtSigBCrc(protectedBits);
}

bool Ieee80211LdpcSoftReceiver::resolveReceptionContext(const Packet *packet,
        const Ieee80211ModeSet *modeSet, simtime_t dataDuration,
        ReceptionContext& context, int mappedSymbolCount)
{
    if (packet == nullptr || modeSet == nullptr || dataDuration <= SIMTIME_ZERO)
        return false;
    context = ReceptionContext();
    try {
        auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet,
                b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
        if (phyHeader == nullptr || phyHeader->isIncorrect() || phyHeader->isIncomplete() ||
            phyHeader->isImproperlyRepresented())
            return false;

        Ieee80211PhyFormat phyFormat;
        unsigned int mcs;
        unsigned int nss;
        int psduOctets = -1;
        Hz bandwidth;
        bool shortGi;
        if (auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader)) {
            // HT MCS 0..31 encodes NSS=(MCS/8)+1 (IEEE Std 802.11-2024,
            // 19.3.11.7.6). MCS32 and extension spatial streams are
            // deliberately outside this exact SU path.
            if (!htHeader->getFecCoding() || htHeader->getStbc() != 0 || htHeader->getMcs() > 31 ||
                !htHeader->getReserved() ||
                htHeader->getNumberOfExtensionSpatialStreams() != 0)
                return false;
            phyFormat = Ieee80211PhyFormat::HT;
            mcs = htHeader->getMcs();
            nss = mcs / 8 + 1;
            bandwidth = htHeader->getChannelWidth40() ? MHz(40) : MHz(20);
            shortGi = htHeader->getShortGi();
            psduOctets = htHeader->getLengthField().get<B>();
        }
        else if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader)) {
            // VHT-SIG-A/SIG-B fields are interpreted per 21.3.10.6-.9 and
            // Table 21-14; the unaligned APEP rule is Eq. 21-46. The
            // non-wire base lengthField is deliberately ignored here.
            if (!vhtHeader->getCoding() ||
                (vhtHeader->getGroupId() != 0 && vhtHeader->getGroupId() != 63) ||
                vhtHeader->getPartialAid() > 511 || vhtHeader->getStbc() || vhtHeader->getBeamformed() ||
                vhtHeader->getMcs() > 9 ||
                !vhtHeader->getReserved1() || !vhtHeader->getReserved2() || !vhtHeader->getReserved3() ||
                vhtHeader->getVhtSigBTail() != 0 ||
                (!vhtHeader->getShortGi() && vhtHeader->getShortGiNsymDisambiguation()))
                return false;
            bandwidth = getVhtBandwidth(vhtHeader->getBandwidth());
            // The reserved VHT-SIG-B bits are fixed by the received
            // bandwidth layout (IEEE Std 802.11-2024, Table 21-14).
            if (vhtHeader->getVhtSigBReserved() !=
                    getVhtSuSigBLayout(vhtHeader->getBandwidth()).getReservedValue())
                return false;
            phyFormat = Ieee80211PhyFormat::VHT_SU;
            mcs = vhtHeader->getMcs();
            nss = vhtHeader->getNumberOfSpaceTimeStreams() + 1;
            shortGi = vhtHeader->getShortGi();
            context.isVht = true;
            context.vhtSigBCrc = computeVhtSigBCrc(vhtHeader.get());
        }
        else
            return false;

        auto mode = Ieee80211ModeSet::findMode(phyFormat, mcs, bandwidth, nss,
                Ieee80211FecType::LDPC, shortGi);
        if (mode == nullptr || !modeSet->containsMode(mode) || mode->getDataMode()->getFecType() != Ieee80211FecType::LDPC)
            return false;

        if (phyFormat == Ieee80211PhyFormat::HT) {
            auto dataMode = dynamic_cast<const Ieee80211HtDataMode *>(mode->getDataMode());
            if (dataMode == nullptr || dataMode->getMcsIndex() != mcs || dataMode->getBandwidth() != bandwidth ||
                dataMode->getNumberOfSpatialStreams() != static_cast<int>(nss) ||
                !isMixedHtPreamble(mode))
                return false;
        }
        else {
            auto dataMode = dynamic_cast<const Ieee80211VhtDataMode *>(mode->getDataMode());
            if (dataMode == nullptr || dataMode->getMcsIndex() != mcs || dataMode->getBandwidth() != bandwidth ||
                dataMode->getNumberOfSpatialStreams() != static_cast<int>(nss) ||
                !isMixedVhtPreamble(mode))
                return false;
        }

        // All consumers use the same receiver-authoritative duration/SIG
        // reconstruction. A mapped-symbol count is only an observation
        // consistency check and cannot replace received timing.
        auto receivedPlan = reconstructIeee80211ReceivedDataEncodingPlan(
                mode->getDataMode(), phyHeader, dataDuration);
        if (mappedSymbolCount >= 0 && mappedSymbolCount != receivedPlan.getNumberOfSymbols())
            return false;

        context.mode = mode;
        context.bandwidthMhz = getBandwidthMhz(bandwidth);
        context.streamModulations = getStreamSubcarrierModulations(mode->getDataMode());
        context.bitsPerSubcarrier.reserve(context.streamModulations.size());
        for (const auto *modulation : context.streamModulations)
            context.bitsPerSubcarrier.push_back(static_cast<int>(modulation->getCodeWordSize()));
        if (context.isVht) {
            psduOctets = (receivedPlan.getUncodedDataBits() - 16) / 8;
        }
        context.plan = std::make_unique<Ieee80211DataEncodingPlan>(std::move(receivedPlan));
        context.psduOctets = psduOctets;
        return context.plan->getFecType() == Ieee80211FecType::LDPC &&
               context.plan->getPhyFormat() == phyFormat;
    }
    catch (const cRuntimeError&) {
        return false;
    }
}

double Ieee80211LdpcSoftReceiver::computeBitLlr(const std::complex<double>& observation,
        const ApskModulationBase *modulation, double noiseSpectralDensity, int bit,
        double maximumLlr)
{
    if (modulation == nullptr || std::isnan(noiseSpectralDensity) || noiseSpectralDensity <= 0 ||
        !std::isfinite(maximumLlr) || maximumLlr <= 0)
        throw cRuntimeError("Invalid IEEE 802.11 LDPC soft demapper parameters");
    const auto *constellation = modulation->getConstellation();
    if (constellation == nullptr || bit < 0 || bit >= static_cast<int>(modulation->getCodeWordSize()))
        throw cRuntimeError("Invalid IEEE 802.11 LDPC soft demapper constellation or bit index");
    auto logSumExp = [&](bool bitValue) {
        double largest = -std::numeric_limits<double>::infinity();
        std::vector<double> metrics;
        for (size_t index = 0; index < constellation->size(); index++) {
            if (((index >> bit) & 1) != static_cast<size_t>(bitValue))
                continue;
            double metric = -std::norm(observation - (*constellation)[index]) / noiseSpectralDensity;
            metrics.push_back(metric);
            largest = std::max(largest, metric);
        }
        if (!std::isfinite(largest))
            return largest;
        double sum = 0;
        for (double metric : metrics)
            sum += std::exp(metric - largest);
        return largest + std::log(sum);
    };
    return clampLlr(logSumExp(false) - logSumExp(true), maximumLlr);
}

bool Ieee80211LdpcSoftReceiver::computeExactDataSuccess(const IReception *reception,
        const ISnir *snir, const Ieee80211LdpcSoftTransmissionModel *signalModel) const
{
    auto transmission = reception->getTransmission();
    auto cached = exactReceptionOutcomes.find(transmission->getId());
    if (cached != exactReceptionOutcomes.end())
        return cached->second.success;

    auto mutableThis = const_cast<Ieee80211LdpcSoftReceiver *>(this);
    mutableThis->emit(ldpcDataDecodeAttemptedSignal, 1L);
    ExactDecodeOutcome outcome;
    ReceptionContext context;
    if (!resolveReceptionContext(transmission->getPacket(), modeSet,
            transmission->getDataDuration(), context,
            static_cast<int>(signalModel->getMappedData().blocks.size())) ||
        context.plan == nullptr || signalModel == nullptr)
        throw cRuntimeError("IEEE 802.11 LDPC soft receiver cannot resolve a matching reception context");

    double snr = snir->getMean();
    if (std::isnan(snr) || snr < 0)
        throw cRuntimeError("IEEE 802.11 LDPC soft receiver requires a nonnegative mean SNIR");
    // Treat +infinity as the zero-noise limit.  A smallest-positive N0
    // makes the exact constellation point produce saturated, finite LLRs
    // without injecting a synthetic noise sample.
    double noiseSpectralDensity = std::isinf(snr) ? std::numeric_limits<double>::min() :
            snr == 0 ? std::numeric_limits<double>::infinity() : 1.0 / snr;
    double sigma = snr == 0 || std::isinf(snr) ? 0 : std::sqrt(noiseSpectralDensity / 2);
    const auto& mappedBits = signalModel->getMappedData().blocks;
    const auto& symbols = signalModel->getSymbols();
    if (mappedBits.size() != static_cast<size_t>(context.plan->getNumberOfSymbols()) || symbols.size() != mappedBits.size())
        throw cRuntimeError("IEEE 802.11 LDPC soft receiver mapped symbol count disagrees with the received plan");
    Ieee80211LdpcMappedReliabilities mapped(mappedBits.size());
    if (context.streamModulations.size() != context.bitsPerSubcarrier.size() ||
        context.streamModulations.empty())
        throw cRuntimeError("IEEE 802.11 LDPC soft receiver has inconsistent stream modulation metadata");
    size_t numberOfStreams = context.streamModulations.size();
    for (size_t symbol = 0; symbol < mappedBits.size(); symbol++) {
        if (mappedBits[symbol].size() != numberOfStreams || symbols[symbol].size() != numberOfStreams)
            throw cRuntimeError("IEEE 802.11 LDPC soft receiver stream count disagrees with the received plan");
        mapped[symbol].resize(numberOfStreams);
        for (size_t stream = 0; stream < numberOfStreams; stream++) {
            const auto *subcarrierModulation = context.streamModulations[stream];
            int bitsPerSubcarrier = context.bitsPerSubcarrier[stream];
            if (mappedBits[symbol][stream].size() != symbols[symbol][stream].size())
                throw cRuntimeError("IEEE 802.11 LDPC soft receiver frequency block shape mismatch");
            mapped[symbol][stream].resize(mappedBits[symbol][stream].size());
            for (size_t block = 0; block < mappedBits[symbol][stream].size(); block++) {
                const auto& blockBits = mappedBits[symbol][stream][block];
                const auto& blockSymbols = symbols[symbol][stream][block];
                if (blockBits.getSize() != blockSymbols.size() * static_cast<size_t>(bitsPerSubcarrier))
                    throw cRuntimeError("IEEE 802.11 LDPC soft receiver constellation dimensions mismatch");
                auto& reliabilities = mapped[symbol][stream][block];
                reliabilities.reserve(blockSymbols.size() * bitsPerSubcarrier);
                for (const auto& transmittedSymbol : blockSymbols) {
                    // All ideal separated streams use the same scalar mean
                    // SNIR, with independent AWGN draws in the deterministic
                    // symbol -> stream -> block -> point traversal order.
                    std::complex<double> observation = transmittedSymbol;
                    if (sigma != 0)
                        observation += std::complex<double>(normal(0, sigma), normal(0, sigma));
                    for (int bit = 0; bit < bitsPerSubcarrier; bit++)
                        reliabilities.push_back(computeBitLlr(observation, subcarrierModulation,
                                noiseSpectralDensity, bit, maximumLlr));
                }
            }
        }
    }
    Ieee80211LdpcDataCoder coder(decodingAlgorithm, maxIterations,
            normalizedMinSumFactor, maximumLlr);
    auto decoded = Ieee80211LdpcDataPipeline::inverseMapAndDecode(mapped, *context.plan,
            context.bitsPerSubcarrier, context.bandwidthMhz, context.psduOctets * 8,
            context.isVht, context.vhtSigBCrc, 1, coder, 0);
    outcome.iterations = decoded.iterations;
    outcome.success = decoded.converged;
    if (outcome.success)
        outcome.psduBits = std::move(decoded.psduBits);
    mutableThis->emit(ldpcDataDecodeIterationsSignal, static_cast<long>(outcome.iterations));
    if (outcome.success)
        mutableThis->emit(ldpcDataDecodeSucceededSignal, 1L);
    exactReceptionOutcomes[transmission->getId()] = outcome;
    EV_INFO << "IEEE 802.11 exact LDPC data decode: transmission=" << transmission->getId()
            << " converged=" << outcome.success << " iterations=" << outcome.iterations << EV_ENDL;
    return outcome.success;
}

bool Ieee80211LdpcSoftReceiver::computeExactPreambleOrHeaderSuccess(
        const IListening *listening, const IReception *reception,
        IRadioSignal::SignalPart part, const IInterference *interference,
        const ISnir *snir, const IIeee80211Mode *receiverMode) const
{
    if (!SnirReceiverBase::computeIsReceptionSuccessful(listening, reception, part,
            interference, snir))
        return false;
    if (errorModel == nullptr)
        return true;
    auto ieee80211ErrorModel = dynamic_cast<const Ieee80211ErrorModelBase *>(errorModel);
    if (ieee80211ErrorModel == nullptr)
        throw cRuntimeError("IEEE 802.11 LDPC soft reception requires an IEEE 802.11 error model for PHY signaling");
    double packetErrorRate = ieee80211ErrorModel->computeHeaderPacketErrorRate(snir, part,
            receiverMode);
    if (packetErrorRate == 0)
        return true;
    if (packetErrorRate == 1)
        return false;
    if (!(packetErrorRate > 0 && packetErrorRate < 1))
        throw cRuntimeError("Invalid IEEE 802.11 PHY-header packet error rate %g", packetErrorRate);
    return dblrand() > packetErrorRate;
}

bool Ieee80211LdpcSoftReceiver::computeIsReceptionPossible(const IListening *listening,
        const ITransmission *transmission) const
{
    auto signalModel = dynamic_cast<const Ieee80211LdpcSoftTransmissionModel *>(transmission->getBitModel());
    if (signalModel == nullptr)
        return Ieee80211Receiver::computeIsReceptionPossible(listening, transmission);
    ReceptionContext context;
    return resolveReceptionContext(transmission->getPacket(), modeSet,
            transmission->getDataDuration(), context,
            static_cast<int>(signalModel->getMappedData().blocks.size())) &&
           NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool Ieee80211LdpcSoftReceiver::computeIsReceptionPossible(const IListening *listening,
        const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto transmission = reception->getTransmission();
    auto signalModel = dynamic_cast<const Ieee80211LdpcSoftTransmissionModel *>(transmission->getBitModel());
    if (signalModel == nullptr)
        return Ieee80211Receiver::computeIsReceptionPossible(listening, reception, part);
    ReceptionContext context;
    return resolveReceptionContext(transmission->getPacket(), modeSet,
            transmission->getDataDuration(), context,
            static_cast<int>(signalModel->getMappedData().blocks.size())) &&
           NarrowbandReceiverBase::computeIsReceptionPossible(listening, reception, part);
}

const IReceptionDecision *Ieee80211LdpcSoftReceiver::computeReceptionDecision(
        const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part,
        const IInterference *interference, const ISnir *snir) const
{
    auto transmission = reception->getTransmission();
    auto signalModel = dynamic_cast<const Ieee80211LdpcSoftTransmissionModel *>(transmission->getBitModel());
    if (signalModel == nullptr)
        return Ieee80211Receiver::computeReceptionDecision(listening, reception, part, interference, snir);
    bool possible = computeIsReceptionPossible(listening, reception, part);
    bool attempted = possible && computeIsReceptionAttempted(listening, reception, part, interference);
    bool successful;
    ReceptionContext context;
    if (attempted && !resolveReceptionContext(transmission->getPacket(), modeSet,
            transmission->getDataDuration(), context,
            static_cast<int>(signalModel->getMappedData().blocks.size())))
        throw cRuntimeError("IEEE 802.11 LDPC soft receiver lost its receiver-derived PHY context");
    if (!attempted)
        successful = false;
    else if (part == IRadioSignal::SIGNAL_PART_DATA)
        successful = computeExactDataSuccess(reception, snir, signalModel);
    else if (part == IRadioSignal::SIGNAL_PART_WHOLE) {
        // Keep legacy/BCC reception whole-part based, and compose the exact
        // result here from independently gated preamble/header decisions plus
        // decoder-owned DATA.  This avoids globally splitting BCC control
        // responses while still preventing DATA decoding after a PHY-header
        // failure.
        successful = computeExactPreambleOrHeaderSuccess(listening, reception,
                IRadioSignal::SIGNAL_PART_PREAMBLE, interference, snir, context.mode) &&
                computeExactPreambleOrHeaderSuccess(listening, reception,
                        IRadioSignal::SIGNAL_PART_HEADER, interference, snir, context.mode) &&
                computeExactDataSuccess(reception, snir, signalModel);
    }
    else
        successful = computeExactPreambleOrHeaderSuccess(listening, reception, part,
                interference, snir, context.mode);
    return new ReceptionDecision(reception, part, possible, attempted, successful);
}

const IReceptionResult *Ieee80211LdpcSoftReceiver::computeReceptionResult(
        const IListening *listening, const IReception *reception, const IInterference *interference,
        const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto signalModel = dynamic_cast<const Ieee80211LdpcSoftTransmissionModel *>(transmission->getBitModel());
    if (signalModel == nullptr)
        return Ieee80211Receiver::computeReceptionResult(listening, reception, interference, snir, decisions);
    auto receptionResult = ReceiverBase::computeReceptionResult(listening, reception, interference, snir, decisions);
    auto packet = const_cast<Packet *>(receptionResult->getPacket());
    ReceptionContext context;
    if (resolveReceptionContext(transmission->getPacket(), modeSet,
            transmission->getDataDuration(), context,
            static_cast<int>(signalModel->getMappedData().blocks.size()))) {
        // RXVECTOR APEP_LENGTH is the four-octet-rounded VHT-SIG-B
        // indication (Table 21-14 and Eq. 21-46), never the exact TX APEP.
        // Refresh only the non-wire base field with that received indication.
        auto receivedHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet,
                b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
        if (auto receivedVhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(receivedHeader);
            receivedVhtHeader != nullptr && receivedVhtHeader->getLengthField() !=
                    decodeVhtSuSigBLength(receivedVhtHeader->getVhtSigBLength())) {
            auto roundedApep = decodeVhtSuSigBLength(receivedVhtHeader->getVhtSigBLength());
            auto correctedVhtHeader = makeShared<Ieee80211VhtPhyHeader>(*receivedVhtHeader);
            correctedVhtHeader->setLengthField(roundedApep);
            packet->replaceAt(correctedVhtHeader, b(0), correctedVhtHeader->getChunkLength(),
                    Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                    Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
        }
        if (auto receivedVhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(receivedHeader))
            packet->addTagIfAbsent<Ieee80211VhtApepInd>()->setApepLength(
                    decodeVhtSuSigBLength(receivedVhtHeader->getVhtSigBLength()).get<B>());
        packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(context.mode);
        packet->addTagIfAbsent<Ieee80211DataEncodingPlanTag>()->setPlan(*context.plan);
    }
    if (channel != nullptr)
        packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(channel);

    auto outcome = exactReceptionOutcomes.find(transmission->getId());
    // The exact DATA decoder remains authoritative for the delivered packet:
    // a failed or missing outcome must reach the MAC as a bit error.
    if (outcome == exactReceptionOutcomes.end() || !outcome->second.success)
        packet->setBitError(true);
    if (outcome != exactReceptionOutcomes.end() && outcome->second.success && context.plan != nullptr) {
        auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet,
                b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
        std::vector<bool> decodedBits;
        decodedBits.reserve(outcome->second.psduBits.getSize());
        for (unsigned int i = 0; i < outcome->second.psduBits.getSize(); i++)
            decodedBits.push_back(outcome->second.psduBits.getBit(i));
        packet->replaceAt(makeShared<BitsChunk>(decodedBits), phyHeader->getChunkLength(),
                B(context.psduOctets), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
    }
    exactReceptionOutcomes.erase(transmission->getId());
    return receptionResult;
}

Packet *Ieee80211LdpcSoftReceiver::computeReceivedPacket(const ISnir *snir,
        bool isReceptionSuccessful) const
{
    auto transmission = snir->getReception()->getTransmission();
    if (dynamic_cast<const Ieee80211LdpcSoftTransmissionModel *>(transmission->getBitModel()) != nullptr)
        return ReceiverBase::computeReceivedPacket(snir, isReceptionSuccessful);
    return FlatReceiverBase::computeReceivedPacket(snir, isReceptionSuccessful);
}

std::ostream& Ieee80211LdpcSoftReceiver::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211LdpcSoftReceiver";
    if (level <= PRINT_LEVEL_INFO)
        stream << EV_FIELD(maxIterations) << EV_FIELD(maximumLlr);
    return Ieee80211Receiver::printToStream(stream, level, evFlags);
}

} // namespace physicallayer
} // namespace inet
