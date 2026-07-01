//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211EhtMru.h"

#include <stdexcept>

namespace inet {
namespace physicallayer {

int getEhtChannelToneCount(Hz bandwidth)
{
    double mhz = bandwidth.get() / 1e6;
    if (std::abs(mhz - 20) < 0.5)
        return 242;
    if (std::abs(mhz - 40) < 0.5)
        return 484;
    if (std::abs(mhz - 80) < 0.5)
        return 996;
    if (std::abs(mhz - 160) < 0.5)
        return 1992;
    if (std::abs(mhz - 320) < 0.5)
        return 3984;
    throw std::invalid_argument("IEEE 802.11be EHT RU layouts require a 20, 40, 80, 160, or 320 MHz channel");
}

int getEhtMruDataSubcarrierCount(int toneSize)
{
    // EHT data subcarrier count (N_SD) for basic and multiple RUs
    switch (toneSize) {
        case 26: return 24;
        case 52: return 48;
        case 106: return 102;
        case 242: return 234;
        case 484: return 468;
        case 996: return 980;
        case 1992: return 1960;
        case 3984: return 3920;
        default:
            throw std::invalid_argument("Unknown EHT RU tone size");
    }
}

int getEhtMruPilotSubcarrierCount(int toneSize)
{
    // EHT pilot subcarrier count (N_SP) for basic and multiple RUs
    switch (toneSize) {
        case 26: return 2;
        case 52: return 4;
        case 106: return 4;
        case 242: return 8;
        case 484: return 16;
        case 996: return 16;
        case 1992: return 32;
        case 3984: return 64;
        default:
            throw std::invalid_argument("Unknown EHT RU tone size");
    }
}

} // namespace physicallayer
} // namespace inet
