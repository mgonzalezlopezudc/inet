//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCBITPIPELINE_H
#define __INET_IEEE80211LDPCBITPIPELINE_H

#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcDataCoder.h"

namespace inet {
namespace physicallayer {

struct Ieee80211LdpcMappedData {
    // [OFDM symbol][spatial stream][frequency segment]. VHT 160 MHz has one
    // segment containing frequency subblock 0 followed by subblock 1.
    std::vector<std::vector<std::vector<BitVector>>> blocks;
};

using Ieee80211LdpcMappedReliabilities =
        std::vector<std::vector<std::vector<BitReliabilityVector>>>;

/**
 * Exact HT/VHT-SU LDPC post-encoder permutations and their receive inverse.
 * HT deliberately has no frequency-interleaver stage. VHT tone mapping is
 * represented as an equivalent permutation of whole NBPSCS-wide bit/LLR
 * groups, so constellation-point membership is preserved.
 */
class INET_API Ieee80211LdpcBitPipeline
{
  protected:
    static BitVector selectBits(const BitVector& input, const std::vector<int>& indices);
    static BitReliabilityVector selectReliabilities(const BitReliabilityVector& input, const std::vector<int>& indices);
    static BitVector toneMapBits(const BitVector& input, int bitsPerSubcarrier, int distance);
    static BitReliabilityVector inverseToneMapReliabilities(const BitReliabilityVector& input, int bitsPerSubcarrier, int distance);

  public:
    static std::vector<std::vector<int>> getSpatialStreamSourceIndices(int numberOfCodedBitsPerSymbol,
            const std::vector<int>& bitsPerSubcarrier);
    static std::vector<std::vector<int>> getVhtSegmentSourceIndices(int numberOfCodedBitsPerSpatialStream,
            int bitsPerSubcarrier);
    static std::vector<int> getVhtToneMapperOutputIndices(int toneCount, int toneMappingDistance);

    static Ieee80211LdpcMappedData encodeAndMap(const BitVector& dataBits,
            const Ieee80211DataEncodingPlan& plan, const std::vector<int>& bitsPerSubcarrier,
            int bandwidthMhz, const Ieee80211LdpcDataCoder& coder = Ieee80211LdpcDataCoder());

    static BitReliabilityVector inverseMap(const Ieee80211LdpcMappedReliabilities& mapped,
            const Ieee80211DataEncodingPlan& plan, const std::vector<int>& bitsPerSubcarrier,
            int bandwidthMhz);

    static FecDecodingResult inverseMapAndDecode(const Ieee80211LdpcMappedReliabilities& mapped,
            const Ieee80211DataEncodingPlan& plan, const std::vector<int>& bitsPerSubcarrier,
            int bandwidthMhz, const Ieee80211LdpcDataCoder& coder = Ieee80211LdpcDataCoder());
};

} // namespace physicallayer
} // namespace inet

#endif
