//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_LDPCCODER_H
#define __INET_LDPCCODER_H

#include <vector>

#include "inet/physicallayer/wireless/common/radio/bitlevel/LdpcCode.h"

namespace inet {
namespace physicallayer {

enum class LdpcDecodingAlgorithm {
    SUM_PRODUCT,
    NORMALIZED_MIN_SUM
};

/**
 * Systematic encoder and layered soft-decision decoder for an LdpcCode.
 * Positive LLRs denote bit 0 and negative LLRs denote bit 1.  The decoder
 * owns no per-call mutable state, making repeated and interleaved calls
 * deterministic and reentrant.
 */
class INET_API LdpcCoder : public IFecCoder
{
  protected:
    const LdpcCode *code;
    LdpcDecodingAlgorithm decodingAlgorithm;
    int maxIterations;
    double normalizedMinSumFactor;
    double maximumLlr;

  protected:
    double clampLlr(double value) const;
    BitVector hardDecision(const std::vector<double>& posterior) const;
    std::vector<double> updateSumProductCheck(const std::vector<double>& extrinsic) const;
    std::vector<double> updateNormalizedMinSumCheck(const std::vector<double>& extrinsic) const;

  public:
    explicit LdpcCoder(const LdpcCode *code,
                       LdpcDecodingAlgorithm decodingAlgorithm = LdpcDecodingAlgorithm::SUM_PRODUCT,
                       int maxIterations = 20,
                       double normalizedMinSumFactor = 0.75,
                       double maximumLlr = 20.0);
    explicit LdpcCoder(const LdpcCode& code,
                       LdpcDecodingAlgorithm decodingAlgorithm = LdpcDecodingAlgorithm::SUM_PRODUCT,
                       int maxIterations = 20,
                       double normalizedMinSumFactor = 0.75,
                       double maximumLlr = 20.0) :
        LdpcCoder(&code, decodingAlgorithm, maxIterations, normalizedMinSumFactor, maximumLlr) {}

    virtual BitVector encode(const BitVector& informationBits) const override;
    virtual std::pair<BitVector, bool> decode(const BitVector& encodedBits) const override;
    virtual FecDecodingResult decodeReliabilities(const BitReliabilityVector& reliabilities) const override;
    virtual const LdpcCode *getForwardErrorCorrection() const override { return code; }

    LdpcDecodingAlgorithm getDecodingAlgorithm() const { return decodingAlgorithm; }
    int getMaxIterations() const { return maxIterations; }
    double getNormalizedMinSumFactor() const { return normalizedMinSumFactor; }
    double getMaximumLlr() const { return maximumLlr; }

    // Test-facing pure check-node operations.  They use the same numerical
    // contract as the production update but have no decoder state.
    std::vector<double> computeSumProductCheckMessages(const std::vector<double>& extrinsic) const;
    std::vector<double> computeNormalizedMinSumCheckMessages(const std::vector<double>& extrinsic) const;

    std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
