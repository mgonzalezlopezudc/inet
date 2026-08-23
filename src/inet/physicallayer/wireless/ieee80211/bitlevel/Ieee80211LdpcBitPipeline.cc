//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcBitPipeline.h"

#include <algorithm>
#include <numeric>

namespace inet {
namespace physicallayer {

namespace {

int getVhtToneMappingDistance(int bandwidthMhz)
{
    switch (bandwidthMhz) {
        case 20: return 4;
        case 40: return 6;
        case 80:
        case 160: return 9;
        default: throw cRuntimeError("Unsupported VHT LDPC bandwidth %d MHz", bandwidthMhz);
    }
}

void validatePermutation(const std::vector<int>& indices, int size, const char *stage)
{
    if (static_cast<int>(indices.size()) != size)
        throw cRuntimeError("IEEE 802.11 LDPC %s permutation has %zu entries, expected %d", stage, indices.size(), size);
    std::vector<bool> visited(size, false);
    for (int index : indices) {
        if (index < 0 || index >= size || visited[index])
            throw cRuntimeError("IEEE 802.11 LDPC %s is not a bijection", stage);
        visited[index] = true;
    }
}

} // namespace

std::vector<std::vector<int>> Ieee80211LdpcBitPipeline::getSpatialStreamSourceIndices(
        int numberOfCodedBitsPerSymbol, const std::vector<int>& bitsPerSubcarrier)
{
    if (numberOfCodedBitsPerSymbol <= 0 || bitsPerSubcarrier.empty())
        throw cRuntimeError("IEEE 802.11 LDPC stream parser requires positive dimensions");
    int bitsPerToneSum = 0;
    int parserBlock = 0;
    for (int bits : bitsPerSubcarrier) {
        if (bits != 1 && bits != 2 && bits != 4 && bits != 6 && bits != 8 && bits != 10)
            throw cRuntimeError("Unsupported IEEE 802.11 coded bits per subcarrier %d", bits);
        bitsPerToneSum += bits;
        parserBlock += std::max(1, bits / 2);
    }
    if (numberOfCodedBitsPerSymbol % bitsPerToneSum != 0)
        throw cRuntimeError("IEEE 802.11 LDPC NCBPS is inconsistent with the spatial streams");
    int dataTones = numberOfCodedBitsPerSymbol / bitsPerToneSum;

    std::vector<std::vector<int>> result(bitsPerSubcarrier.size());
    int prefix = 0;
    std::vector<int> flattened;
    for (size_t spatialStream = 0; spatialStream < bitsPerSubcarrier.size(); spatialStream++) {
        int s = std::max(1, bitsPerSubcarrier[spatialStream] / 2);
        int outputBits = dataTones * bitsPerSubcarrier[spatialStream];
        auto& indices = result[spatialStream];
        indices.reserve(outputBits);
        for (int k = 0; k < outputBits; k++)
            indices.push_back(prefix + parserBlock * (k / s) + k % s);
        flattened.insert(flattened.end(), indices.begin(), indices.end());
        prefix += s;
    }
    validatePermutation(flattened, numberOfCodedBitsPerSymbol, "stream parser");
    return result;
}

std::vector<std::vector<int>> Ieee80211LdpcBitPipeline::getVhtSegmentSourceIndices(
        int numberOfCodedBitsPerSpatialStream, int bitsPerSubcarrier)
{
    if (numberOfCodedBitsPerSpatialStream <= 0 || numberOfCodedBitsPerSpatialStream % 2 != 0)
        throw cRuntimeError("VHT LDPC segment parser requires an even positive NCBPSS");
    int s = std::max(1, bitsPerSubcarrier / 2);
    if (numberOfCodedBitsPerSpatialStream % (2 * s) != 0)
        throw cRuntimeError("VHT LDPC NCBPSS is inconsistent with segment parser block size");
    std::vector<std::vector<int>> result(2);
    std::vector<int> flattened;
    int half = numberOfCodedBitsPerSpatialStream / 2;
    for (int subblock = 0; subblock < 2; subblock++) {
        result[subblock].reserve(half);
        for (int k = 0; k < half; k++)
            result[subblock].push_back(2 * s * (k / s) + subblock * s + k % s);
        flattened.insert(flattened.end(), result[subblock].begin(), result[subblock].end());
    }
    validatePermutation(flattened, numberOfCodedBitsPerSpatialStream, "segment parser");
    return result;
}

std::vector<int> Ieee80211LdpcBitPipeline::getVhtToneMapperOutputIndices(int toneCount, int toneMappingDistance)
{
    if (toneCount <= 0 || toneMappingDistance <= 0 || toneCount % toneMappingDistance != 0)
        throw cRuntimeError("Invalid VHT LDPC tone-mapper dimensions");
    int columns = toneCount / toneMappingDistance;
    std::vector<int> result;
    result.reserve(toneCount);
    for (int input = 0; input < toneCount; input++)
        result.push_back(toneMappingDistance * (input % columns) + input / columns);
    validatePermutation(result, toneCount, "tone mapper");
    return result;
}

BitVector Ieee80211LdpcBitPipeline::selectBits(const BitVector& input, const std::vector<int>& indices)
{
    BitVector result;
    for (int index : indices)
        result.appendBit(input.getBit(index));
    return result;
}

BitReliabilityVector Ieee80211LdpcBitPipeline::selectReliabilities(const BitReliabilityVector& input,
        const std::vector<int>& indices)
{
    BitReliabilityVector result;
    result.reserve(indices.size());
    for (int index : indices)
        result.push_back(input.at(index));
    return result;
}

BitVector Ieee80211LdpcBitPipeline::toneMapBits(const BitVector& input, int bitsPerSubcarrier, int distance)
{
    if (input.getSize() % bitsPerSubcarrier != 0)
        throw cRuntimeError("VHT LDPC tone mapper received a partial constellation point");
    int tones = input.getSize() / bitsPerSubcarrier;
    auto outputIndices = getVhtToneMapperOutputIndices(tones, distance);
    BitVector result;
    for (unsigned int i = 0; i < input.getSize(); i++)
        result.appendBit(false);
    for (int inputTone = 0; inputTone < tones; inputTone++)
        for (int bit = 0; bit < bitsPerSubcarrier; bit++)
            result.setBit(outputIndices[inputTone] * bitsPerSubcarrier + bit,
                          input.getBit(inputTone * bitsPerSubcarrier + bit));
    return result;
}

BitReliabilityVector Ieee80211LdpcBitPipeline::inverseToneMapReliabilities(
        const BitReliabilityVector& input, int bitsPerSubcarrier, int distance)
{
    if (input.size() % bitsPerSubcarrier != 0)
        throw cRuntimeError("VHT LDPC inverse tone mapper received a partial constellation point");
    int tones = input.size() / bitsPerSubcarrier;
    auto outputIndices = getVhtToneMapperOutputIndices(tones, distance);
    BitReliabilityVector result(input.size());
    for (int inputTone = 0; inputTone < tones; inputTone++)
        for (int bit = 0; bit < bitsPerSubcarrier; bit++)
            result[inputTone * bitsPerSubcarrier + bit] = input[outputIndices[inputTone] * bitsPerSubcarrier + bit];
    return result;
}

Ieee80211LdpcMappedData Ieee80211LdpcBitPipeline::encodeAndMap(const BitVector& dataBits,
        const Ieee80211DataEncodingPlan& plan, const std::vector<int>& bitsPerSubcarrier,
        int bandwidthMhz, const Ieee80211LdpcDataCoder& coder)
{
    BitVector encoded = coder.encode(dataBits, plan);
    auto streamIndices = getSpatialStreamSourceIndices(plan.getNumberOfCodedBitsPerSymbol(), bitsPerSubcarrier);
    if (encoded.getSize() != static_cast<unsigned int>(plan.getAvailableEncodedBits()))
        throw cRuntimeError("IEEE 802.11 LDPC encoded length disagrees with the plan");
    if (plan.getPhyFormat() == Ieee80211PhyFormat::HT && bandwidthMhz != 20 && bandwidthMhz != 40)
        throw cRuntimeError("Unsupported HT LDPC bandwidth %d MHz", bandwidthMhz);

    Ieee80211LdpcMappedData mapped;
    mapped.blocks.resize(plan.getNumberOfSymbols());
    for (int symbol = 0; symbol < plan.getNumberOfSymbols(); symbol++) {
        BitVector symbolBits;
        int offset = symbol * plan.getNumberOfCodedBitsPerSymbol();
        for (int i = 0; i < plan.getNumberOfCodedBitsPerSymbol(); i++)
            symbolBits.appendBit(encoded.getBit(offset + i));
        mapped.blocks[symbol].resize(bitsPerSubcarrier.size());
        for (size_t spatialStream = 0; spatialStream < bitsPerSubcarrier.size(); spatialStream++) {
            BitVector spatialBits = selectBits(symbolBits, streamIndices[spatialStream]);
            if (plan.getPhyFormat() == Ieee80211PhyFormat::HT) {
                // Clause 19 explicitly bypasses frequency interleaving for LDPC.
                mapped.blocks[symbol][spatialStream].push_back(spatialBits);
            }
            else {
                std::vector<BitVector> subblocks;
                if (bandwidthMhz == 160) {
                    auto segmentIndices = getVhtSegmentSourceIndices(spatialBits.getSize(), bitsPerSubcarrier[spatialStream]);
                    subblocks.push_back(selectBits(spatialBits, segmentIndices[0]));
                    subblocks.push_back(selectBits(spatialBits, segmentIndices[1]));
                }
                else
                    subblocks.push_back(spatialBits);
                int distance = getVhtToneMappingDistance(bandwidthMhz);
                std::vector<BitVector> toneMappedSubblocks;
                for (auto& subblock : subblocks)
                    toneMappedSubblocks.push_back(toneMapBits(subblock, bitsPerSubcarrier[spatialStream], distance));
                if (bandwidthMhz == 160) {
                    // Equation 21-88 segment deparser: the lower-frequency
                    // subblock precedes the upper-frequency subblock.
                    BitVector frequencySegment = toneMappedSubblocks[0];
                    for (unsigned int i = 0; i < toneMappedSubblocks[1].getSize(); i++)
                        frequencySegment.appendBit(toneMappedSubblocks[1].getBit(i));
                    mapped.blocks[symbol][spatialStream].push_back(frequencySegment);
                }
                else
                    mapped.blocks[symbol][spatialStream].push_back(toneMappedSubblocks[0]);
            }
        }
    }
    return mapped;
}

BitReliabilityVector Ieee80211LdpcBitPipeline::inverseMap(const Ieee80211LdpcMappedReliabilities& mapped,
        const Ieee80211DataEncodingPlan& plan, const std::vector<int>& bitsPerSubcarrier,
        int bandwidthMhz)
{
    if (mapped.size() != static_cast<size_t>(plan.getNumberOfSymbols()))
        throw cRuntimeError("IEEE 802.11 LDPC mapped symbol count disagrees with the plan");
    auto streamIndices = getSpatialStreamSourceIndices(plan.getNumberOfCodedBitsPerSymbol(), bitsPerSubcarrier);
    BitReliabilityVector encoded;
    encoded.reserve(plan.getAvailableEncodedBits());
    for (int symbol = 0; symbol < plan.getNumberOfSymbols(); symbol++) {
        if (mapped[symbol].size() != bitsPerSubcarrier.size())
            throw cRuntimeError("IEEE 802.11 LDPC mapped spatial-stream count disagrees with the mode");
        BitReliabilityVector symbolBits(plan.getNumberOfCodedBitsPerSymbol());
        for (size_t spatialStream = 0; spatialStream < bitsPerSubcarrier.size(); spatialStream++) {
            BitReliabilityVector spatialBits;
            if (plan.getPhyFormat() == Ieee80211PhyFormat::HT) {
                if (mapped[symbol][spatialStream].size() != 1)
                    throw cRuntimeError("HT LDPC must have one frequency block per spatial stream");
                spatialBits = mapped[symbol][spatialStream][0];
            }
            else {
                if (mapped[symbol][spatialStream].size() != 1)
                    throw cRuntimeError("VHT LDPC frequency-segment count disagrees with the bandwidth");
                int distance = getVhtToneMappingDistance(bandwidthMhz);
                std::vector<BitReliabilityVector> subblocks;
                const auto& frequencySegment = mapped[symbol][spatialStream][0];
                if (bandwidthMhz == 160) {
                    if (frequencySegment.size() % 2 != 0)
                        throw cRuntimeError("VHT 160 MHz LDPC frequency segment cannot be split into two subblocks");
                    size_t half = frequencySegment.size() / 2;
                    subblocks.push_back(inverseToneMapReliabilities(
                            BitReliabilityVector(frequencySegment.begin(), frequencySegment.begin() + half),
                            bitsPerSubcarrier[spatialStream], distance));
                    subblocks.push_back(inverseToneMapReliabilities(
                            BitReliabilityVector(frequencySegment.begin() + half, frequencySegment.end()),
                            bitsPerSubcarrier[spatialStream], distance));
                }
                else
                    subblocks.push_back(inverseToneMapReliabilities(frequencySegment, bitsPerSubcarrier[spatialStream], distance));
                if (bandwidthMhz != 160)
                    spatialBits = subblocks[0];
                else {
                    int total = subblocks[0].size() + subblocks[1].size();
                    auto segmentIndices = getVhtSegmentSourceIndices(total, bitsPerSubcarrier[spatialStream]);
                    spatialBits.resize(total);
                    for (int subblock = 0; subblock < 2; subblock++) {
                        if (subblocks[subblock].size() != segmentIndices[subblock].size())
                            throw cRuntimeError("VHT LDPC subblock length disagrees with segment parsing");
                        for (size_t k = 0; k < subblocks[subblock].size(); k++)
                            spatialBits[segmentIndices[subblock][k]] = subblocks[subblock][k];
                    }
                }
            }
            if (spatialBits.size() != streamIndices[spatialStream].size())
                throw cRuntimeError("IEEE 802.11 LDPC spatial-stream length disagrees with stream parsing");
            for (size_t k = 0; k < spatialBits.size(); k++)
                symbolBits[streamIndices[spatialStream][k]] = spatialBits[k];
        }
        encoded.insert(encoded.end(), symbolBits.begin(), symbolBits.end());
    }
    return encoded;
}

FecDecodingResult Ieee80211LdpcBitPipeline::inverseMapAndDecode(
        const Ieee80211LdpcMappedReliabilities& mapped,
        const Ieee80211DataEncodingPlan& plan, const std::vector<int>& bitsPerSubcarrier,
        int bandwidthMhz, const Ieee80211LdpcDataCoder& coder)
{
    return coder.decodeReliabilities(inverseMap(mapped, plan, bitsPerSubcarrier, bandwidthMhz), plan);
}

} // namespace physicallayer
} // namespace inet
