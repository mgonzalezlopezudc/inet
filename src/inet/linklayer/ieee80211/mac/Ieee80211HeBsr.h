//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEBSR_H
#define __INET_IEEE80211HEBSR_H

#include <cstdint>
#include <limits>

#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"

namespace inet {
namespace ieee80211 {

/** IEEE 802.11-2024 Figure 9-30 BSR Control Information fields. */
struct Ieee80211HeBufferStatus {
    uint8_t aciBitmap = 0;
    uint8_t deltaTid = 0;
    uint8_t aciHigh = 0;
    uint8_t scalingFactor = 0;
    uint8_t queueSizeHigh = 0;
    uint8_t queueSizeAll = 0;
};

enum class Ieee80211HeQueueSizeKind {
    QUANTIZED,
    OVERFLOW,
    UNKNOWN
};

constexpr uint8_t IEEE80211_HE_VARIANT = 3;
constexpr uint8_t IEEE80211_HE_BSR_CONTROL_ID = 3;
constexpr uint8_t IEEE80211_HE_BSR_OVERFLOW_QUEUE_CODE = 254;
constexpr uint8_t IEEE80211_HE_BSR_UNKNOWN_QUEUE_CODE = 255;
constexpr uint32_t IEEE80211_HE_BSR_UNKNOWN_QUEUE_SIZE = std::numeric_limits<uint32_t>::max();

inline uint32_t getHeBufferStatusScaleUnit(uint8_t scalingFactor)
{
    // IEEE 802.11-2024 Table 9-33.
    static constexpr uint32_t units[] = {16, 256, 2048, 32768};
    return scalingFactor < 4 ? units[scalingFactor] : 0;
}

inline bool isValidHeBufferStatus(const Ieee80211HeBufferStatus& status)
{
    if (status.aciBitmap > 0xF || status.deltaTid > 3 || status.aciHigh > 3 || status.scalingFactor > 3)
        return false;
    uint8_t numberOfSetAcis = 0;
    for (uint8_t bitmap = status.aciBitmap; bitmap != 0; bitmap >>= 1)
        numberOfSetAcis += bitmap & 1;
    // IEEE 802.11-2024 Table 9-32. Delta TID expresses a represented TID
    // count; it does not identify any particular TID.
    return numberOfSetAcis == 0 ? status.deltaTid == 3 : status.deltaTid <= (numberOfSetAcis < 3 ? numberOfSetAcis : 3);
}

inline bool packHeBufferStatusHtControl(const Ieee80211HeBufferStatus& status, uint32_t& htControl)
{
    if (!isValidHeBufferStatus(status))
        return false;
    htControl = IEEE80211_HE_VARIANT |
            (static_cast<uint32_t>(IEEE80211_HE_BSR_CONTROL_ID) << 2) |
            (static_cast<uint32_t>(status.aciBitmap) << 6) |
            (static_cast<uint32_t>(status.deltaTid) << 10) |
            (static_cast<uint32_t>(status.aciHigh) << 12) |
            (static_cast<uint32_t>(status.scalingFactor) << 14) |
            (static_cast<uint32_t>(status.queueSizeHigh) << 16) |
            (static_cast<uint32_t>(status.queueSizeAll) << 24);
    return true;
}

inline bool unpackHeBufferStatusHtControl(uint32_t htControl, Ieee80211HeBufferStatus& status)
{
    if ((htControl & 0x3) != IEEE80211_HE_VARIANT ||
            ((htControl >> 2) & 0xF) != IEEE80211_HE_BSR_CONTROL_ID)
        return false;
    status.aciBitmap = (htControl >> 6) & 0xF;
    status.deltaTid = (htControl >> 10) & 0x3;
    status.aciHigh = (htControl >> 12) & 0x3;
    status.scalingFactor = (htControl >> 14) & 0x3;
    status.queueSizeHigh = (htControl >> 16) & 0xFF;
    status.queueSizeAll = (htControl >> 24) & 0xFF;
    return isValidHeBufferStatus(status);
}

inline Ieee80211HeQueueSizeKind decodeHeBufferStatusQueueSize(uint8_t queueCode,
        uint8_t scalingFactor, uint32_t& queueSize)
{
    auto unit = getHeBufferStatusScaleUnit(scalingFactor);
    if (queueCode < IEEE80211_HE_BSR_OVERFLOW_QUEUE_CODE) {
        queueSize = queueCode * unit;
        return Ieee80211HeQueueSizeKind::QUANTIZED;
    }
    if (queueCode == IEEE80211_HE_BSR_OVERFLOW_QUEUE_CODE) {
        // This is a strict lower bound, not a quantized upper bound.
        queueSize = queueCode * unit;
        return Ieee80211HeQueueSizeKind::OVERFLOW;
    }
    queueSize = IEEE80211_HE_BSR_UNKNOWN_QUEUE_SIZE;
    return Ieee80211HeQueueSizeKind::UNKNOWN;
}

inline bool encodeHeBufferStatusQueueSize(uint32_t queueSize, uint8_t scalingFactor, uint8_t& queueCode)
{
    if (queueSize == IEEE80211_HE_BSR_UNKNOWN_QUEUE_SIZE) {
        queueCode = IEEE80211_HE_BSR_UNKNOWN_QUEUE_CODE;
        return true;
    }
    auto unit = getHeBufferStatusScaleUnit(scalingFactor);
    auto roundedCode = (static_cast<uint64_t>(queueSize) + unit - 1) / unit;
    if (roundedCode <= 253) {
        queueCode = roundedCode;
        return true;
    }
    // Code 254 specifically means greater than 254 * SF. Values in the gap
    // (253 * SF, 254 * SF] require a larger scaling factor.
    if (queueSize > static_cast<uint64_t>(254) * unit) {
        queueCode = IEEE80211_HE_BSR_OVERFLOW_QUEUE_CODE;
        return true;
    }
    return false;
}

inline bool encodeHeBufferStatusQueueSizes(uint32_t queueSizeHigh, uint32_t queueSizeAll,
        Ieee80211HeBufferStatus& status)
{
    for (uint8_t scalingFactor = 0; scalingFactor < 4; ++scalingFactor) {
        uint8_t queueSizeHighCode;
        uint8_t queueSizeAllCode;
        if (encodeHeBufferStatusQueueSize(queueSizeHigh, scalingFactor, queueSizeHighCode) &&
                encodeHeBufferStatusQueueSize(queueSizeAll, scalingFactor, queueSizeAllCode) &&
                queueSizeHighCode != IEEE80211_HE_BSR_OVERFLOW_QUEUE_CODE &&
                queueSizeAllCode != IEEE80211_HE_BSR_OVERFLOW_QUEUE_CODE) {
            status.scalingFactor = scalingFactor;
            status.queueSizeHigh = queueSizeHighCode;
            status.queueSizeAll = queueSizeAllCode;
            return true;
        }
    }
    // If no finite upper bound exists, choose the largest scale that can
    // represent both values. This gives the tightest available strict lower
    // bound for any field that uses overflow code 254.
    for (int scalingFactor = 3; scalingFactor >= 0; --scalingFactor) {
        uint8_t queueSizeHighCode;
        uint8_t queueSizeAllCode;
        if (encodeHeBufferStatusQueueSize(queueSizeHigh, scalingFactor, queueSizeHighCode) &&
                encodeHeBufferStatusQueueSize(queueSizeAll, scalingFactor, queueSizeAllCode)) {
            status.scalingFactor = scalingFactor;
            status.queueSizeHigh = queueSizeHighCode;
            status.queueSizeAll = queueSizeAllCode;
            return true;
        }
    }
    return false;
}

inline uint8_t mapHeBufferStatusTidToAci(uint8_t tid)
{
    switch (tid) {
        case 0: case 3: return 0;
        case 1: case 2: return 1;
        case 4: case 5: return 2;
        case 6: case 7: return 3;
        default: return 0xFF;
    }
}

inline uint8_t mapHeBufferStatusAccessCategoryToAci(AccessCategory accessCategory)
{
    switch (accessCategory) {
        case AC_BE: return 0;
        case AC_BK: return 1;
        case AC_VI: return 2;
        case AC_VO: return 3;
        default: return 0xFF;
    }
}

inline AccessCategory mapHeBufferStatusAciToAccessCategory(uint8_t aci)
{
    switch (aci) {
        case 0: return AC_BE;
        case 1: return AC_BK;
        case 2: return AC_VI;
        case 3: return AC_VO;
        default: return AC_NUMCATEGORIES;
    }
}

inline uint8_t getHeBufferStatusRepresentativeTid(uint8_t aci)
{
    // The BSR wire field carries no exact TID. These stable representatives
    // only support the legacy Ieee80211DataHeader::bufferStatusTid API.
    static constexpr uint8_t representativeTids[] = {0, 1, 4, 6};
    return aci < 4 ? representativeTids[aci] : 0xFF;
}

inline bool encodeHeSingleAcBufferStatus(uint8_t tid, AccessCategory accessCategory, uint32_t queueSize,
        Ieee80211HeBufferStatus& status)
{
    auto derivedAci = mapHeBufferStatusTidToAci(tid);
    auto accessCategoryAci = mapHeBufferStatusAccessCategoryToAci(accessCategory);
    if (derivedAci == 0xFF || accessCategoryAci == 0xFF || accessCategoryAci != derivedAci)
        return false;
    status.aciBitmap = 1 << derivedAci;
    status.deltaTid = 0;
    status.aciHigh = derivedAci;
    return encodeHeBufferStatusQueueSizes(queueSize, queueSize, status);
}

} // namespace ieee80211
} // namespace inet

#endif
