//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HETWTGATING_H
#define __INET_HETWTGATING_H

#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

inline bool isTwtSleeping(const Ieee80211Mac *mac, const MacAddress& peer)
{
    return mac != nullptr && !mac->isTwtPeerEligible(peer);
}

} // namespace ieee80211
} // namespace inet

#endif // __INET_HETWTGATING_H
