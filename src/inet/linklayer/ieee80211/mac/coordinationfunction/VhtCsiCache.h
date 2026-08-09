//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTCSICACHE_H
#define __INET_VHTCSICACHE_H

#include <functional>
#include <cmath>
#include <iterator>
#include <map>
#include <utility>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * Owns locally modeled VHT channel-state freshness.
 *
 * The standard defines how explicit feedback is obtained; the validity
 * interval is deliberately a simulation policy. Association generation and
 * channel width are part of the key so CSI cannot survive reassociation or a
 * bandwidth change accidentally.
 */
class INET_API VhtCsiCache
{
  public:
    struct Entry {
        simtime_t acquisitionTime = SIMTIME_ZERO;
        simtime_t expiryTime = SIMTIME_ZERO;
        double beamformingGainDb = 0;
        int soundingNsts = 2;
        bool feedbackTypeMu = false;
        uint8_t nc = 1;
        uint8_t nr = 2;
    };

  protected:
    struct Key {
        MacAddress peer;
        Hz channelWidth = Hz(0);
        uint64_t associationGeneration = 0;

        bool operator<(const Key& other) const
        {
            if (peer != other.peer)
                return peer < other.peer;
            if (channelWidth != other.channelWidth)
                return channelWidth < other.channelWidth;
            return associationGeneration < other.associationGeneration;
        }
    };

    simtime_t validityDuration = SimTime(0.1);
    std::map<Key, Entry> entries;
    std::function<simtime_t()> timeProvider = []() { return simTime(); };

  public:
    void configure(simtime_t validityDuration)
    {
        if (validityDuration < SIMTIME_ZERO)
            throw cRuntimeError("VHT CSI validity duration must not be negative");
        this->validityDuration = validityDuration;
    }

    void setTimeProvider(std::function<simtime_t()> provider)
    {
        if (!provider)
            throw cRuntimeError("VHT CSI time provider must be defined");
        timeProvider = std::move(provider);
    }

    void update(const MacAddress& peer, Hz channelWidth,
            uint64_t associationGeneration, double beamformingGainDb,
            int soundingNsts = 2, bool feedbackTypeMu = false,
            uint8_t nc = 1, uint8_t nr = 2)
    {
        if (peer.isUnspecified() || channelWidth <= Hz(0) ||
                associationGeneration == 0 || !std::isfinite(beamformingGainDb) ||
                beamformingGainDb < 0 || soundingNsts < 2 || soundingNsts > 8 ||
                nc < 1 || nc > 8 || nr < 2 || nr > 8 || nc > nr)
            throw cRuntimeError("Invalid VHT CSI cache entry");
        auto now = timeProvider();
        entries[{peer, channelWidth, associationGeneration}] =
                {now, now + validityDuration, beamformingGainDb, soundingNsts,
                 feedbackTypeMu, nc, nr};
    }

    const Entry *findFresh(const MacAddress& peer, Hz channelWidth,
            uint64_t associationGeneration) const
    {
        auto it = entries.find({peer, channelWidth, associationGeneration});
        return it != entries.end() && timeProvider() <= it->second.expiryTime ?
                &it->second : nullptr;
    }

    void invalidatePeer(const MacAddress& peer)
    {
        for (auto it = entries.begin(); it != entries.end(); )
            it = it->first.peer == peer ? entries.erase(it) : std::next(it);
    }

    void invalidate(const MacAddress& peer, Hz channelWidth,
            uint64_t associationGeneration)
    {
        entries.erase({peer, channelWidth, associationGeneration});
    }

    void invalidateOtherWidths(Hz channelWidth)
    {
        for (auto it = entries.begin(); it != entries.end(); )
            it = it->first.channelWidth != channelWidth ? entries.erase(it) : std::next(it);
    }

    void clear() { entries.clear(); }
};

} // namespace ieee80211
} // namespace inet

#endif
