//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211LdpcPhyCalculator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace inet {
namespace physicallayer {

static bool isSupportedLdpcRate(int numerator, int denominator)
{
    return (numerator == 1 && denominator == 2) ||
            (numerator == 2 && denominator == 3) ||
            (numerator == 3 && denominator == 4) ||
            (numerator == 5 && denominator == 6);
}

bool getIeee80211LdpcRateParameters(double codeRate,
        int& rateNumerator, int& rateDenominator)
{
    for (auto rate : {std::pair<int, int>{1, 2}, {2, 3}, {3, 4}, {5, 6}}) {
        if (std::abs(codeRate - (double)rate.first / rate.second) < 1e-9) {
            rateNumerator = rate.first;
            rateDenominator = rate.second;
            return true;
        }
    }
    return false;
}

Ieee80211LdpcCodewordGeometry calculateIeee80211LdpcCodewordGeometry(
        int64_t payloadBits, int64_t availableBits,
        int rateNumerator, int rateDenominator)
{
    using Wide = __int128;
    Ieee80211LdpcCodewordGeometry result;
    if (payloadBits <= 0 || availableBits <= 0 || !isSupportedLdpcRate(rateNumerator, rateDenominator)) {
        result.error = "invalid IEEE 802.11 LDPC payload, capacity, or code rate";
        return result;
    }

    if (availableBits <= 648) {
        result.codewordCount = 1;
        result.codewordLength = (Wide)availableBits * rateDenominator >=
                (Wide)payloadBits * rateDenominator + 912LL * (rateDenominator - rateNumerator) ? 1296 : 648;
    }
    else if (availableBits <= 1296) {
        result.codewordCount = 1;
        result.codewordLength = (Wide)availableBits * rateDenominator >=
                (Wide)payloadBits * rateDenominator + 1464LL * (rateDenominator - rateNumerator) ? 1944 : 1296;
    }
    else if (availableBits <= 1944) {
        result.codewordCount = 1;
        result.codewordLength = 1944;
    }
    else if (availableBits <= 2592) {
        result.codewordCount = 2;
        result.codewordLength = (Wide)availableBits * rateDenominator >=
                (Wide)payloadBits * rateDenominator + 2916LL * (rateDenominator - rateNumerator) ? 1944 : 1296;
    }
    else {
        result.codewordLength = 1944;
        const Wide numerator = (Wide)payloadBits * rateDenominator;
        const Wide denominator = 1944LL * rateNumerator;
        const Wide count = numerator / denominator + (numerator % denominator != 0);
        if (count > std::numeric_limits<int>::max()) {
            result.error = "IEEE 802.11 PSDU requires too many LDPC codewords";
            return result;
        }
        result.codewordCount = static_cast<int>(count);
    }

    const Wide codedBits = (Wide)result.codewordCount * result.codewordLength;
    const Wide informationCapacity = codedBits * rateNumerator / rateDenominator;
    if ((Wide)payloadBits > informationCapacity) {
        result.error = "IEEE 802.11 LDPC payload exceeds the selected information capacity";
        return result;
    }
    const Wide shortening = std::max<Wide>(0, informationCapacity - payloadBits);
    const Wide puncturing = std::max<Wide>(0, codedBits - availableBits - shortening);
    if (shortening > std::numeric_limits<int64_t>::max() || puncturing > std::numeric_limits<int64_t>::max()) {
        result.error = "IEEE 802.11 LDPC bit counts exceed the model range";
        return result;
    }
    result.shorteningBits = static_cast<int64_t>(shortening);
    result.initialPuncturingBits = static_cast<int64_t>(puncturing);

    const Wide parityRate = rateDenominator - rateNumerator;
    const Wide scaledPuncturing = (Wide)10 * rateDenominator * puncturing;
    result.primaryAdditionalCapacityCondition =
            scaledPuncturing > codedBits * parityRate &&
            (Wide)5 * parityRate * shortening < (Wide)6 * rateNumerator * puncturing;
    result.extremePuncturingCondition = scaledPuncturing > (Wide)3 * codedBits * parityRate;
    result.requiresAdditionalCodedBits = result.primaryAdditionalCapacityCondition || result.extremePuncturingCondition;
    result.valid = true;
    return result;
}

Ieee80211LdpcBitAllocation calculateIeee80211LdpcBitAllocation(
        const Ieee80211LdpcCodewordGeometry& geometry,
        int64_t payloadBits, int64_t finalAvailableBits,
        int rateNumerator, int rateDenominator)
{
    using Wide = __int128;
    Ieee80211LdpcBitAllocation result;
    if (!geometry || payloadBits <= 0 || finalAvailableBits <= 0 ||
            !isSupportedLdpcRate(rateNumerator, rateDenominator)) {
        result.error = "invalid IEEE 802.11 LDPC final allocation input";
        return result;
    }
    const Wide codedBits = (Wide)geometry.codewordCount * geometry.codewordLength;
    const Wide puncturing = std::max<Wide>(0,
            codedBits - finalAvailableBits - geometry.shorteningBits);
    const Wide repetition = std::max<Wide>(0,
            finalAvailableBits - codedBits * (rateDenominator - rateNumerator) / rateDenominator - payloadBits);
    if (puncturing > std::numeric_limits<int64_t>::max() || repetition > std::numeric_limits<int64_t>::max() ||
            (puncturing > 0 && repetition > 0)) {
        result.error = "IEEE 802.11 LDPC puncturing and repetition outcomes are inconsistent";
        return result;
    }
    result.puncturingBits = static_cast<int64_t>(puncturing);
    result.repetitionBits = static_cast<int64_t>(repetition);
    result.valid = true;
    return result;
}

} // namespace physicallayer
} // namespace inet
