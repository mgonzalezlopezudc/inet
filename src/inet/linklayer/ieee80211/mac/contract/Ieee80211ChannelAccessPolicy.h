//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211CHANNELACCESSPOLICY_H
#define __INET_IEEE80211CHANNELACCESSPOLICY_H

#include <cstring>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/IRx.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

/**
 * Selects what the MAC does when the secondary part of a requested PPDU is
 * unavailable at the end of contention.  Dynamic selection follows the
 * primary-channel hierarchy, while static selection restarts the current CW.
 */
enum class Ieee80211ChannelWidthSelectionPolicy {
    DYNAMIC,
    STATIC,
};

inline Ieee80211ChannelWidthSelectionPolicy parseIeee80211ChannelWidthSelectionPolicy(const char *value)
{
    if (strcmp(value, "dynamic") == 0)
        return Ieee80211ChannelWidthSelectionPolicy::DYNAMIC;
    if (strcmp(value, "static") == 0)
        return Ieee80211ChannelWidthSelectionPolicy::STATIC;
    throw cRuntimeError("Unknown channelWidthSelectionPolicy '%s', expected 'dynamic' or 'static'", value);
}

inline physicallayer::Ieee80211ChannelWidth getIeee80211ChannelWidth(const physicallayer::IIeee80211Mode *mode)
{
    if (mode == nullptr)
        throw cRuntimeError("Cannot determine the channel width of a null IEEE 802.11 mode");
    auto bandwidth = mode->getDataMode()->getBandwidth();
    if (bandwidth <= MHz(20))
        return physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ;
    if (bandwidth <= MHz(40))
        return physicallayer::IEEE80211_CHANNEL_WIDTH_40MHZ;
    if (bandwidth <= MHz(80))
        return physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ;
    return physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ;
}

inline int getIeee80211ChannelWidthMHz(physicallayer::Ieee80211ChannelWidth width)
{
    switch (width) {
        case physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ: return 20;
        case physicallayer::IEEE80211_CHANNEL_WIDTH_40MHZ: return 40;
        case physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ: return 80;
        case physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ:
        case physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ: return 160;
        default: throw cRuntimeError("Unknown IEEE 802.11 channel width: %d", width);
    }
}

inline const physicallayer::IIeee80211Mode *findWidestIeee80211Mode(const physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *requestedMode, physicallayer::Ieee80211ChannelWidth maximumWidth)
{
    if (modeSet == nullptr || requestedMode == nullptr)
        return nullptr;
    const auto requestedBitrate = requestedMode->getDataMode()->getNetBitrate();
    const auto maximumWidthMHz = getIeee80211ChannelWidthMHz(maximumWidth);
    const physicallayer::IIeee80211Mode *best = nullptr;
    int bestWidthMHz = -1;
    bps bestBitrate = bps(-1);
    for (int i = 0; i < modeSet->getNumModes(); i++) {
        auto candidate = modeSet->getMode(i);
        auto candidateWidth = getIeee80211ChannelWidth(candidate);
        int candidateWidthMHz = getIeee80211ChannelWidthMHz(candidateWidth);
        if (candidateWidthMHz > maximumWidthMHz)
            continue;
        auto candidateBitrate = candidate->getDataMode()->getNetBitrate();
        bool candidateIsBetter = candidateWidthMHz > bestWidthMHz;
        if (!candidateIsBetter && candidateWidthMHz == bestWidthMHz) {
            bool candidateAtOrBelowRequestedRate = candidateBitrate <= requestedBitrate;
            bool bestAtOrBelowRequestedRate = bestBitrate <= requestedBitrate;
            if (candidateAtOrBelowRequestedRate != bestAtOrBelowRequestedRate)
                candidateIsBetter = candidateAtOrBelowRequestedRate;
            else if (candidateAtOrBelowRequestedRate)
                candidateIsBetter = candidateBitrate > bestBitrate;
            else
                candidateIsBetter = candidateBitrate < bestBitrate;
        }
        if (candidateIsBetter) {
            best = candidate;
            bestWidthMHz = candidateWidthMHz;
            bestBitrate = candidateBitrate;
        }
    }
    return best;
}

struct Ieee80211ChannelAccessSelection {
    const physicallayer::IIeee80211Mode *mode = nullptr;
    physicallayer::Ieee80211ChannelWidth channelWidth = physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ;
    bool restart = false;
};

inline Ieee80211ChannelAccessSelection selectIeee80211ChannelAccess(const physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *requestedMode, const IRx *rx, simtime_t requiredIdle,
        Ieee80211ChannelWidthSelectionPolicy policy)
{
    if (requestedMode == nullptr)
        return {nullptr, physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ, true};
    auto requestedWidth = getIeee80211ChannelWidth(requestedMode);
    if (rx == nullptr || rx->isChannelIdleForTransmission(requestedWidth, requiredIdle))
        return {requestedMode, requestedWidth, false};
    if (policy == Ieee80211ChannelWidthSelectionPolicy::STATIC)
        return {nullptr, requestedWidth, true};

    const physicallayer::Ieee80211ChannelWidth fallbackWidths[] = {
        physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ,
        physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ,
        physicallayer::IEEE80211_CHANNEL_WIDTH_40MHZ,
        physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ,
    };
    int requestedWidthMHz = getIeee80211ChannelWidthMHz(requestedWidth);
    for (auto fallbackWidth : fallbackWidths) {
        if (getIeee80211ChannelWidthMHz(fallbackWidth) > requestedWidthMHz)
            continue;
        if (!rx->isChannelIdleForTransmission(fallbackWidth, requiredIdle))
            continue;
        auto fallbackMode = findWidestIeee80211Mode(modeSet, requestedMode, fallbackWidth);
        if (fallbackMode != nullptr)
            return {fallbackMode, fallbackWidth, false};
    }
    return {nullptr, physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ, true};
}

inline simtime_t getIeee80211SecondaryChannelIdleInterval(const physicallayer::Ieee80211ModeSet *modeSet)
{
    if (modeSet == nullptr)
        return SIMTIME_ZERO;
    // IEEE Std 802.11-2024, 11.15.9 item b): DIFS in 2.4 GHz and PIFS in 5 GHz.
    bool is24GHz = strstr(modeSet->getName(), "2.4Ghz") != nullptr;
    return modeSet->getSifsTime() + (is24GHz ? 2 : 1) * modeSet->getSlotTime();
}

} // namespace ieee80211
} // namespace inet

#endif
