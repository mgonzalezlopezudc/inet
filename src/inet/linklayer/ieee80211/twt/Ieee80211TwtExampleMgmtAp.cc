//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/twt/Ieee80211TwtExampleMgmtAp.h"

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/twt/ITwtManager.h"

namespace inet {
namespace ieee80211 {

Define_Module(Ieee80211TwtExampleMgmtAp);

void Ieee80211TwtExampleMgmtAp::initialize(int stage)
{
    Ieee80211MgmtAp::initialize(stage);
    if (stage != INITSTAGE_LAST || !par("createBroadcastSchedule"))
        return;
    auto manager = dynamic_cast<ITwtManager *>(findModuleFromPar<cModule>(par("twtModule"), this));
    if (manager == nullptr || !manager->isEnabled())
        return;
    TwtBroadcastSchedule schedule;
    schedule.peerAddress = MacAddress::BROADCAST_ADDRESS;
    schedule.broadcastId = par("broadcastId");
    schedule.implicit = par("implicit");
    schedule.announced = par("announced");
    schedule.triggerEnabled = par("triggerEnabled");
    schedule.persistence = par("persistence");
    schedule.nextWakeTime = simTime() + par("firstWakeOffset");
    schedule.wakeInterval = par("wakeInterval");
    schedule.wakeDuration = par("wakeDuration");
    schedule.active = true;
    manager->installBroadcastSchedule(schedule);
}

} // namespace ieee80211
} // namespace inet
