// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_IEEE80211HEOMI_H
#define __INET_IEEE80211HEOMI_H
#include <cstdint>
namespace inet { namespace ieee80211 {
struct Ieee80211HeOperatingMode { uint8_t channelWidth = 0; uint8_t rxNss = 1; bool ulMuDisable = false; };
constexpr uint8_t IEEE80211_HE_OMI_CONTROL_ID = 1;
inline uint32_t packHeOperatingModeHtControl(const Ieee80211HeOperatingMode& mode) {
    uint32_t omInfo = ((mode.rxNss - 1) & 0x7) |
                      ((mode.channelWidth & 0x3) << 3) |
                      ((mode.ulMuDisable ? 1U : 0U) << 5);
    uint32_t aControl = IEEE80211_HE_OMI_CONTROL_ID | (omInfo << 4);
    return 3 | (aControl << 2);
}
inline bool unpackHeOperatingModeHtControl(uint32_t value, Ieee80211HeOperatingMode& mode) {
    if ((value & 3) != 3) return false;
    uint32_t aControl = value >> 2;
    if ((aControl & 0xf) != IEEE80211_HE_OMI_CONTROL_ID) return false;
    uint32_t omInfo = aControl >> 4;
    mode.rxNss = (omInfo & 0x7) + 1;
    mode.channelWidth = (omInfo >> 3) & 0x3;
    mode.ulMuDisable = ((omInfo >> 5) & 1) != 0;
    return true;
}
} }
#endif
