//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EHTMRU_H
#define __INET_IEEE80211EHTMRU_H

#include <cmath>
#include <ostream>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {

using namespace inet::units::values;

namespace physicallayer {

/**
 * IEEE 802.11be Extremely High Throughput (EHT) MRU description.
 *
 * Extending the HE RU tree to support 320 MHz and MRUs.
 */
struct Ieee80211EhtMru {
    int index = -1;
    int toneSize = 0;
    int toneOffset = 0;
    int dataSubcarriers = 0;
    int pilotSubcarriers = 0;
    Hz centerFrequency = Hz(NaN);
    Hz bandwidth = Hz(NaN);

    bool operator==(const Ieee80211EhtMru& other) const
    {
        return index == other.index && toneSize == other.toneSize &&
                toneOffset == other.toneOffset && centerFrequency == other.centerFrequency &&
                bandwidth == other.bandwidth;
    }
};

inline std::ostream& operator<<(std::ostream& os, const Ieee80211EhtMru& ru)
{
    os << "idx=" << ru.index
       << " tones=" << ru.toneSize
       << " offset=" << ru.toneOffset
       << " data=" << ru.dataSubcarriers
       << " pilots=" << ru.pilotSubcarriers
       << " bw=" << ru.bandwidth;
    return os;
}

int getEhtMruDataSubcarrierCount(int toneSize);
int getEhtMruPilotSubcarrierCount(int toneSize);
int getEhtChannelToneCount(Hz bandwidth);

} // namespace physicallayer
} // namespace inet

#endif
