//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HESIGCODEC_H
#define __INET_IEEE80211HESIGCODEC_H

#include <algorithm>
#include <string>
#include <vector>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace physicallayer {

struct Ieee80211HeSigBRuAllocation
{
    std::vector<uint8_t> allocationCodes;
    std::vector<Ieee80211HeRu> rus;
};

struct Ieee80211HeSigCodecResult
{
    bool valid = false;
    std::string error;
    Ieee80211HeSigBRuAllocation allocation;

    explicit operator bool() const { return valid; }
};

inline Ieee80211HeSigCodecResult encodeHeSigBRuAllocation(
        const std::vector<Ieee80211HeRu>& rus, Hz channelBandwidth)
{
    Ieee80211HeSigCodecResult result;
    if (!validateHeRuLayout(rus, channelBandwidth)) {
        result.error = "overlapping, out-of-band, or reserved HE RU allocation";
        return result;
    }
    auto catalog = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    result.allocation.rus = rus;
    for (const auto& ru : rus) {
        auto it = std::find_if(catalog.begin(), catalog.end(), [&] (const auto& candidate) {
            return candidate.toneSize == ru.toneSize && candidate.toneOffset == ru.toneOffset;
        });
        if (it == catalog.end()) {
            result.error = "HE RU geometry has no HE-SIG-B allocation encoding";
            result.allocation = {};
            return result;
        }
        auto index = std::distance(catalog.begin(), it);
        if (index > 255) {
            result.error = "HE-SIG-B allocation catalog index overflow";
            result.allocation = {};
            return result;
        }
        result.allocation.allocationCodes.push_back(index);
    }
    result.valid = true;
    return result;
}

inline Ieee80211HeSigCodecResult decodeHeSigBRuAllocation(
        const std::vector<uint8_t>& allocationCodes, Hz channelCenterFrequency,
        Hz channelBandwidth)
{
    Ieee80211HeSigCodecResult result;
    auto zeroCenteredCatalog = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    auto centeredCatalog = getHeRuAllocationCatalog(channelCenterFrequency, channelBandwidth);
    for (uint8_t code : allocationCodes) {
        if (code >= zeroCenteredCatalog.size()) {
            result.error = "reserved HE-SIG-B RU allocation code";
            return result;
        }
        const auto& encoded = zeroCenteredCatalog[code];
        auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&] (const auto& candidate) {
            return candidate.toneSize == encoded.toneSize &&
                    candidate.toneOffset == encoded.toneOffset;
        });
        if (it == centeredCatalog.end()) {
            result.error = "HE-SIG-B RU allocation cannot be resolved";
            return result;
        }
        result.allocation.rus.push_back(*it);
        result.allocation.allocationCodes.push_back(code);
    }
    if (!validateHeRuLayout(result.allocation.rus, channelBandwidth)) {
        result.error = "decoded HE-SIG-B RU allocation overlaps";
        result.allocation = {};
        return result;
    }
    result.valid = true;
    return result;
}

} // namespace physicallayer
} // namespace inet

#endif
