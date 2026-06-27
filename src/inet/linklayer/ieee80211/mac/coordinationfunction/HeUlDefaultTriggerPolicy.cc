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
        WATCH(minimumTriggerInterval);
        WATCH(lastContext.associatedStations);
        WATCH(lastContext.freshReports);
        WATCH(lastContext.backloggedStations);
        WATCH(lastContext.retryStations);
        WATCH(lastContext.elapsedSinceLastTrigger);
        WATCH(lastSelectedTrigger);
        WATCH_EXPR("lastSelectedTriggerName", getLastSelectedTriggerName());
    }
}

IIeee80211HeUlTriggerPolicy::TriggerType HeUlDefaultTriggerPolicy::selectTrigger(const Context& context) const
{
    // IEEE 802.11-2024 Clause 26.5.2 ("Uplink multi-user operation").
    // Trigger frames coordinate HE TB PPDU transmissions by defining parameters such as RU size and MCS.
    // - NO_TRIGGER: No uplink exchange is requested (e.g. within minimum interval to prevent overhead).
    // - BASIC_TRIGGER (Type 0): Requests HE TB payload transmissions from STAs with known backlogged or retry traffic.
    // - BSRP_TRIGGER (Type 4): Polls associated STAs for fresh buffer status reports when current backlog info is stale.
    lastContext = context;
    if (context.associatedStations == 0 || context.elapsedSinceLastTrigger < minimumTriggerInterval)
        lastSelectedTrigger = NO_TRIGGER;
    else if (context.backloggedStations > 0 || context.retryStations > 0)
        lastSelectedTrigger = BASIC_TRIGGER;
    else if (context.freshReports < context.associatedStations)
        lastSelectedTrigger = BSRP_TRIGGER;
    else
        lastSelectedTrigger = NO_TRIGGER;
    return lastSelectedTrigger;
}

const char *HeUlDefaultTriggerPolicy::getLastSelectedTriggerName() const
{
    switch (lastSelectedTrigger) {
        case NO_TRIGGER: return "NO_TRIGGER";
        case BASIC_TRIGGER: return "BASIC_TRIGGER";
        case BSRP_TRIGGER: return "BSRP_TRIGGER";
        default: return "UNKNOWN";
    }
}

} // namespace ieee80211
} // namespace inet
