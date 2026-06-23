//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211TWTAGREEMENT_H
#define __INET_IEEE80211TWTAGREEMENT_H

#include <set>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

/** Runtime representation of one negotiated individual or broadcast TWT. */
struct INET_API TwtAgreement
{
    MacAddress peerAddress;
    uint8_t flowId = 0;
    uint8_t broadcastId = 0;
    bool broadcast = false;
    bool implicit = true;
    bool announced = false;
    bool triggerEnabled = false;
    bool active = false;
    bool peerAwakeAnnounced = false;
    simtime_t nextWakeTime = SIMTIME_ZERO;
    simtime_t wakeInterval = SIMTIME_ZERO;
    simtime_t wakeDuration = SIMTIME_ZERO;
    uint8_t persistence = 255;
};

/** AP-side description of a broadcast TWT schedule and its current members. */
struct INET_API TwtBroadcastSchedule : public TwtAgreement
{
    std::set<MacAddress> members;
    simtime_t expiresAt = SIMTIME_MAX;

    TwtBroadcastSchedule() { broadcast = true; }
};

} // namespace ieee80211
} // namespace inet

#endif
