//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211RATESELECTIONUTILS_H
#define __INET_IEEE80211RATESELECTIONUTILS_H

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

inline int getMaximumModeBandwidthMHz(physicallayer::Ieee80211ChannelWidth width)
{
    switch (width) {
        case physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ: return 20;
        case physicallayer::IEEE80211_CHANNEL_WIDTH_40MHZ: return 40;
        case physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ: return 80;
        case physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ:
        case physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ: return 160;
        default: throw cRuntimeError("Unknown IEEE 802.11 maximum channel width: %d", width);
    }
}

/**
 * Applies a TXOP-local width constraint using the configured rate-selection
 * mode set.  The requested mode is retained whenever it is legal; fallback
 * selection then prefers the widest legal mode and uses bitrate only as a
 * deterministic tie breaker.  Channel-access policy deliberately does not
 * select a mode or mutate the queued ModeReq tag.
 */
inline const physicallayer::IIeee80211Mode *selectIeee80211ModeForWidth(
        const physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *requestedMode,
        physicallayer::Ieee80211ChannelWidth maximumWidth)
{
    if (modeSet == nullptr || requestedMode == nullptr)
        throw cRuntimeError("IEEE 802.11 rate selection requires a mode set and requested mode");
    const int maximumWidthMHz = getMaximumModeBandwidthMHz(maximumWidth);
    const auto requestedBandwidth = requestedMode->getDataMode()->getBandwidth();
    const int requestedWidthMHz = requestedBandwidth <= MHz(20) ? 20 :
            requestedBandwidth <= MHz(40) ? 40 : requestedBandwidth <= MHz(80) ? 80 : 160;
    if (requestedWidthMHz <= maximumWidthMHz)
        return requestedMode;

    const bps requestedBitrate = requestedMode->getDataMode()->getNetBitrate();
    const physicallayer::IIeee80211Mode *best = nullptr;
    int bestWidthMHz = -1;
    bps bestBitrate = bps(-1);
    for (int index = 0; index < modeSet->getNumModes(); ++index) {
        const auto *candidate = modeSet->getMode(index);
        const auto candidateBandwidth = candidate->getDataMode()->getBandwidth();
        const int candidateWidthMHz = candidateBandwidth <= MHz(20) ? 20 :
                candidateBandwidth <= MHz(40) ? 40 : candidateBandwidth <= MHz(80) ? 80 : 160;
        if (candidateWidthMHz > maximumWidthMHz)
            continue;
        const auto candidateBitrate = candidate->getDataMode()->getNetBitrate();
        bool candidateIsBetter = candidateWidthMHz > bestWidthMHz;
        if (!candidateIsBetter && candidateWidthMHz == bestWidthMHz) {
            const bool candidateAtOrBelowRequestedRate = candidateBitrate <= requestedBitrate;
            const bool bestAtOrBelowRequestedRate = bestBitrate <= requestedBitrate;
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
    if (best == nullptr)
        throw cRuntimeError("No IEEE 802.11 mode is available for the selected channel width");
    return best;
}

} // namespace ieee80211
} // namespace inet

#endif
