//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EHTPREAMBLEPUNCTURING_H
#define __INET_IEEE80211EHTPREAMBLEPUNCTURING_H

#include <cmath>
#include <cstdint>

#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/** The EHT PPDU class determines the puncturing resolution and patterns. */
enum class Ieee80211EhtPreamblePuncturingMode {
    NON_OFDMA,
    OFDMA,
};

inline int getEhtPreamblePuncturingSubchannelCount(Hz bandwidth)
{
    int widthMHz = (int)std::lround(bandwidth.get() / 1e6);
    return widthMHz == 20 || widthMHz == 40 || widthMHz == 80 || widthMHz == 160 || widthMHz == 320 ?
            widthMHz / 20 : 0;
}

inline int countEhtPuncturedBits(uint16_t value)
{
    int count = 0;
    while (value != 0) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

/**
 * Validates an EHT preamble-puncturing bitmap.
 *
 * The bitmap uses the INET/TXVECTOR polarity (1 means punctured) and orders
 * 20 MHz subchannels from the lowest frequency upward. This is also the
 * polarity used by the EHT Operation Disabled Subchannel Bitmap. The
 * standard's U-SIG fields use encoded pattern values; conversion to those
 * fields belongs to the PHY codec boundary.
 *
 * IEEE Std 802.11be-2024: 9.4.2.321 / Table 9-417e, 35.15.2,
 * 36.3.12.11.2, 36.3.12.11.3, and Table 36-30.
 *
 * primary20SubchannelIndex is relative to the lowest-frequency 20 MHz
 * subchannel. Pass -1 when channel geometry is not available; in that case
 * the pattern and reserved-bit checks are still performed.
 */
inline bool isValidIeee80211EhtPreamblePuncturing(uint16_t puncturedMask,
        Hz bandwidth, int primary20SubchannelIndex = -1,
        Ieee80211EhtPreamblePuncturingMode mode = Ieee80211EhtPreamblePuncturingMode::NON_OFDMA)
{
    int subchannelCount = getEhtPreamblePuncturingSubchannelCount(bandwidth);
    if (subchannelCount == 0)
        return false;
    if (primary20SubchannelIndex < -1 || primary20SubchannelIndex >= subchannelCount)
        return false;

    uint16_t widthMask = subchannelCount == 16 ? 0xffff : uint16_t((1u << subchannelCount) - 1);
    if ((puncturedMask & ~widthMask) != 0)
        return false;
    if (primary20SubchannelIndex >= 0 &&
            (puncturedMask & (uint16_t(1) << primary20SubchannelIndex)) != 0)
        return false;

    int widthMHz = subchannelCount * 20;
    if (mode == Ieee80211EhtPreamblePuncturingMode::OFDMA) {
        // Above 40 MHz each 80 MHz subblock independently uses the patterns
        // from 36.3.12.11.2. A complete 0000 block is valid only when it does
        // not contain the primary 20 MHz channel.
        if (widthMHz <= 40)
            return puncturedMask == 0;
        constexpr uint16_t allowedPuncturedNibbles =
                (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
                (1u << 4) | (1u << 6) | (1u << 8) | (1u << 12) |
                (1u << 15);
        for (int offset = 0; offset < subchannelCount; offset += 4) {
            uint16_t nibble = (puncturedMask >> offset) & 0xf;
            if ((allowedPuncturedNibbles & (uint16_t(1u) << nibble)) == 0)
                return false;
            if (primary20SubchannelIndex >= offset && primary20SubchannelIndex < offset + 4 && nibble == 0xf)
                return false;
        }
        return true;
    }

    // Table 36-30 permits no puncturing at 20/40 MHz.
    if (widthMHz <= 40)
        return puncturedMask == 0;

    if (widthMHz == 80)
        // No puncturing or exactly one punctured 20 MHz subchannel.
        return countEhtPuncturedBits(puncturedMask) <= 1;

    if (widthMHz == 160) {
        // Table 36-30 values 0..12: no puncturing, one 20 MHz subchannel,
        // or one aligned 40 MHz pair.
        if (countEhtPuncturedBits(puncturedMask) <= 1)
            return true;
        return puncturedMask == 0x0003 || puncturedMask == 0x000c ||
                puncturedMask == 0x0030 || puncturedMask == 0x00c0;
    }

    // For 320 MHz Table 36-30 has eight 40 MHz positions. Expand each
    // punctured 40 MHz position into two adjacent 20 MHz bitmap bits.
    uint16_t slots = 0;
    for (int slot = 0; slot < 8; ++slot) {
        uint16_t pair = (puncturedMask >> (2 * slot)) & 0x3;
        if (pair == 0x3)
            slots |= uint16_t(1) << slot;
        else if (pair != 0)
            return false;
    }

    // Table 36-30 values 0..24, expressed as eight 40 MHz slots.
    if (slots == 0 || countEhtPuncturedBits(slots) == 1)
        return true;
    if (slots == 0x03 || slots == 0x0c || slots == 0x30 || slots == 0xc0)
        return true;
    return (slots & 0x03) == 0x03 && countEhtPuncturedBits(slots) == 3 ||
            (slots & 0xc0) == 0xc0 && countEhtPuncturedBits(slots) == 3;
}

} // namespace physicallayer
} // namespace inet

#endif // __INET_IEEE80211EHTPREAMBLEPUNCTURING_H
