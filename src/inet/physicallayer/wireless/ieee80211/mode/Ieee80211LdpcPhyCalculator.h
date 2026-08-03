//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCPHYCALCULATOR_H
#define __INET_IEEE80211LDPCPHYCALCULATOR_H

#include <cstdint>
#include <string>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/**
 * Amendment-neutral result of IEEE Std 802.11-2024 Table 19-16 and
 * Equations 19-37 through 19-42. Generation-specific calculators supply
 * N_pld and N_avbits and decide whether the requested extra capacity is a
 * complete OFDM symbol or a pre-FEC symbol segment.
 */
struct Ieee80211LdpcCodewordGeometry
{
    bool valid = false;
    std::string error;
    int codewordCount = 0;
    int codewordLength = 0;
    int64_t shorteningBits = 0;
    int64_t initialPuncturingBits = 0;
    bool primaryAdditionalCapacityCondition = false;
    bool extremePuncturingCondition = false;
    bool requiresAdditionalCodedBits = false;

    explicit operator bool() const { return valid; }
};

struct Ieee80211LdpcBitAllocation
{
    bool valid = false;
    std::string error;
    int64_t puncturingBits = 0;
    int64_t repetitionBits = 0;

    explicit operator bool() const { return valid; }
};

INET_API bool getIeee80211LdpcRateParameters(double codeRate,
        int& rateNumerator, int& rateDenominator);

INET_API Ieee80211LdpcCodewordGeometry calculateIeee80211LdpcCodewordGeometry(
        int64_t payloadBits, int64_t availableBits,
        int rateNumerator, int rateDenominator);

INET_API Ieee80211LdpcBitAllocation calculateIeee80211LdpcBitAllocation(
        const Ieee80211LdpcCodewordGeometry& geometry,
        int64_t payloadBits, int64_t finalAvailableBits,
        int rateNumerator, int rateDenominator);

} // namespace physicallayer
} // namespace inet

#endif
