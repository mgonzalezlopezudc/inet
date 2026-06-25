//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEBSR_H
#define __INET_IEEE80211HEBSR_H

#include <algorithm>
#include <cstdint>

namespace inet {
namespace ieee80211 {

struct Ieee80211HeBufferStatus {
    uint8_t tid = 0;
    uint8_t accessCategory = 0;
    uint32_t queueSize = 0;
};

constexpr uint8_t IEEE80211_HE_BSR_CONTROL_ID = 3;
constexpr uint32_t IEEE80211_HE_BSR_MAX_QUEUE_SIZE = 0x3FFFFF;

inline uint32_t packHeBufferStatusHtControl(const Ieee80211HeBufferStatus& status)
{
    return IEEE80211_HE_BSR_CONTROL_ID |
            ((status.tid & 0xF) << 4) |
            ((status.accessCategory & 0x3) << 8) |
            (std::min<uint32_t>(status.queueSize, IEEE80211_HE_BSR_MAX_QUEUE_SIZE) << 10);
}

inline bool unpackHeBufferStatusHtControl(uint32_t htControl, Ieee80211HeBufferStatus& status)
{
    if ((htControl & 0xF) != IEEE80211_HE_BSR_CONTROL_ID)
        return false;
    status.tid = (htControl >> 4) & 0xF;
    status.accessCategory = (htControl >> 8) & 0x3;
    status.queueSize = (htControl >> 10) & IEEE80211_HE_BSR_MAX_QUEUE_SIZE;
    return true;
}

} // namespace ieee80211
} // namespace inet

#endif

