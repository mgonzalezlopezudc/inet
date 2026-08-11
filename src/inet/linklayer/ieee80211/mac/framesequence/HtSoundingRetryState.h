//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HTSOUNDINGRETRYSTATE_H
#define __INET_HTSOUNDINGRETRYSTATE_H

#include <map>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

/**
 * Owns the per-peer cooldown and request-token state for HT sounding attempts.
 */
class INET_API HtSoundingRetryState
{
  private:
    std::map<MacAddress, simtime_t> nextAttemptTimes;
    std::map<MacAddress, uint8_t> nextTokens;

  public:
    bool isAttemptAllowed(const MacAddress& peer, simtime_t now) const;
    uint8_t recordAttempt(const MacAddress& peer, simtime_t now, simtime_t retryInterval);
    void invalidate(const MacAddress& peer);
};

} // namespace ieee80211
} // namespace inet

#endif
