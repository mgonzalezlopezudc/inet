//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace inet {
namespace physicallayer {

int getHeChannelToneCount(Hz bandwidth)
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

int getHeMaxRuCount(Hz bandwidth)
{
    switch (getHeChannelToneCount(bandwidth)) {
        case 242: return 9;
        case 484: return 18;
        case 996: return 37;
        case 1992: return 74;
        default: return 0;
    }
}

std::vector<int> getHeEqualRuCounts(Hz bandwidth)
{
    switch (getHeChannelToneCount(bandwidth)) {
        case 242: return {1, 2, 4, 9};
        case 484: return {1, 2, 4, 8, 18};
        case 996: return {1, 2, 4, 8, 16, 37};
        case 1992: return {1, 2, 4, 8, 16, 32, 74};
        default: return {};
    }
}

int getHeEqualRuToneSize(Hz bandwidth, int count)
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

Ieee80211HeRu makeHeRu(Hz centerFrequency, int channelTones,
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

namespace {

void appendHeRuAllocationTree(std::vector<Ieee80211HeRu>& catalog,
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

} // namespace

std::vector<Ieee80211HeRu> getHeRuAllocationCatalog(Hz centerFrequency, Hz channelBandwidth)
{
    int channelTones = getHeChannelToneCount(channelBandwidth);
    std::vector<Ieee80211HeRu> catalog;
    int nextIndex = 0;
    appendHeRuAllocationTree(catalog, centerFrequency, channelTones,
            channelTones, 0, nextIndex);
    return catalog;
}

std::vector<Ieee80211HeRu> getHeEqualRuLayout(Hz centerFrequency, Hz channelBandwidth, int count)
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

bool validateHeRuLayout(const std::vector<Ieee80211HeRu>& layout, Hz channelBandwidth)
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

bool allocateHeRus(Hz centerFrequency, Hz channelBandwidth,
        const std::vector<int>& requestedToneSizes, std::vector<Ieee80211HeRu>& allocations,
        const std::vector<bool>& puncturedSubchannels)
{
    auto catalog = getHeRuAllocationCatalog(centerFrequency, channelBandwidth);
    int channelTones = getHeChannelToneCount(channelBandwidth);
    std::vector<bool> occupied(channelTones, false);
    if (!puncturedSubchannels.empty()) {
        int expectedSubchannels = std::lround(channelBandwidth.get() / 20e6);
        if ((int)puncturedSubchannels.size() != expectedSubchannels)
            throw std::invalid_argument("HE puncturing mask does not match channel bandwidth");
        for (int tone = 0; tone < channelTones; ++tone)
            occupied[tone] = puncturedSubchannels[std::min(expectedSubchannels - 1,
                    tone * expectedSubchannels / channelTones)];
    }
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

std::vector<Ieee80211HeRu> calculateHeRus(Hz centerFrequency, Hz bandwidth, int numRUs)
{
    if (numRUs <= 0)
        return {};
    return getHeEqualRuLayout(centerFrequency, bandwidth, numRUs);
}

} // namespace physicallayer
} // namespace inet
