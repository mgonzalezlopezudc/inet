//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEPREAMBLEPUNCTURING_H
#define __INET_HEPREAMBLEPUNCTURING_H

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

inline bool isValidHePreamblePuncturing(const std::vector<bool>& mask, int widthMhz)
{
    if (mask.empty())
        return true;
    if (widthMhz != 80 && widthMhz != 160)
        return false;

    // 1. Primary 20 MHz subchannel must not be punctured (mask[0] represents primary 20 MHz).
    if (mask[0])
        return false;

    // 2. At least one subchannel must remain active.
    if (std::all_of(mask.begin(), mask.end(), [](bool b) { return b; }))
        return false;

    if (widthMhz == 80) {
        // Allowed 80 MHz patterns (Table 27-21, Bandwidth values 4 and 5):
        // - Value 4: Secondary 20 MHz punctured (mask: 0100)
        // - Value 5: One of two 20 MHz subchannels in secondary 40 MHz channel punctured (mask: 0010 or 0001)
        int count = 0;
        for (bool b : mask) if (b) count++;
        if (count == 0) return true;
        if (count == 1) {
            return mask[1] || mask[2] || mask[3];
        }
        return false;
    }
    else { // 160 MHz (8 subchannels: b0, b1, b2, b3, b4, b5, b6, b7)
        // b0: Primary 20 (always active / 0)
        // b1: Secondary 20
        // b2, b3: Secondary 40
        // b4, b5, b6, b7: Secondary 80

        // Table 27-21, Bandwidth values 6 and 7:
        // "If two of the 20 MHz subchannels in the secondary 80 MHz channel are punctured,
        // these are either the lower two or the higher two."
        int sec80Count = (mask[4]?1:0) + (mask[5]?1:0) + (mask[6]?1:0) + (mask[7]?1:0);
        if (sec80Count > 2)
            return false;
        if (sec80Count == 2) {
            bool lowerTwo = mask[4] && mask[5];
            bool higherTwo = mask[6] && mask[7];
            if (!lowerTwo && !higherTwo)
                return false;
        }

        // "No more than two adjacent 20 MHz subchannels are punctured across 160 MHz."
        for (size_t i = 0; i + 2 < mask.size(); ++i) {
            if (mask[i] && mask[i+1] && mask[i+2])
                return false;
        }

        // - Value 6: Secondary 20 MHz is punctured (b1 = 1), secondary 40 MHz is not punctured (b2 = 0, b3 = 0).
        bool isSet6 = mask[1] && !mask[2] && !mask[3];

        // - Value 7: Secondary 20 MHz is not punctured (b1 = 0), and at least one 20 MHz subchannel is punctured.
        int totalCount = 0;
        for (bool b : mask) if (b) totalCount++;
        bool isSet7 = !mask[1] && (totalCount >= 1);

        if (!isSet6 && !isSet7)
            return false;

        return true;
    }
}

inline std::vector<bool> parseHePreamblePuncturing(const char *value, inet::units::values::Hz bandwidth)
{
    std::string mask(value == nullptr ? "" : value);
    if (mask.empty())
        return {};
    int widthMhz = std::lround(bandwidth.get() / 1e6);
    if (widthMhz != 80 && widthMhz != 160)
        throw omnetpp::cRuntimeError("HE preamble puncturing is supported only for 80 and 160 MHz channels");
    int expectedBits = widthMhz / 20;
    if ((int)mask.size() != expectedBits)
        throw omnetpp::cRuntimeError("HE preamble puncturing mask must contain %d bits", expectedBits);
    std::vector<bool> result;
    for (char bit : mask) {
        if (bit != '0' && bit != '1')
            throw omnetpp::cRuntimeError("HE preamble puncturing mask must contain only 0 and 1");
        result.push_back(bit == '1');
    }
    if (result.front())
        throw omnetpp::cRuntimeError("The primary 20 MHz HE subchannel must not be punctured");
    if (std::all_of(result.begin(), result.end(), [] (bool val) { return val; }))
        throw omnetpp::cRuntimeError("At least one HE 20 MHz subchannel must remain active");
    if (!isValidHePreamblePuncturing(result, widthMhz))
        throw omnetpp::cRuntimeError("HE preamble puncturing mask '%s' is not a permitted standard pattern for %d MHz channel", value, widthMhz);
    return result;
}

inline bool overlapsHePuncturedSubchannel(const inet::physicallayer::Ieee80211HeRu& ru,
        const std::vector<bool>& puncturedSubchannels, inet::units::values::Hz bandwidth)
{
    if (puncturedSubchannels.empty())
        return false;
    int channelTones = inet::physicallayer::getHeChannelToneCount(bandwidth);
    int first = ru.toneOffset * puncturedSubchannels.size() / channelTones;
    int last = (ru.toneOffset + ru.toneSize - 1) * puncturedSubchannels.size() / channelTones;
    for (int subchannel = first; subchannel <= last; ++subchannel)
        if (puncturedSubchannels.at(subchannel))
            return true;
    return false;
}

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEPREAMBLEPUNCTURING_H
