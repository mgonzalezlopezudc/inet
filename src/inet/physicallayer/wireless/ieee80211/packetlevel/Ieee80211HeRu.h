//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HERU_H
#define __INET_IEEE80211HERU_H

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include <vector>

namespace inet {

using namespace inet::units::values;

namespace physicallayer {

struct Ieee80211HeRu {
    int index;
    Hz centerFrequency;
    Hz bandwidth;
};

inline std::vector<Ieee80211HeRu> calculateHeRus(Hz centerFrequency, Hz bandwidth, int numRUs)
{
    std::vector<Ieee80211HeRu> rus;
    if (numRUs <= 0)
        return rus;
    Hz ruBandwidth = bandwidth / numRUs;
    for (int i = 0; i < numRUs; ++i) {
        Hz ruCenterFrequency = centerFrequency - bandwidth / 2.0 + ruBandwidth * (i + 0.5);
        rus.push_back({i, ruCenterFrequency, ruBandwidth});
    }
    return rus;
}

} // namespace physicallayer
} // namespace inet

#endif // __INET_IEEE80211HERU_H
