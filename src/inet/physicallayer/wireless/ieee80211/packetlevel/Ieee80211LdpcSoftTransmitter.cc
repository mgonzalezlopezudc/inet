//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211LdpcSoftTransmitter.h"

#include <cmath>

#include "inet/common/packet/chunk/BitsChunk.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/ApskModulationBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcDataPipeline.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211LdpcSoftTransmitter);

simsignal_t Ieee80211LdpcSoftTransmitter::ldpcDataEncodedSignal =
        cComponent::registerSignal("ldpcDataEncoded");

namespace {

int getBandwidthMhz(Hz bandwidth)
{
    int bandwidthMhz = static_cast<int>(std::llround(bandwidth.get() / 1e6));
    if (std::fabs(bandwidth.get() - bandwidthMhz * 1e6) > 0.5)
        throw cRuntimeError("IEEE 802.11 LDPC soft path requires an integral MHz bandwidth");
    return bandwidthMhz;
}

uint8_t getVhtBandwidthCode(Hz bandwidth)
{
    if (bandwidth == MHz(20))
        return 0;
    if (bandwidth == MHz(40))
        return 1;
    if (bandwidth == MHz(80))
        return 2;
    if (bandwidth == MHz(160))
        return 3;
    throw cRuntimeError("Unsupported VHT-SU bandwidth %s", bandwidth.str().c_str());
}

Ieee80211DataEncodingPlan requirePlan(const Packet *packet,
        const IIeee80211Mode *mode, b indicatedLength)
{
    const auto *dataMode = mode->getDataMode();
    auto planTag = packet->findTag<Ieee80211DataEncodingPlanTag>();
    if (dataMode->getPhyFormat() == Ieee80211PhyFormat::VHT_SU) {
        // Exact APEP_LENGTH is intentionally absent from the VHT PHY chunk.
        // The locally computed plan is consumed here before the immutable
        // transmission is created; it contains Npld/Navbits, not an exact
        // MPDU or APEP boundary.
        if (planTag == nullptr || !planTag->hasPlan())
            throw cRuntimeError("IEEE 802.11 VHT-SU LDPC soft transmission requires a local encoding plan");
        const auto& plan = planTag->getPlan();
        int numberOfDataBitsPerSymbol = dataMode->getNumberOfDataBitsPerSymbol();
        int numberOfCodedBitsPerSymbol = dataMode->getNumberOfCodedBitsPerSymbol();
        if (plan.getFecType() != Ieee80211FecType::LDPC ||
            plan.getPhyFormat() != Ieee80211PhyFormat::VHT_SU ||
            plan.getNumberOfCodedBitsPerSymbol() != numberOfCodedBitsPerSymbol ||
            plan.getUncodedDataBits() != plan.getInitialNumberOfSymbols() * numberOfDataBitsPerSymbol ||
            plan.getAvailableEncodedBits() != plan.getNumberOfSymbols() * numberOfCodedBitsPerSymbol)
            throw cRuntimeError("IEEE 802.11 VHT-SU LDPC local encoding plan disagrees with the selected mode");
        return plan;
    }

    auto computed = dataMode->computeEncodingPlan(indicatedLength);
    if (planTag != nullptr && planTag->hasPlan() && !(planTag->getPlan() == computed))
        throw cRuntimeError("IEEE 802.11 LDPC soft transmission plan disagrees with the selected mode and PSDU length");
    return computed;
}

void validateSoftMode(const IIeee80211Mode *mode, const Ptr<const Ieee80211PhyHeader>& phyHeader,
        b psduLength)
{
    const auto *dataMode = mode->getDataMode();
    if (dataMode->getFecType() != Ieee80211FecType::LDPC)
        throw cRuntimeError("IEEE 802.11 LDPC soft transmitter received a non-LDPC mode");
    if (auto htDataMode = dynamic_cast<const Ieee80211HtDataMode *>(dataMode)) {
        auto preambleMode = dynamic_cast<const Ieee80211HtPreambleMode *>(mode->getPreambleMode());
        int bandwidthMhz = getBandwidthMhz(htDataMode->getBandwidth());
        // HT-SIG fields and the MCS/NSS mapping follow IEEE Std 802.11-2024,
        // 19.3.5 and 19.3.11.7.6.
        if ((bandwidthMhz != 20 && bandwidthMhz != 40) || htDataMode->getMcsIndex() > 31 ||
            htDataMode->getNumberOfSpatialStreams() < 1 || htDataMode->getNumberOfSpatialStreams() > 4 ||
            preambleMode == nullptr || preambleMode->getPreambleFormat() != Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED)
            throw cRuntimeError("IEEE 802.11 HT soft LDPC supports only legal mixed HT-SU widths, MCS0-31, and NSS1-4");
        auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader);
        if (htHeader == nullptr || htHeader->getMcs() != htDataMode->getMcsIndex() ||
            htHeader->getChannelWidth40() != (bandwidthMhz == 40) ||
            htHeader->getShortGi() != (htDataMode->getGuardIntervalType() == Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) ||
            htHeader->getStbc() != 0 ||
            !htHeader->getFecCoding() || htHeader->getLengthField() != psduLength)
            throw cRuntimeError("IEEE 802.11 HT PHY header is incompatible with the exact LDPC soft path");
    }
    else if (auto vhtDataMode = dynamic_cast<const Ieee80211VhtDataMode *>(dataMode)) {
        auto preambleMode = dynamic_cast<const Ieee80211VhtPreambleMode *>(mode->getPreambleMode());
        int bandwidthMhz = getBandwidthMhz(vhtDataMode->getBandwidth());
        // VHT-SIG-A/SIG-B validation follows 21.3.4.9.2, 21.3.10.6-.9,
        // Table 21-14, and Eq. 21-46 for the rounded Length field.
        if ((bandwidthMhz != 20 && bandwidthMhz != 40 && bandwidthMhz != 80 && bandwidthMhz != 160) ||
            vhtDataMode->getMcsIndex() > 9 || vhtDataMode->getNumberOfSpatialStreams() < 1 ||
            vhtDataMode->getNumberOfSpatialStreams() > 8 || preambleMode == nullptr ||
            preambleMode->getPreambleFormat() != Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED)
            throw cRuntimeError("IEEE 802.11 VHT soft LDPC supports only legal mixed VHT-SU widths, MCS0-9, and NSS1-8");
        auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader);
        if (vhtHeader == nullptr)
            throw cRuntimeError("IEEE 802.11 VHT soft LDPC requires a VHT PHY header");
        auto sigBLayout = getVhtSuSigBLayout(vhtHeader->getBandwidth());
        if (vhtHeader->getBandwidth() != getVhtBandwidthCode(vhtDataMode->getBandwidth()) ||
            (vhtHeader->getGroupId() != 0 && vhtHeader->getGroupId() != 63) ||
            vhtHeader->getPartialAid() > 511 ||
            vhtHeader->getNumberOfSpaceTimeStreams() != vhtDataMode->getNumberOfSpatialStreams() - 1 ||
            vhtHeader->getShortGi() != (vhtDataMode->getGuardIntervalType() == Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) ||
            (!vhtHeader->getShortGi() && vhtHeader->getShortGiNsymDisambiguation()) ||
            !vhtHeader->getCoding() || vhtHeader->getStbc() || vhtHeader->getBeamformed() ||
            vhtHeader->getMcs() != vhtDataMode->getMcsIndex() ||
            !vhtHeader->getReserved1() || !vhtHeader->getReserved2() || !vhtHeader->getReserved3() ||
            vhtHeader->getVhtSigBReserved() != sigBLayout.getReservedValue() || vhtHeader->getVhtSigBTail() != 0 ||
            vhtHeader->getLengthField() != decodeVhtSuSigBLength(vhtHeader->getVhtSigBLength()) ||
            vhtHeader->getVhtSigBLength() != encodeVhtSuSigBLength(psduLength))
            throw cRuntimeError("IEEE 802.11 VHT PHY header is incompatible with the exact LDPC soft path");
    }
    else
        throw cRuntimeError("IEEE 802.11 LDPC soft transmitter supports only HT and VHT-SU modes");
}

uint8_t getVhtSigBCrc(const Ieee80211VhtPhyHeader *header)
{
    auto layout = getVhtSuSigBLayout(header->getBandwidth());
    std::vector<bool> protectedBits;
    for (int i = 0; i < layout.lengthFieldWidth; i++)
        protectedBits.push_back((header->getVhtSigBLength() >> i) & 1);
    for (int i = 0; i < layout.reservedFieldWidth; i++)
        protectedBits.push_back((header->getVhtSigBReserved() >> i) & 1);
    return Ieee80211LdpcDataPipeline::computeVhtSigBCrc(protectedBits);
}

} // namespace

BitVector Ieee80211LdpcSoftTransmitter::extractPsduBits(const Packet *packet,
        const Ptr<const Ieee80211PhyHeader>& phyHeader,
        const Ieee80211DataEncodingPlan& plan)
{
    // For VHT-SU, the serialized PSDU contains the standards MAC padding
    // selected from N_SYM. Encode all complete PSDU octets implied by Npld;
    // the remaining 0..7 bits are PHY padding inside the LDPC field.
    b length = phyHeader->getLengthField();
    if (plan.getPhyFormat() == Ieee80211PhyFormat::VHT_SU)
        length = B((plan.getUncodedDataBits() - 16) / 8);
    auto bitsChunk = packet->peekAt<BitsChunk>(phyHeader->getChunkLength(), length,
            Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
    if (bitsChunk == nullptr || bitsChunk->getBitArraySize() != static_cast<size_t>(length.get<b>()))
        throw cRuntimeError("IEEE 802.11 LDPC soft transmitter cannot obtain the serialized PSDU bits");
    BitVector bits;
    for (bool bit : bitsChunk->getBits())
        bits.appendBit(bit);
    return bits;
}

Ieee80211LdpcSoftTransmissionModel::SymbolBlocks Ieee80211LdpcSoftTransmitter::makeSymbols(
        const Ieee80211LdpcMappedData& mappedData,
        const std::vector<const ApskModulationBase *>& modulations)
{
    Ieee80211LdpcSoftTransmissionModel::SymbolBlocks symbols;
    symbols.resize(mappedData.blocks.size());
    for (size_t symbol = 0; symbol < mappedData.blocks.size(); symbol++) {
        if (mappedData.blocks[symbol].size() != modulations.size())
            throw cRuntimeError("IEEE 802.11 LDPC soft transmitter stream count disagrees with selected mode");
        symbols[symbol].resize(mappedData.blocks[symbol].size());
        for (size_t stream = 0; stream < mappedData.blocks[symbol].size(); stream++) {
            const auto *modulation = modulations[stream];
            if (modulation == nullptr)
                throw cRuntimeError("IEEE 802.11 LDPC soft transmitter has no subcarrier modulation for stream %zu", stream);
            symbols[symbol][stream].resize(mappedData.blocks[symbol][stream].size());
            for (size_t block = 0; block < mappedData.blocks[symbol][stream].size(); block++) {
                const auto& bits = mappedData.blocks[symbol][stream][block];
                const int bitsPerSymbol = modulation->getCodeWordSize();
                if (bits.getSize() % bitsPerSymbol != 0)
                    throw cRuntimeError("IEEE 802.11 LDPC soft transmitter frequency block has a partial constellation point");
                auto& symbolBlock = symbols[symbol][stream][block];
                symbolBlock.reserve(bits.getSize() / bitsPerSymbol);
                for (unsigned int offset = 0; offset < bits.getSize(); offset += bitsPerSymbol) {
                    unsigned int labelValue = 0;
                    for (int bit = 0; bit < bitsPerSymbol; bit++)
                        if (bits.getBit(offset + bit))
                            labelValue |= 1U << bit;
                    ShortBitVector label(labelValue, bitsPerSymbol);
                    symbolBlock.push_back(*modulation->mapToConstellationDiagram(label));
                }
            }
        }
    }
    return symbols;
}

std::vector<const ApskModulationBase *> Ieee80211LdpcSoftTransmitter::getStreamSubcarrierModulations(
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
        throw cRuntimeError("IEEE 802.11 LDPC soft transmitter requires an HT or VHT data mode");

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

const ITransmission *Ieee80211LdpcSoftTransmitter::createTransmission(const IRadio *transmitter,
        const Packet *packet, simtime_t startTime) const
{
    auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet);
    auto transmissionMode = computeTransmissionMode(packet);
    auto *dataMode = transmissionMode->getDataMode();
    if (dataMode->getFecType() != Ieee80211FecType::LDPC)
        return Ieee80211Transmitter::createTransmission(transmitter, packet, startTime);
    if (dynamic_cast<const ScalarTransmitterAnalogModel *>(getAnalogModel()) == nullptr)
        throw cRuntimeError("IEEE 802.11 LDPC soft transmission requires the scalar analog representation");

    auto psduLength = phyHeader->getLengthField();
    validateSoftMode(transmissionMode, phyHeader, psduLength);
    if (dataMode->getNumberOfSpatialStreams() > transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Number of IEEE 802.11 LDPC spatial streams is higher than the number of transmit antennas");
    auto plan = requirePlan(packet, transmissionMode, psduLength);
    // This is a local MAC/PHY planning request. The exact receiver rebuilds
    // its plan from received timing and PHY fields, so do not leave the
    // sender plan on the immutable transmission packet.
    const_cast<Packet *>(packet)->removeTagIfPresent<Ieee80211DataEncodingPlanTag>();
    if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader)) {
        if (vhtHeader->getShortGiNsymDisambiguation() !=
                    (vhtHeader->getShortGi() && plan.getNumberOfSymbols() % 10 == 9) ||
            vhtHeader->getLdpcExtraOfdmSymbol() != plan.getAdditionalCapacityApplied())
            throw cRuntimeError("IEEE 802.11 VHT-SIG-A GI disambiguation or LDPC Extra OFDM Symbol disagrees with the exact transmit plan");
    }
    auto psduBits = extractPsduBits(packet, phyHeader, plan);
    uint8_t vhtSigBCrc = 0;
    if (plan.getPhyFormat() == Ieee80211PhyFormat::VHT_SU)
        vhtSigBCrc = getVhtSigBCrc(check_and_cast<const Ieee80211VhtPhyHeader *>(phyHeader.get()));
    auto streamModulations = getStreamSubcarrierModulations(dataMode);
    std::vector<int> bitsPerSubcarrier;
    bitsPerSubcarrier.reserve(streamModulations.size());
    for (const auto *modulation : streamModulations)
        bitsPerSubcarrier.push_back(static_cast<int>(modulation->getCodeWordSize()));
    int bandwidthMhz = getBandwidthMhz(dataMode->getBandwidth());
    auto scramblerRegisterState = static_cast<uint8_t>(intuniform(1, 0x7f));
    auto encoded = Ieee80211LdpcDataPipeline::encodeAndMap(psduBits, plan, bitsPerSubcarrier, bandwidthMhz,
            vhtSigBCrc, 1, Ieee80211LdpcDataCoder(), scramblerRegisterState);
    auto symbols = makeSymbols(encoded.mapped, streamModulations);
    auto bitModel = new Ieee80211LdpcSoftTransmissionModel(encoded.mapped, symbols);
    const_cast<Ieee80211LdpcSoftTransmitter *>(this)->emit(ldpcDataEncodedSignal, 1L);

    auto transmissionChannel = computeTransmissionChannel(packet);
    W transmissionPower = computeTransmissionPower(packet);
    Hz transmissionBandwidth = dataMode->getBandwidth();
    const simtime_t preambleDuration = transmissionMode->getPreambleMode()->getDuration();
    const simtime_t headerDuration = transmissionMode->getHeaderMode()->getDuration();
    const simtime_t dataDuration = plan.getNumberOfSymbols() * dataMode->getSymbolInterval();
    const simtime_t endTime = startTime + preambleDuration + headerDuration + dataDuration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord& startPosition = mobility->getCurrentPosition();
    const Coord& endPosition = mobility->getCurrentPosition();
    const Quaternion& startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion& endOrientation = mobility->getCurrentAngularPosition();
    auto analogModel = getAnalogModel()->createAnalogModel(preambleDuration, headerDuration, dataDuration,
            centerFrequency, transmissionBandwidth, transmissionPower);
    return new Ieee80211Transmission(transmitter, packet, startTime, endTime, preambleDuration,
            headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation,
            nullptr, bitModel, nullptr, nullptr, analogModel, transmissionMode, transmissionChannel);
}

} // namespace physicallayer
} // namespace inet
