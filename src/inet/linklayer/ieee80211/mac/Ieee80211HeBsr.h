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
    // B0-B1: Variant = 11 (HE variant)
    // B2-B5: Control ID = 3 (BSR)
    // A-Control is: Control ID (3) | (BSR Info << 4)
    // BSR Info is: ACI Bitmap (4 bits) | (Delta TID (2 bits) << 4) | (ACI High (2 bits) << 6) | (Scaling Factor (2 bits) << 8) | (Queue Size High (8 bits) << 10) | (Queue Size All (8 bits) << 18)
    
    uint8_t aciBitmap = 1 << status.accessCategory;
    uint8_t deltaTid = 0;
    uint8_t aciHigh = status.accessCategory;
    
    // Calculate Scaling Factor and scaled queue size
    uint8_t sf = 0;
    uint32_t scaledSize = 0;
    uint32_t size = status.queueSize;
    if (size == 0) {
        sf = 0;
        scaledSize = 0;
    } else if (size <= 256 * 254) {
        sf = 0;
        scaledSize = (size + 255) / 256;
    } else if (size <= 1024 * 254) {
        sf = 1;
        scaledSize = (size + 1023) / 1024;
    } else if (size <= 8192 * 254) {
        sf = 2;
        scaledSize = (size + 8191) / 8192;
    } else {
        sf = 3;
        scaledSize = (size + 65535) / 65536;
        if (scaledSize > 254)
            scaledSize = 254;
    }
    
    uint32_t bsrInfo = (aciBitmap & 0xF) |
                       ((deltaTid & 0x3) << 4) |
                       ((aciHigh & 0x3) << 6) |
                       ((sf & 0x3) << 8) |
                       ((scaledSize & 0xFF) << 10) |
                       ((scaledSize & 0xFF) << 18);
                       
    uint32_t aControl = IEEE80211_HE_BSR_CONTROL_ID | (bsrInfo << 4);
    
    // HE variant HT Control is: 3 | (aControl << 2)
    return 3 | (aControl << 2);
}

inline bool unpackHeBufferStatusHtControl(uint32_t htControl, Ieee80211HeBufferStatus& status)
{
    // Variant must be 11 (HE variant)
    if ((htControl & 3) != 3)
        return false;
    
    uint32_t aControl = htControl >> 2;
    if ((aControl & 0xF) != IEEE80211_HE_BSR_CONTROL_ID)
        return false;
        
    uint32_t bsrInfo = aControl >> 4;
    uint8_t deltaTid = (bsrInfo >> 4) & 0x3;
    uint8_t aciHigh = (bsrInfo >> 6) & 0x3;
    uint8_t sf = (bsrInfo >> 8) & 0x3;
    uint8_t scaledSizeAll = (bsrInfo >> 18) & 0xFF;
    
    status.accessCategory = aciHigh;
    
    // Map aciHigh and deltaTid back to tid
    if (aciHigh == 3) status.tid = (deltaTid == 1) ? 7 : 6;
    else if (aciHigh == 2) status.tid = (deltaTid == 1) ? 5 : 4;
    else if (aciHigh == 0) status.tid = (deltaTid == 3) ? 3 : 0;
    else if (aciHigh == 1) status.tid = (deltaTid == 1) ? 2 : 1;
    else status.tid = aciHigh * 2;
    
    // Convert scaledSizeAll back to queueSize in bytes
    uint32_t unit = 256;
    if (sf == 1) unit = 1024;
    else if (sf == 2) unit = 8192;
    else if (sf == 3) unit = 65536;
    status.queueSize = scaledSizeAll * unit;
    
    return true;
}

} // namespace ieee80211
} // namespace inet

#endif

