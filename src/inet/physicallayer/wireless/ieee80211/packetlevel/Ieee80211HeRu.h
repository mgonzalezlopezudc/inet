//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HERU_H
#define __INET_IEEE80211HERU_H

#include <cmath>
#include <ostream>
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
 * Returns the signed physical data-subcarrier indices for a canonical HE RU.
 * The indices are relative to the channel DC tone and exclude pilots, DC, and
 * guard/null tones (IEEE 802.11-2024 Tables 27-8 through 27-10 and 27-38,
 * 27-40, 27-42, 27-43, 27-45, and 27-46). For 160 MHz, the 80 MHz mappings
 * are replicated with -512/+512 shifts as specified by the pilot tables.
 */
std::vector<int> getHeRuDataToneIndices(int channelTones, int toneSize, int toneOffset);

/** Returns the corresponding table-defined HE pilot-subcarrier indices. */
std::vector<int> getHeRuPilotToneIndices(int channelTones, int toneSize, int toneOffset);

/**
 * Returns the total tone size for the given channel bandwidth.
 * IEEE 802.11-2024, Clause 27.3.2.2, where:
 * - 20 MHz bandwidth maps to a 242-tone RU.
 * - 40 MHz bandwidth maps to a 484-tone RU.
 * - 80 MHz bandwidth maps to a 996-tone RU.
 * - 160 MHz (or 80+80 MHz) bandwidth maps to a 1992-tone RU.
 */
int getHeChannelToneCount(Hz bandwidth);

/** Returns the maximum number of 26-tone RUs that can be fitted in the given bandwidth. */
int getHeMaxRuCount(Hz bandwidth);

/** Returns supported configurations of equal-sized RU allocations for the channel bandwidth. */
std::vector<int> getHeEqualRuCounts(Hz bandwidth);

/**
 * Returns the RU tone size corresponding to the division of the channel into an equal-sized RU count.
 * E.g., dividing 20 MHz (242 tones) into 9 RUs yields 26-tone RUs.
 */
int getHeEqualRuToneSize(Hz bandwidth, int count);

/** Instantiates an RU structure, mapping physical center frequency and tone offset based on the HE subcarrier spacing. */
Ieee80211HeRu makeHeRu(Hz centerFrequency, int channelTones,
        int index, int toneSize, int toneOffset);

/**
 * Builds the canonical nested RU allocation catalog for a channel. The index
 * is stable within this catalog and is therefore usable in HE-SIG-B encoding.
 */
std::vector<Ieee80211HeRu> getHeRuAllocationCatalog(Hz centerFrequency, Hz channelBandwidth);

/**
 * Returns the standard equal-sized layout as one level of the allocation tree.
 */
std::vector<Ieee80211HeRu> getHeEqualRuLayout(Hz centerFrequency, Hz channelBandwidth, int count);

/** Checks that a layout contains only standard, non-overlapping RUs in-band. */
bool validateHeRuLayout(const std::vector<Ieee80211HeRu>& layout, Hz channelBandwidth);

/**
 * Allocates exact requested RU sizes at canonical, non-overlapping positions.
 * Requests should be ordered from largest to smallest for deterministic fitting.
 * Includes subchannel puncturing masks mapping to punctured preamble subchannels (preamble puncturing, Clause 27.3.2.6).
 */
bool allocateHeRus(Hz centerFrequency, Hz channelBandwidth,
        const std::vector<int>& requestedToneSizes, std::vector<Ieee80211HeRu>& allocations,
        const std::vector<bool>& puncturedSubchannels = {});

// Compatibility entry point retained for existing callers. Unlike the old
// approximation, it accepts only standard equal-sized RU counts.
std::vector<Ieee80211HeRu> calculateHeRus(Hz centerFrequency, Hz bandwidth, int numRUs);

} // namespace physicallayer
} // namespace inet

#endif
