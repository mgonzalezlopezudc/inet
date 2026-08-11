//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HtSoundingRetryState.h"

namespace inet {
namespace ieee80211 {

bool HtSoundingRetryState::isAttemptAllowed(const MacAddress& peer, simtime_t now) const
{
    auto attempt = nextAttemptTimes.find(peer);
    return attempt == nextAttemptTimes.end() || now >= attempt->second;
}

uint8_t HtSoundingRetryState::recordAttempt(const MacAddress& peer, simtime_t now,
        simtime_t retryInterval)
{
    auto token = nextTokens[peer];
    nextTokens[peer] = (token + 1) % 7;
    nextAttemptTimes[peer] = now + retryInterval;
    return token;
}

void HtSoundingRetryState::invalidate(const MacAddress& peer)
{
    nextAttemptTimes.erase(peer);
    nextTokens.erase(peer);
}

} // namespace ieee80211
} // namespace inet
