//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcDataCoder.h"

#include <algorithm>
#include <cmath>

#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcCode.h"

namespace inet {
namespace physicallayer {

Ieee80211LdpcDataCoder::Ieee80211LdpcDataCoder(LdpcDecodingAlgorithm decodingAlgorithm,
        int maxIterations, double normalizedMinSumFactor, double maximumLlr) :
    decodingAlgorithm(decodingAlgorithm),
    maxIterations(maxIterations),
    normalizedMinSumFactor(normalizedMinSumFactor),
    maximumLlr(maximumLlr)
{
    // Reuse the generic coder's complete configuration validation.
    const auto& validationCode = Ieee80211LdpcCode::getCode(648, Ieee80211LdpcRate::RATE_1_2);
    LdpcCoder validationCoder(validationCode, decodingAlgorithm, maxIterations, normalizedMinSumFactor, maximumLlr);
}

Ieee80211LdpcRate Ieee80211LdpcDataCoder::getLdpcRate(const Ieee80211LdpcCodewordPlan& codewordPlan)
{
    int n = codewordPlan.getCodewordLength();
    int k = codewordPlan.getInformationLength();
    if (2 * k == n)
        return Ieee80211LdpcRate::RATE_1_2;
    if (3 * k == 2 * n)
        return Ieee80211LdpcRate::RATE_2_3;
    if (4 * k == 3 * n)
        return Ieee80211LdpcRate::RATE_3_4;
    if (6 * k == 5 * n)
        return Ieee80211LdpcRate::RATE_5_6;
    throw cRuntimeError("Unsupported IEEE 802.11 LDPC code dimensions N=%d K=%d", n, k);
}

std::vector<int> Ieee80211LdpcDataCoder::getShortenedCodewordPositions(
        const Ieee80211LdpcCodewordPlan& codewordPlan)
{
    std::vector<int> positions;
    positions.reserve(codewordPlan.getCodewordLength() - codewordPlan.getShortenedBits());
    for (int i = 0; i < codewordPlan.getDataBits(); i++)
        positions.push_back(i);
    for (int i = codewordPlan.getInformationLength(); i < codewordPlan.getCodewordLength(); i++)
        positions.push_back(i);
    return positions;
}

BitVector Ieee80211LdpcDataCoder::encode(const BitVector& dataBits,
        const Ieee80211DataEncodingPlan& plan) const
{
    if (plan.getFecType() != Ieee80211FecType::LDPC)
        throw cRuntimeError("IEEE 802.11 LDPC data coder requires an LDPC plan");
    if (dataBits.getSize() != static_cast<unsigned int>(plan.getUncodedDataBits()))
        throw cRuntimeError("IEEE 802.11 LDPC Data field has %u bits, expected %d",
                            dataBits.getSize(), plan.getUncodedDataBits());

    BitVector transmitted;
    unsigned int dataOffset = 0;
    for (const auto& codewordPlan : plan.getCodewords()) {
        const auto& code = Ieee80211LdpcCode::getCode(codewordPlan.getCodewordLength(), getLdpcRate(codewordPlan));
        BitVector information;
        for (int i = 0; i < codewordPlan.getDataBits(); i++)
            information.appendBit(dataBits.getBit(dataOffset++));
        for (int i = 0; i < codewordPlan.getShortenedBits(); i++)
            information.appendBit(false);

        BitVector fullCodeword = code.encode(information);
        for (int i = 0; i < codewordPlan.getDataBits(); i++)
            transmitted.appendBit(fullCodeword.getBit(i));
        int transmittedParityBits = codewordPlan.getCodewordLength() -
                                    codewordPlan.getInformationLength() - codewordPlan.getPuncturedBits();
        for (int i = 0; i < transmittedParityBits; i++)
            transmitted.appendBit(fullCodeword.getBit(codewordPlan.getInformationLength() + i));

        auto repeatPositions = getShortenedCodewordPositions(codewordPlan);
        for (int i = 0; i < codewordPlan.getRepeatedBits(); i++)
            transmitted.appendBit(fullCodeword.getBit(repeatPositions[i % repeatPositions.size()]));
    }
    if (dataOffset != dataBits.getSize() || transmitted.getSize() != static_cast<unsigned int>(plan.getAvailableEncodedBits()))
        throw cRuntimeError("IEEE 802.11 LDPC mapping did not consume or produce the planned bit count");
    return transmitted;
}

std::vector<BitReliabilityVector> Ieee80211LdpcDataCoder::reconstructCodewordReliabilities(
        const BitReliabilityVector& transmittedReliabilities,
        const Ieee80211DataEncodingPlan& plan) const
{
    if (plan.getFecType() != Ieee80211FecType::LDPC)
        throw cRuntimeError("IEEE 802.11 LDPC reconstruction requires an LDPC plan");
    if (transmittedReliabilities.size() != static_cast<size_t>(plan.getAvailableEncodedBits()))
        throw cRuntimeError("IEEE 802.11 LDPC received %zu reliabilities, expected %d",
                            transmittedReliabilities.size(), plan.getAvailableEncodedBits());
    for (double reliability : transmittedReliabilities) {
        if (!std::isfinite(reliability))
            throw cRuntimeError("IEEE 802.11 LDPC received reliability must be finite");
    }

    std::vector<BitReliabilityVector> result;
    result.reserve(plan.getCodewords().size());
    size_t inputOffset = 0;
    for (const auto& codewordPlan : plan.getCodewords()) {
        BitReliabilityVector codeword(codewordPlan.getCodewordLength(), 0.0);
        for (int i = 0; i < codewordPlan.getDataBits(); i++)
            codeword[i] = transmittedReliabilities[inputOffset++];
        std::fill(codeword.begin() + codewordPlan.getDataBits(),
                  codeword.begin() + codewordPlan.getInformationLength(), maximumLlr);

        int transmittedParityBits = codewordPlan.getCodewordLength() -
                                    codewordPlan.getInformationLength() - codewordPlan.getPuncturedBits();
        for (int i = 0; i < transmittedParityBits; i++)
            codeword[codewordPlan.getInformationLength() + i] = transmittedReliabilities[inputOffset++];

        auto repeatPositions = getShortenedCodewordPositions(codewordPlan);
        for (int i = 0; i < codewordPlan.getRepeatedBits(); i++)
            codeword[repeatPositions[i % repeatPositions.size()]] += transmittedReliabilities[inputOffset++];
        result.push_back(std::move(codeword));
    }
    if (inputOffset != transmittedReliabilities.size())
        throw cRuntimeError("IEEE 802.11 LDPC reconstruction did not consume the planned observations");
    return result;
}

FecDecodingResult Ieee80211LdpcDataCoder::decodeReliabilities(
        const BitReliabilityVector& transmittedReliabilities,
        const Ieee80211DataEncodingPlan& plan) const
{
    auto codewordReliabilities = reconstructCodewordReliabilities(transmittedReliabilities, plan);
    BitVector dataBits;
    int totalIterations = 0;
    for (size_t i = 0; i < plan.getCodewords().size(); i++) {
        const auto& codewordPlan = plan.getCodewords()[i];
        const auto& code = Ieee80211LdpcCode::getCode(codewordPlan.getCodewordLength(), getLdpcRate(codewordPlan));
        LdpcCoder coder(code, decodingAlgorithm, maxIterations, normalizedMinSumFactor, maximumLlr);
        FecDecodingResult decoded = coder.decodeReliabilities(codewordReliabilities[i]);
        totalIterations += decoded.iterations;
        if (!decoded.converged)
            return {BitVector(), false, totalIterations};
        for (int j = 0; j < codewordPlan.getDataBits(); j++)
            dataBits.appendBit(decoded.informationBits.getBit(j));
    }
    if (dataBits.getSize() != static_cast<unsigned int>(plan.getUncodedDataBits()))
        throw cRuntimeError("IEEE 802.11 LDPC decoder produced an unexpected Data-field length");
    return {dataBits, true, totalIterations};
}

} // namespace physicallayer
} // namespace inet
