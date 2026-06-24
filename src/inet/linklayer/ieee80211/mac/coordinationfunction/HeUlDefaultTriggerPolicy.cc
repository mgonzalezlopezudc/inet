//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlDefaultTriggerPolicy.h"

namespace inet {
namespace ieee80211 {

Define_Module(HeUlDefaultTriggerPolicy);

void HeUlDefaultTriggerPolicy::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        minimumTriggerInterval = par("minimumTriggerInterval");
        ASSERT(minimumTriggerInterval >= SIMTIME_ZERO);
    }
}

IIeee80211HeUlTriggerPolicy::TriggerType HeUlDefaultTriggerPolicy::selectTrigger(const Context& context) const
{
    // Grounded on IEEE 802.11-2024 Clause 26.5.2 ("Uplink multi-user operation").
    // Trigger frames coordinate HE TB PPDU transmissions by defining parameters such as RU size and MCS.
    // - NO_TRIGGER: No uplink exchange is requested (e.g. within minimum interval to prevent overhead).
    // - BASIC_TRIGGER (Type 0): Requests HE TB payload transmissions from STAs with known backlogged or retry traffic.
    // - BSRP_TRIGGER (Type 4): Polls associated STAs for fresh buffer status reports when current backlog info is stale.
    if (context.associatedStations == 0 || context.elapsedSinceLastTrigger < minimumTriggerInterval)
        return NO_TRIGGER;
    if (context.backloggedStations > 0 || context.retryStations > 0)
        return BASIC_TRIGGER;
    if (context.freshReports < context.associatedStations)
        return BSRP_TRIGGER;
    return NO_TRIGGER;
}

} // namespace ieee80211
} // namespace inet
