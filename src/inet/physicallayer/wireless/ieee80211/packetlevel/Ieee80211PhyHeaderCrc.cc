//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderCrc.h"

namespace inet {
namespace physicallayer {

uint8_t computeIeee80211PhyHeaderCrc(const std::vector<bool>& protectedBits)
{
    if (protectedBits.size() != 34)
        throw cRuntimeError("IEEE 802.11 PHY header CRC requires exactly 34 protected bits");
    uint64_t message = 0;
    for (bool bit : protectedBits)
        message = (message << 1) | bit;

    // I(D)=D^33+...+D^26 is equivalent to complementing the first
    // eight transmitted message coefficients before polynomial division.
    uint64_t dividend = (message ^ (uint64_t(0xFF) << 26)) << 8;
    constexpr uint64_t generator = (uint64_t(1) << 8) | (1 << 2) | (1 << 1) | 1;
    for (int degree = 41; degree >= 8; degree--)
        if (dividend & (uint64_t(1) << degree))
            dividend ^= generator << (degree - 8);
    return static_cast<uint8_t>(~dividend);
}

} // namespace physicallayer
} // namespace inet
