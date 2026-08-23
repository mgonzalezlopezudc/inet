//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/radio/bitlevel/LdpcCoder.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace inet {
namespace physicallayer {

LdpcCoder::LdpcCoder(const LdpcCode *code, LdpcDecodingAlgorithm decodingAlgorithm, int maxIterations,
        double normalizedMinSumFactor, double maximumLlr) :
    code(code),
    decodingAlgorithm(decodingAlgorithm),
    maxIterations(maxIterations),
    normalizedMinSumFactor(normalizedMinSumFactor),
    maximumLlr(maximumLlr)
{
    if (code == nullptr)
        throw cRuntimeError("LDPC coder requires a code");
    if (decodingAlgorithm != LdpcDecodingAlgorithm::SUM_PRODUCT && decodingAlgorithm != LdpcDecodingAlgorithm::NORMALIZED_MIN_SUM)
        throw cRuntimeError("Unsupported LDPC decoding algorithm");
    if (maxIterations <= 0)
        throw cRuntimeError("LDPC maximum iteration count must be positive");
    if (!std::isfinite(normalizedMinSumFactor) || normalizedMinSumFactor <= 0 || normalizedMinSumFactor > 1)
        throw cRuntimeError("LDPC normalized min-sum factor must be finite and in (0,1]");
    if (!std::isfinite(maximumLlr) || maximumLlr <= 0)
        throw cRuntimeError("LDPC maximum LLR must be finite and positive");
}

double LdpcCoder::clampLlr(double value) const
{
    return std::max(-maximumLlr, std::min(maximumLlr, value));
}

BitVector LdpcCoder::hardDecision(const std::vector<double>& posterior) const
{
    if (posterior.size() != static_cast<size_t>(code->getCodewordLength()))
        throw cRuntimeError("LDPC posterior has %zu values, expected %d", posterior.size(), code->getCodewordLength());
    BitVector decision;
    for (double value : posterior) {
        if (!std::isfinite(value))
            throw cRuntimeError("LDPC posterior must be finite");
        decision.appendBit(value < 0);
    }
    return decision;
}

std::vector<double> LdpcCoder::updateSumProductCheck(const std::vector<double>& extrinsic) const
{
    return computeSumProductCheckMessages(extrinsic);
}

std::vector<double> LdpcCoder::updateNormalizedMinSumCheck(const std::vector<double>& extrinsic) const
{
    return computeNormalizedMinSumCheckMessages(extrinsic);
}

std::vector<double> LdpcCoder::computeSumProductCheckMessages(const std::vector<double>& extrinsic) const
{
    std::vector<double> messages(extrinsic.size(), 0.0);
    if (extrinsic.empty())
        return messages;

    std::vector<double> magnitudes(extrinsic.size());
    std::vector<double> prefix(extrinsic.size() + 1, 1.0);
    std::vector<double> suffix(extrinsic.size() + 1, 1.0);
    std::vector<int> signs(extrinsic.size(), 1);
    int totalSign = 1;
    for (size_t i = 0; i < extrinsic.size(); i++) {
        if (!std::isfinite(extrinsic[i]))
            throw cRuntimeError("LDPC check-node input must be finite");
        double value = clampLlr(extrinsic[i]);
        signs[i] = value < 0 ? -1 : 1;
        totalSign *= signs[i];
        magnitudes[i] = std::tanh(0.5 * std::abs(value));
        prefix[i + 1] = prefix[i] * magnitudes[i];
    }
    for (size_t i = extrinsic.size(); i-- > 0;)
        suffix[i] = suffix[i + 1] * magnitudes[i];

    for (size_t i = 0; i < extrinsic.size(); i++) {
        double productExceptI = prefix[i] * suffix[i + 1];
        productExceptI = std::max(0.0, std::min(std::nextafter(1.0, 0.0), productExceptI));
        double magnitude = 2.0 * std::atanh(productExceptI);
        messages[i] = clampLlr(totalSign * signs[i] * magnitude);
    }
    return messages;
}

std::vector<double> LdpcCoder::computeNormalizedMinSumCheckMessages(const std::vector<double>& extrinsic) const
{
    std::vector<double> messages(extrinsic.size(), 0.0);
    if (extrinsic.empty())
        return messages;

    double minimum1 = std::numeric_limits<double>::infinity();
    double minimum2 = std::numeric_limits<double>::infinity();
    size_t minimumEdge = extrinsic.size();
    int totalSign = 1;
    std::vector<double> magnitudes(extrinsic.size());
    std::vector<int> signs(extrinsic.size(), 1);

    for (size_t i = 0; i < extrinsic.size(); i++) {
        if (!std::isfinite(extrinsic[i]))
            throw cRuntimeError("LDPC check-node input must be finite");
        double value = clampLlr(extrinsic[i]);
        magnitudes[i] = std::abs(value);
        signs[i] = value < 0 ? -1 : 1;
        totalSign *= signs[i];

        // Strict comparisons give a stable first-minimum tie break.  An
        // equal second minimum is retained for the selected first edge.
        if (magnitudes[i] < minimum1) {
            minimum2 = minimum1;
            minimum1 = magnitudes[i];
            minimumEdge = i;
        }
        else if (magnitudes[i] < minimum2) {
            minimum2 = magnitudes[i];
        }
    }

    for (size_t i = 0; i < extrinsic.size(); i++) {
        double magnitude = i == minimumEdge ? minimum2 : minimum1;
        if (!std::isfinite(magnitude))
            magnitude = minimum1;
        messages[i] = clampLlr(normalizedMinSumFactor * totalSign * signs[i] * magnitude);
    }
    return messages;
}

BitVector LdpcCoder::encode(const BitVector& informationBits) const
{
    return code->encode(informationBits);
}

std::pair<BitVector, bool> LdpcCoder::decode(const BitVector& encodedBits) const
{
    BitReliabilityVector reliabilities;
    reliabilities.reserve(encodedBits.getSize());
    for (unsigned int i = 0; i < encodedBits.getSize(); i++)
        reliabilities.push_back(encodedBits.getBit(i) ? -maximumLlr : maximumLlr);
    FecDecodingResult result = decodeReliabilities(reliabilities);
    return {result.informationBits, result.converged};
}

FecDecodingResult LdpcCoder::decodeReliabilities(const BitReliabilityVector& reliabilities) const
{
    if (reliabilities.size() != static_cast<size_t>(code->getCodewordLength()))
        throw cRuntimeError("LDPC reliability vector has %zu values, expected %d", reliabilities.size(), code->getCodewordLength());
    for (double reliability : reliabilities) {
        if (!std::isfinite(reliability))
            throw cRuntimeError("LDPC reliability must be finite");
    }

    std::vector<double> posterior(reliabilities.size());
    for (size_t i = 0; i < reliabilities.size(); i++)
        posterior[i] = clampLlr(reliabilities[i]);
    std::vector<double> checkToVariable(code->getEdges().size(), 0.0);
    std::vector<double> extrinsic;

    for (int iteration = 1; iteration <= maxIterations; iteration++) {
        for (int checkNode = 0; checkNode < code->getParityLength(); checkNode++) {
            const std::vector<int>& checkEdges = code->getEdgesOfCheck(checkNode);
            extrinsic.resize(checkEdges.size());
            for (size_t i = 0; i < checkEdges.size(); i++) {
                const LdpcCode::Edge& edge = code->getEdge(checkEdges[i]);
                extrinsic[i] = clampLlr(posterior[edge.variableNode] - checkToVariable[checkEdges[i]]);
            }

            std::vector<double> messages;
            if (decodingAlgorithm == LdpcDecodingAlgorithm::SUM_PRODUCT)
                messages = updateSumProductCheck(extrinsic);
            else
                messages = updateNormalizedMinSumCheck(extrinsic);

            for (size_t i = 0; i < checkEdges.size(); i++) {
                int edgeIndex = checkEdges[i];
                const LdpcCode::Edge& edge = code->getEdge(edgeIndex);
                checkToVariable[edgeIndex] = messages[i];
                posterior[edge.variableNode] = clampLlr(extrinsic[i] + messages[i]);
            }
        }

        BitVector decision = hardDecision(posterior);
        if (code->isCodeword(decision)) {
            BitVector informationBits;
            for (int i = 0; i < code->getInformationLength(); i++)
                informationBits.appendBit(decision.getBit(i));
            return {informationBits, true, iteration};
        }
    }

    BitVector decision = hardDecision(posterior);
    BitVector informationBits;
    for (int i = 0; i < code->getInformationLength(); i++)
        informationBits.appendBit(decision.getBit(i));
    return {informationBits, false, maxIterations};
}

std::ostream& LdpcCoder::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "LdpcCoder" << EV_FIELD(code) << EV_FIELD(maxIterations) << EV_FIELD(maximumLlr);
    return stream;
}

} // namespace physicallayer
} // namespace inet
