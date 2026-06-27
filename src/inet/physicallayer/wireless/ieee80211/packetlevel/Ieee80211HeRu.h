//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HERU_H
#define __INET_IEEE80211HERU_H

#include <algorithm>
#include <cmath>
#include <ostream>
#include <set>
#include <stdexcept>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"

namespace inet {

using namespace inet::units::values;

namespace physicallayer {

/**
 * IEEE 802.11ax resource unit (RU) description.
 *
 * IEEE 802.11-2024, Clause 27.3.2.2 ("Resource unit, guard, and DC subcarriers").
 * 802.11ax HE (High Efficiency) introduces narrower subcarrier tone spacing of 78.125 kHz,
 * which is exactly 1/4 of the 312.5 kHz spacing used in 802.11a/g/n/ac. This increases
 * the number of subcarrier tones by a factor of 4 for a given channel bandwidth, enabling
 * fine-grained multi-user orthogonal frequency-division multiple access (OFDMA).
 *
 * The allocationIndex is local to the selected RU layout. The toneOffset is
 * measured from the first occupied HE tone of the channel and makes the
 * frequency placement independent of the number of scheduled users.
 *
 * Implementation note: the RU allocation tree models the standard RU splits
 * from Figures 27-5..27-8.  The small fixed gaps (e.g. the central 26-tone DC/
 * guard between two 484-tone RUs) are hard-coded; this is faithful to the
 * standard layout but does not attempt to model every possible puncturing or
 * partial-bandwidth configuration.
 */
struct Ieee80211HeRu {
    int index = -1;
    int toneSize = 0;              // Total tone size (26, 52, 106, 242, 484, 996, 1992)
    int toneOffset = 0;            // Tone offset relative to the channel's starting subcarrier
    int dataSubcarriers = 0;       // Number of data subcarriers in this RU (N_SD)
    int pilotSubcarriers = 0;      // Number of pilot subcarriers in this RU (N_SP)
    Hz centerFrequency = Hz(NaN);
    Hz bandwidth = Hz(NaN);

    bool operator==(const Ieee80211HeRu& other) const
    {
        return index == other.index && toneSize == other.toneSize &&
                toneOffset == other.toneOffset && centerFrequency == other.centerFrequency &&
                bandwidth == other.bandwidth;
    }
};

inline std::ostream& operator<<(std::ostream& os, const Ieee80211HeRu& ru)
{
    os << "idx=" << ru.index
       << " tones=" << ru.toneSize
       << " offset=" << ru.toneOffset
       << " data=" << ru.dataSubcarriers
       << " pilots=" << ru.pilotSubcarriers
       << " bw=" << ru.bandwidth;
    return os;
}

/**
 * Returns the data-subcarrier count (N_SD) for a standard HE RU tone size.
 *
 * IEEE 802.11-2024, Clause 27.3.2.2.
 * - 26-tone RU: 24 data subcarriers (24 data + 2 pilot = 26 tones)
 * - 52-tone RU: 48 data subcarriers (48 data + 4 pilot = 52 tones)
 * - 106-tone RU: 102 data subcarriers (102 data + 4 pilot = 106 tones)
 * - 242-tone RU: 234 data subcarriers (234 data + 8 pilot = 242 tones, standard 20 MHz channel)
 * - 484-tone RU: 468 data subcarriers (468 data + 16 pilot = 484 tones, standard 40 MHz channel)
 * - 996-tone RU: 980 data subcarriers (980 data + 16 pilot = 996 tones, standard 80 MHz channel)
 * - 1992-tone RU: 1960 data subcarriers (1960 data + 32 pilot = 1992 tones, standard 160 MHz channel)
 */
int getHeRuDataSubcarrierCount(int toneSize);

/**
 * Returns the pilot-subcarrier count (N_SP) for a standard HE RU tone size.
 *
 * IEEE 802.11-2024, Clause 27.3.2.4 ("Pilot subcarriers").
 * The pilots are used to estimate residual frequency offset and phase noise tracking during
 * reception of the HE payload.
 */
int getHeRuPilotSubcarrierCount(int toneSize);

/**
 * Returns the total tone size for the given channel bandwidth.
 * IEEE 802.11-2024, Clause 27.3.2.2, where:
 * - 20 MHz bandwidth maps to a 242-tone RU.
 * - 40 MHz bandwidth maps to a 484-tone RU.
 * - 80 MHz bandwidth maps to a 996-tone RU.
 * - 160 MHz (or 80+80 MHz) bandwidth maps to a 1992-tone RU.
 */
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

/** Returns the maximum number of 26-tone RUs that can be fitted in the given bandwidth. */
inline int getHeMaxRuCount(Hz bandwidth)
{
    switch (getHeChannelToneCount(bandwidth)) {
        case 242: return 9;     // 9x 26-tone RUs + 26 guard/DC tones
        case 484: return 18;    // 18x 26-tone RUs + guard/DC tones
        case 996: return 37;    // 37x 26-tone RUs + guard/DC tones
        case 1992: return 74;   // 74x 26-tone RUs + guard/DC tones
        default: return 0;
    }
}

/** Returns supported configurations of equal-sized RU allocations for the channel bandwidth. */
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

/**
 * Returns the RU tone size corresponding to the division of the channel into an equal-sized RU count.
 * E.g., dividing 20 MHz (242 tones) into 9 RUs yields 26-tone RUs.
 */
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

/** Instantiates an RU structure, mapping physical center frequency and tone offset based on the HE subcarrier spacing. */
inline Ieee80211HeRu makeHeRu(Hz centerFrequency, int channelTones,
        int index, int toneSize, int toneOffset)
{
    constexpr double HE_TONE_SPACING = 78125; // 78.125 kHz spacing (IEEE 802.11-2024, Clause 27.3.2.2)
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
 * IEEE 802.11-2024, Figure 27-5 through Figure 27-8 (RU allocation maps).
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

/**
 * Builds the canonical nested RU allocation catalog for a channel. The index
 * is stable within this catalog and is therefore usable in HE-SIG-B encoding.
 */
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

/** Checks that a layout contains only standard, non-overlapping RUs in-band. */
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
 * Includes subchannel puncturing masks mapping to punctured preamble subchannels (preamble puncturing, Clause 27.3.2.6).
 */
inline bool allocateHeRus(Hz centerFrequency, Hz channelBandwidth,
        const std::vector<int>& requestedToneSizes, std::vector<Ieee80211HeRu>& allocations,
        const std::vector<bool>& puncturedSubchannels = {})
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
