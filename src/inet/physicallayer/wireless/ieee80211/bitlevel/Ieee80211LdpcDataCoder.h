//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCDATACODER_H
#define __INET_IEEE80211LDPCDATACODER_H

#include "inet/physicallayer/wireless/common/radio/bitlevel/LdpcCoder.h"
#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcCode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DataEncodingPlan.h"

namespace inet {
namespace physicallayer {

/**
 * Maps a planned IEEE 802.11 LDPC Data field to and from full Annex F
 * codewords. Shortened bits are known-zero LLRs, punctures are erasures, and
 * repeated observations are combined by LLR addition.
 */
class INET_API Ieee80211LdpcDataCoder
{
  protected:
    LdpcDecodingAlgorithm decodingAlgorithm;
    int maxIterations;
    double normalizedMinSumFactor;
    double maximumLlr;

  protected:
    static Ieee80211LdpcRate getLdpcRate(const Ieee80211LdpcCodewordPlan& codewordPlan);
    static std::vector<int> getShortenedCodewordPositions(const Ieee80211LdpcCodewordPlan& codewordPlan);

  public:
    Ieee80211LdpcDataCoder(LdpcDecodingAlgorithm decodingAlgorithm = LdpcDecodingAlgorithm::SUM_PRODUCT,
            int maxIterations = 20, double normalizedMinSumFactor = 0.75, double maximumLlr = 20.0);

    BitVector encode(const BitVector& dataBits, const Ieee80211DataEncodingPlan& plan) const;
    std::vector<BitReliabilityVector> reconstructCodewordReliabilities(
            const BitReliabilityVector& transmittedReliabilities,
            const Ieee80211DataEncodingPlan& plan) const;
    FecDecodingResult decodeReliabilities(const BitReliabilityVector& transmittedReliabilities,
            const Ieee80211DataEncodingPlan& plan) const;
};

} // namespace physicallayer
} // namespace inet

#endif
