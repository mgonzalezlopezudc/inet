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
#include "inet/linklayer/ieee80211/mac/contract/Ieee80211HePreamblePuncturing.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

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

inline std::vector<bool> resolveHePreamblePuncturing(const omnetpp::cComponent *component,
        inet::units::values::Hz bandwidth)
{
    auto dynamicMask = component->par("dynamicHePreamblePuncturing").stringValue();
    auto start = omnetpp::SimTime(component->par("dynamicHePreamblePuncturingStart"));
    auto end = omnetpp::SimTime(component->par("dynamicHePreamblePuncturingEnd"));
    bool insideDynamicInterval = dynamicMask[0] != '\0' && start >= omnetpp::SimTime::ZERO &&
            omnetpp::simTime() >= start && (end < omnetpp::SimTime::ZERO || omnetpp::simTime() < end);
    return parseHePreamblePuncturing(insideDynamicInterval ? dynamicMask :
            component->par("hePreamblePuncturing").stringValue(), bandwidth);
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
