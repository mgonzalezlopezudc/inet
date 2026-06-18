//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HERU_H
#define __INET_IEEE80211HERU_H

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"

namespace inet {

using namespace inet::units::values;

namespace physicallayer {

/**
 * IEEE 802.11ax resource unit description.
 *
 * The allocationIndex is local to the selected RU layout. The toneOffset is
 * measured from the first occupied HE tone of the channel and makes the
 * frequency placement independent of the number of scheduled users.
 */
struct Ieee80211HeRu {
    int index = -1;
    int toneSize = 0;
    int toneOffset = 0;
    int dataSubcarriers = 0;
    int pilotSubcarriers = 0;
    Hz centerFrequency = Hz(NaN);
    Hz bandwidth = Hz(NaN);

    bool operator==(const Ieee80211HeRu& other) const
    {
        return index == other.index && toneSize == other.toneSize &&
                toneOffset == other.toneOffset && centerFrequency == other.centerFrequency &&
                bandwidth == other.bandwidth;
    }
};

inline int getHeRuDataSubcarrierCount(int toneSize)
{
    switch (toneSize) {
        case 26: return 24;
        case 52: return 48;
        case 106: return 102;
        case 242: return 234;
        case 484: return 468;
        case 996: return 980;
        case 1992: return 1960;
        default: throw std::invalid_argument("Unsupported IEEE 802.11ax RU tone size");
    }
}

inline int getHeRuPilotSubcarrierCount(int toneSize)
{
    switch (toneSize) {
        case 26: return 2;
        case 52: return 4;
        case 106: return 4;
        case 242: return 8;
        case 484: return 16;
        case 996: return 16;
        case 1992: return 32;
        default: throw std::invalid_argument("Unsupported IEEE 802.11ax RU tone size");
    }
}

inline int getHeChannelToneCount(Hz bandwidth)
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
    throw std::invalid_argument("IEEE 802.11ax RU layouts require a 20, 40, 80, or 160 MHz channel");
}

inline int getHeMaxRuCount(Hz bandwidth)
{
    switch (getHeChannelToneCount(bandwidth)) {
        case 242: return 9;
        case 484: return 18;
        case 996: return 37;
        case 1992: return 74;
        default: return 0;
    }
}

inline std::vector<int> getHeEqualRuCounts(Hz bandwidth)
{
    switch (getHeChannelToneCount(bandwidth)) {
        case 242: return {1, 2, 4, 9};
        case 484: return {1, 2, 4, 8, 18};
        case 996: return {1, 2, 4, 8, 16, 37};
        case 1992: return {1, 2, 4, 8, 16, 32, 74};
        default: return {};
    }
}

inline int getHeEqualRuToneSize(Hz bandwidth, int count)
{
    int channelTones = getHeChannelToneCount(bandwidth);
    if (count == 1)
        return channelTones;
    if (channelTones == 242) {
        if (count == 2) return 106;
        if (count == 4) return 52;
        if (count == 9) return 26;
    }
    else if (channelTones == 484) {
        if (count == 2) return 242;
        if (count == 4) return 106;
        if (count == 8) return 52;
        if (count == 18) return 26;
    }
    else if (channelTones == 996) {
        if (count == 2) return 484;
        if (count == 4) return 242;
        if (count == 8) return 106;
        if (count == 16) return 52;
        if (count == 37) return 26;
    }
    else if (channelTones == 1992) {
        if (count == 2) return 996;
        if (count == 4) return 484;
        if (count == 8) return 242;
        if (count == 16) return 106;
        if (count == 32) return 52;
        if (count == 74) return 26;
    }
    throw std::invalid_argument("The requested count is not a standard equal-sized HE RU layout");
}

inline Ieee80211HeRu makeHeRu(Hz centerFrequency, int channelTones,
        int index, int toneSize, int toneOffset)
{
    constexpr double HE_TONE_SPACING = 78125;
    Ieee80211HeRu ru;
    ru.index = index;
    ru.toneSize = toneSize;
    ru.toneOffset = toneOffset;
    ru.dataSubcarriers = getHeRuDataSubcarrierCount(toneSize);
    ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(toneSize);
    ru.bandwidth = Hz(toneSize * HE_TONE_SPACING);
    double centerTone = toneOffset + toneSize / 2.0 - channelTones / 2.0;
    ru.centerFrequency = centerFrequency + Hz(centerTone * HE_TONE_SPACING);
    return ru;
}

/**
 * Appends the canonical HE allocation tree in pre-order. The small gaps model
 * the DC/guard tones between standard sibling RUs. A node may be allocated as
 * a whole, or replaced by all of its children:
 *
 * 2x996 -> 996 + 996
 * 996   -> 484 + central 26 + 484
 * 484   -> 242 + 242
 * 242   -> 106 + central 26 + 106
 * 106   -> 52 + 52
 * 52    -> 26 + 26
 */
inline void appendHeRuAllocationTree(std::vector<Ieee80211HeRu>& catalog,
        Hz centerFrequency, int channelTones, int toneSize, int toneOffset, int& nextIndex)
{
    catalog.push_back(makeHeRu(centerFrequency, channelTones, nextIndex++, toneSize, toneOffset));
    switch (toneSize) {
        case 1992:
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 996, toneOffset, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 996, toneOffset + 996, nextIndex);
            break;
        case 996:
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 484, toneOffset, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 26, toneOffset + 485, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 484, toneOffset + 512, nextIndex);
            break;
        case 484:
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 242, toneOffset, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 242, toneOffset + 242, nextIndex);
            break;
        case 242:
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 106, toneOffset, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 26, toneOffset + 108, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 106, toneOffset + 136, nextIndex);
            break;
        case 106:
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 52, toneOffset, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 52, toneOffset + 54, nextIndex);
            break;
        case 52:
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 26, toneOffset, nextIndex);
            appendHeRuAllocationTree(catalog, centerFrequency, channelTones, 26, toneOffset + 26, nextIndex);
            break;
        case 26:
            break;
        default:
            throw std::invalid_argument("Unsupported IEEE 802.11ax RU allocation-tree node");
    }
}

inline std::vector<Ieee80211HeRu> getHeRuAllocationCatalog(
        Hz centerFrequency, Hz channelBandwidth)
{
    int channelTones = getHeChannelToneCount(channelBandwidth);
    std::vector<Ieee80211HeRu> catalog;
    int nextIndex = 0;
    appendHeRuAllocationTree(catalog, centerFrequency, channelTones,
            channelTones, 0, nextIndex);
    return catalog;
}

/**
 * Returns the standard equal-sized layout as one level of the allocation tree.
 */
inline std::vector<Ieee80211HeRu> getHeEqualRuLayout(Hz centerFrequency, Hz channelBandwidth, int count)
{
    int ruTones = getHeEqualRuToneSize(channelBandwidth, count);
    std::vector<Ieee80211HeRu> result;
    for (const auto& ru : getHeRuAllocationCatalog(centerFrequency, channelBandwidth))
        if (ru.toneSize == ruTones)
            result.push_back(ru);
    std::sort(result.begin(), result.end(),
            [] (const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
                return a.toneOffset < b.toneOffset;
            });
    if ((int)result.size() != count)
        throw std::logic_error("Incomplete IEEE 802.11ax equal-sized RU allocation tree");
    return result;
}

inline bool validateHeRuLayout(const std::vector<Ieee80211HeRu>& layout, Hz channelBandwidth)
{
    int channelTones = getHeChannelToneCount(channelBandwidth);
    auto catalog = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    std::vector<bool> occupied(channelTones, false);
    std::set<int> indices;
    for (const auto& ru : layout) {
        if (ru.index < 0 || ru.toneOffset < 0 || ru.toneOffset + ru.toneSize > channelTones)
            return false;
        if (!indices.insert(ru.index).second)
            return false;
        auto catalogIt = std::find_if(catalog.begin(), catalog.end(),
                [&] (const Ieee80211HeRu& candidate) {
                    return candidate.index == ru.index &&
                            candidate.toneSize == ru.toneSize &&
                            candidate.toneOffset == ru.toneOffset;
                });
        if (catalogIt == catalog.end())
            return false;
        for (int tone = ru.toneOffset; tone < ru.toneOffset + ru.toneSize; ++tone) {
            if (occupied[tone])
                return false;
            occupied[tone] = true;
        }
    }
    return true;
}

/**
 * Allocates exact requested RU sizes at canonical, non-overlapping positions.
 * Requests should be ordered from largest to smallest for deterministic fitting.
 */
inline bool allocateHeRus(Hz centerFrequency, Hz channelBandwidth,
        const std::vector<int>& requestedToneSizes, std::vector<Ieee80211HeRu>& allocations)
{
    auto catalog = getHeRuAllocationCatalog(centerFrequency, channelBandwidth);
    int channelTones = getHeChannelToneCount(channelBandwidth);
    std::vector<bool> occupied(channelTones, false);
    allocations.clear();
    allocations.reserve(requestedToneSizes.size());
    for (int requestedToneSize : requestedToneSizes) {
        auto selected = catalog.end();
        for (auto it = catalog.begin(); it != catalog.end(); ++it) {
            if (it->toneSize != requestedToneSize)
                continue;
            bool free = true;
            for (int tone = it->toneOffset; tone < it->toneOffset + it->toneSize; ++tone)
                if (occupied[tone]) {
                    free = false;
                    break;
                }
            if (free) {
                selected = it;
                break;
            }
        }
        if (selected == catalog.end()) {
            allocations.clear();
            return false;
        }
        allocations.push_back(*selected);
        for (int tone = selected->toneOffset; tone < selected->toneOffset + selected->toneSize; ++tone)
            occupied[tone] = true;
    }
    return validateHeRuLayout(allocations, channelBandwidth);
}

// Compatibility entry point retained for existing callers. Unlike the old
// approximation, it accepts only standard equal-sized RU counts.
inline std::vector<Ieee80211HeRu> calculateHeRus(Hz centerFrequency, Hz bandwidth, int numRUs)
{
    if (numRUs <= 0)
        return {};
    return getHeEqualRuLayout(centerFrequency, bandwidth, numRUs);
}

} // namespace physicallayer
} // namespace inet

#endif
