//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEPREAMBLEPUNCTURING_H
#define __INET_IEEE80211HEPREAMBLEPUNCTURING_H

#include <algorithm>
#include <vector>

namespace inet {
namespace ieee80211 {

/** Returns whether the punctured 20 MHz subchannels form a legal HE pattern. */
inline bool isValidHePreamblePuncturing(const std::vector<bool>& mask, int widthMhz)
{
    if (mask.empty())
        return true;
    if ((widthMhz != 80 && widthMhz != 160) ||
            mask.size() != static_cast<size_t>(widthMhz / 20))
        return false;

    // The primary 20 MHz subchannel must remain active.
    if (mask[0])
        return false;
    if (std::all_of(mask.begin(), mask.end(), [](bool punctured) { return punctured; }))
        return false;

    if (widthMhz == 80) {
        int puncturedCount = std::count(mask.begin(), mask.end(), true);
        return puncturedCount == 0 || (puncturedCount == 1 && (mask[1] || mask[2] || mask[3]));
    }

    int secondary80Count = std::count(mask.begin() + 4, mask.end(), true);
    if (secondary80Count > 2)
        return false;
    if (secondary80Count == 2 && !(mask[4] && mask[5]) && !(mask[6] && mask[7]))
        return false;
    for (size_t i = 0; i + 2 < mask.size(); ++i)
        if (mask[i] && mask[i + 1] && mask[i + 2])
            return false;

    bool bandwidthValue6 = mask[1] && !mask[2] && !mask[3];
    bool bandwidthValue7 = !mask[1] && std::count(mask.begin(), mask.end(), true) >= 1;
    return bandwidthValue6 || bandwidthValue7;
}

} // namespace ieee80211
} // namespace inet

#endif // __INET_IEEE80211HEPREAMBLEPUNCTURING_H
