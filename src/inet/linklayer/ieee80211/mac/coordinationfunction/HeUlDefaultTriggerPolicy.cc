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
    if (stage == INITSTAGE_LOCAL)
        minimumTriggerInterval = par("minimumTriggerInterval");
}

IIeee80211HeUlTriggerPolicy::TriggerType HeUlDefaultTriggerPolicy::selectTrigger(const Context& context) const
{
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
