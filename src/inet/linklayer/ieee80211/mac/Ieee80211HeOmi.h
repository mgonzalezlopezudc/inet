// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_IEEE80211HEOMI_H
#define __INET_IEEE80211HEOMI_H
#include <cstdint>
namespace inet { namespace ieee80211 {
struct Ieee80211HeOperatingMode { uint8_t channelWidth = 0; uint8_t rxNss = 1; bool ulMuDisable = false; };
constexpr uint8_t IEEE80211_HE_OMI_CONTROL_ID = 1;
inline uint32_t packHeOperatingModeHtControl(const Ieee80211HeOperatingMode& mode) {
    return IEEE80211_HE_OMI_CONTROL_ID | ((mode.rxNss - 1) & 0x7) << 4 |
            (mode.channelWidth & 0x3) << 7 | (mode.ulMuDisable ? 1U : 0U) << 9;
}
inline bool unpackHeOperatingModeHtControl(uint32_t value, Ieee80211HeOperatingMode& mode) {
    if ((value & 0xf) != IEEE80211_HE_OMI_CONTROL_ID) return false;
    mode.rxNss = ((value >> 4) & 0x7) + 1; mode.channelWidth = (value >> 7) & 0x3;
    mode.ulMuDisable = ((value >> 9) & 1) != 0; return true;
}
} }
#endif
