//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace inet {
namespace physicallayer {

namespace {

std::vector<std::vector<std::pair<int, int>>> getHeRuRanges(int channelTones, int toneSize)
{
    if (channelTones == 1992) {
        if (toneSize == 1992) {
            auto half = getHeRuRanges(996, 996).front();
            std::vector<std::pair<int, int>> ranges;
            for (auto range : half)
                ranges.emplace_back(range.first - 512, range.second - 512);
            for (auto range : half)
                ranges.emplace_back(range.first + 512, range.second + 512);
            return {ranges};
        }
        auto halfRanges = getHeRuRanges(996, toneSize);
        std::vector<std::vector<std::pair<int, int>>> ranges;
        for (int shift : {-512, 512})
            for (const auto& halfRange : halfRanges) {
                std::vector<std::pair<int, int>> shifted;
                for (auto range : halfRange)
                    shifted.emplace_back(range.first + shift, range.second + shift);
                ranges.push_back(shifted);
            }
        return ranges;
    }
    std::vector<int> starts;
    if (channelTones == 242) {
        if (toneSize == 26) starts = {-121, -95, -68, -42, 0, 17, 43, 70, 96};
        else if (toneSize == 52) starts = {-121, -68, 17, 70};
        else if (toneSize == 106) starts = {-122, 17};
        else if (toneSize == 242) return {{{-122, -2}, {2, 122}}};
    }
    else if (channelTones == 484) {
        if (toneSize == 26) starts = {-243, -217, -189, -163, -136, -109, -83, -55, -29,
                4, 30, 58, 84, 111, 138, 164, 192, 218};
        else if (toneSize == 52) starts = {-243, -189, -109, -55, 4, 58, 138, 192};
        else if (toneSize == 106) starts = {-243, -109, 4, 138};
        else if (toneSize == 242) starts = {-244, 3};
        else if (toneSize == 484) return {{{-244, -3}, {3, 244}}};
    }
    else if (channelTones == 996) {
        if (toneSize == 26) starts = {-499, -473, -445, -419, -392, -365, -339, -311, -285,
                -257, -231, -203, -177, -150, -123, -97, -69, -43, 0,
                18, 44, 72, 98, 125, 152, 178, 206, 232, 260, 286, 314, 340,
                367, 394, 420, 448, 474};
        else if (toneSize == 52) starts = {-499, -445, -365, -311, -257, -203, -123, -69,
                18, 72, 152, 206, 260, 314, 394, 448};
        else if (toneSize == 106) starts = {-499, -365, -257, -123, 18, 152, 260, 394};
        else if (toneSize == 242) starts = {-500, -258, 17, 259};
        else if (toneSize == 484) starts = {-500, 17};
        else if (toneSize == 996) return {{{-500, -3}, {3, 500}}};
    }
    if (starts.empty())
        throw std::invalid_argument("Unsupported HE channel/RU tone-size combination");
    std::vector<std::vector<std::pair<int, int>>> result;
    for (int start : starts) {
        if (start == 0 && toneSize == 26)
            result.push_back({{-16, -4}, {4, 16}});
        else
            result.push_back({{start, start + toneSize - 1}});
    }
    return result;
}

std::vector<std::vector<int>> getPilotGroups(int channelTones, int toneSize)
{
    if (channelTones == 1992) {
        if (toneSize == 1992) {
            auto half = getPilotGroups(996, 996).front();
            std::vector<int> combined;
            for (int shift : {-512, 512})
                for (int pilot : half)
                    combined.push_back(pilot + shift);
            return {combined};
        }
        auto halfGroups = getPilotGroups(996, toneSize);
        std::vector<std::vector<int>> groups;
        for (int shift : {-512, 512})
            for (const auto& half : halfGroups) {
                std::vector<int> shifted;
                for (int pilot : half)
                    shifted.push_back(pilot + shift);
                groups.push_back(shifted);
            }
        return groups;
    }
    std::vector<int> flat;
    int groupSize = getHeRuPilotSubcarrierCount(toneSize);
    if (toneSize == 26) {
        if (channelTones == 242) flat = {-116,-102, -90,-76, -62,-48, -36,-22, -10,10,
                22,36, 48,62, 76,90, 102,116};
        else if (channelTones == 484) flat = {-238,-224, -212,-198, -184,-170, -158,-144,
                -130,-116, -104,-90, -78,-64, -50,-36, -24,-10, 10,24, 36,50,
                64,78, 90,104, 116,130, 144,158, 170,184, 198,212, 224,238};
        else if (channelTones == 996) flat = {-494,-480, -468,-454, -440,-426, -414,-400,
                -386,-372, -360,-346, -334,-320, -306,-292, -280,-266, -252,-238,
                -226,-212, -198,-184, -172,-158, -144,-130, -118,-104, -92,-78,
                -64,-50, -38,-24, -10,10, 24,38, 50,64, 78,92, 104,118,
                130,144, 158,172, 184,198, 212,226, 238,252, 266,280, 292,306,
                320,334, 346,360, 372,386, 400,414, 426,440, 454,468, 480,494};
    }
    else if (toneSize == 52) {
        if (channelTones == 242) flat = {-116,-102,-90,-76, -62,-48,-36,-22,
                22,36,48,62, 76,90,102,116};
        else if (channelTones == 484) flat = {-238,-224,-212,-198, -184,-170,-158,-144,
                -104,-90,-78,-64, -50,-36,-24,-10, 10,24,36,50, 64,78,90,104,
                144,158,170,184, 198,212,224,238};
        else if (channelTones == 996) flat = {-494,-480,-468,-454, -440,-426,-414,-400,
                -360,-346,-334,-320, -306,-292,-280,-266, -252,-238,-226,-212,
                -198,-184,-172,-158, -118,-104,-92,-78, -64,-50,-38,-24,
                24,38,50,64, 78,92,104,118, 158,172,184,198, 212,226,238,252,
                266,280,292,306, 320,334,346,360, 400,414,426,440, 454,468,480,494};
    }
    else if (toneSize == 106) {
        if (channelTones == 242) flat = {-116,-90,-48,-22, 22,48,90,116};
        else if (channelTones == 484) flat = {-238,-212,-170,-144, -104,-78,-36,-10,
                10,36,78,104, 144,170,212,238};
        else if (channelTones == 996) flat = {-494,-468,-426,-400, -360,-334,-292,-266,
                -252,-226,-184,-158, -118,-92,-50,-24, 24,50,92,118,
                158,184,226,252, 266,292,334,360, 400,426,468,494};
    }
    else if (toneSize == 242) {
        if (channelTones == 242) flat = {-116,-90,-48,-22,22,48,90,116};
        else if (channelTones == 484) flat = {-238,-212,-170,-144,-104,-78,-36,-10,
                10,36,78,104,144,170,212,238};
        else if (channelTones == 996) flat = {-494,-468,-426,-400,-360,-334,-292,-266,
                -252,-226,-184,-158,-118,-92,-50,-24, 24,50,92,118,158,184,226,252,
                266,292,334,360,400,426,468,494};
    }
    else if (toneSize == 484) {
        if (channelTones == 484) flat = {-238,-212,-170,-144,-104,-78,-36,-10,
                10,36,78,104,144,170,212,238};
        else if (channelTones == 996) flat = {-494,-468,-426,-400,-360,-334,-292,-266,
                -252,-226,-184,-158,-118,-92,-50,-24, 24,50,92,118,158,184,226,252,
                266,292,334,360,400,426,468,494};
    }
    else if (toneSize == 996 && channelTones == 996)
        flat = {-468,-400,-334,-266,-226,-158,-92,-24,24,92,158,226,266,334,400,468};
    if (flat.empty() || flat.size() % groupSize != 0)
        throw std::invalid_argument("Unsupported HE pilot table combination");
    std::vector<std::vector<int>> groups;
    for (size_t i = 0; i < flat.size(); i += groupSize)
        groups.emplace_back(flat.begin() + i, flat.begin() + i + groupSize);
    return groups;
}

} // namespace

std::vector<int> getHeRuDataToneIndices(int channelTones, int toneSize, int toneOffset)
{
    Hz bandwidth;
    switch (channelTones) {
        case 242: bandwidth = MHz(20); break;
        case 484: bandwidth = MHz(40); break;
        case 996: bandwidth = MHz(80); break;
        case 1992: bandwidth = MHz(160); break;
        default: throw std::invalid_argument("Unsupported HE channel tone count");
    }
    auto catalog = getHeRuAllocationCatalog(Hz(0), bandwidth);
    std::vector<int> offsets;
    for (const auto& ru : catalog)
        if (ru.toneSize == toneSize)
            offsets.push_back(ru.toneOffset);
    std::sort(offsets.begin(), offsets.end());
    offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());
    auto offsetIt = std::find(offsets.begin(), offsets.end(), toneOffset);
    if (offsetIt == offsets.end())
        throw std::invalid_argument("Noncanonical HE RU tone offset");
    auto ranges = getHeRuRanges(channelTones, toneSize);
    size_t index = offsetIt - offsets.begin();
    if (index >= ranges.size())
        throw std::logic_error("HE RU catalog does not match the standard subcarrier table");
    auto pilots = getHeRuPilotToneIndices(channelTones, toneSize, toneOffset);
    std::set<int> pilotSet(pilots.begin(), pilots.end());
    std::vector<int> result;
    for (auto range : ranges[index])
        for (int tone = range.first; tone <= range.second; ++tone)
            if (pilotSet.find(tone) == pilotSet.end())
                result.push_back(tone);
    if ((int)result.size() != getHeRuDataSubcarrierCount(toneSize))
        throw std::logic_error("HE RU data-tone table has an unexpected size");
    return result;
}

std::vector<int> getHeRuPilotToneIndices(int channelTones, int toneSize, int toneOffset)
{
    Hz bandwidth;
    switch (channelTones) {
        case 242: bandwidth = MHz(20); break;
        case 484: bandwidth = MHz(40); break;
        case 996: bandwidth = MHz(80); break;
        case 1992: bandwidth = MHz(160); break;
        default: throw std::invalid_argument("Unsupported HE channel tone count");
    }
    auto catalog = getHeRuAllocationCatalog(Hz(0), bandwidth);
    std::vector<int> offsets;
    for (const auto& ru : catalog)
        if (ru.toneSize == toneSize)
            offsets.push_back(ru.toneOffset);
    std::sort(offsets.begin(), offsets.end());
    offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());
    auto offsetIt = std::find(offsets.begin(), offsets.end(), toneOffset);
    if (offsetIt == offsets.end())
        throw std::invalid_argument("Noncanonical HE RU tone offset");
    auto groups = getPilotGroups(channelTones, toneSize);
    size_t index = offsetIt - offsets.begin();
    if (index >= groups.size())
        throw std::logic_error("HE RU catalog does not match the standard pilot table");
    return groups[index];
}

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
