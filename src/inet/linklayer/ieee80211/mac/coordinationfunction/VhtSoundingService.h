//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTSOUNDINGSERVICE_H
#define __INET_VHTSOUNDINGSERVICE_H

#include "inet/linklayer/ieee80211/mac/contract/IVhtSoundingCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtCsiCache.h"

namespace inet {
namespace ieee80211 {

/** Singular owner of VHT sounding dialog, CSI, width, and cooldown state. */
class INET_API VhtSoundingService
{
  private:
    uint8_t nextDialogToken = 1;
    Hz effectiveChannelWidth = Hz(0);
    VhtCsiCache csiCache;
    IVhtSoundingCoordinator *coordinator = nullptr;

  public:
    static void updateEffectiveChannelWidth(VhtCsiCache& cache,
            Hz& previousWidth, Hz currentWidth)
    {
        if (previousWidth != Hz(0) && currentWidth != previousWidth)
            cache.clear();
        previousWidth = currentWidth;
    }

    void configure(simtime_t csiValidityDuration,
            IVhtSoundingCoordinator *soundingCoordinator)
    {
        if (soundingCoordinator == nullptr)
            throw cRuntimeError("VHT sounding service requires a coordinator");
        csiCache.configure(csiValidityDuration);
        coordinator = soundingCoordinator;
    }

    bool updateChannelWidth(Hz channelWidth)
    {
        bool changed = effectiveChannelWidth != Hz(0) &&
                channelWidth != effectiveChannelWidth;
        updateEffectiveChannelWidth(csiCache, effectiveChannelWidth, channelWidth);
        return changed;
    }

    uint8_t reserveDialogToken() { return nextDialogToken++ & 0x3f; }
    uint8_t commitDialogToken(uint8_t expectedToken)
    {
        if ((nextDialogToken & 0x3f) != expectedToken)
            throw cRuntimeError("Prepared VHT sounding dialog token became stale");
        return reserveDialogToken();
    }
    void rollbackDialogToken(uint8_t token) noexcept
    {
        if (nextDialogToken > 1 && ((nextDialogToken - 1) & 0x3f) == token)
            --nextDialogToken;
    }
    uint8_t getNextDialogToken() const { return nextDialogToken; }
    Hz getEffectiveChannelWidth() const { return effectiveChannelWidth; }
    VhtCsiCache& getCsiCache() { return csiCache; }
    const VhtCsiCache& getCsiCache() const { return csiCache; }
    IVhtSoundingCoordinator& getCoordinator() const
        { ASSERT(coordinator != nullptr); return *coordinator; }

    void invalidatePeer(const MacAddress& peer)
    {
        csiCache.invalidatePeer(peer);
        getCoordinator().invalidatePeer(peer);
    }

    void reset()
    {
        csiCache.clear();
        getCoordinator().reset();
    }

    void clearCsi() { csiCache.clear(); }
};

} // namespace ieee80211
} // namespace inet

#endif
