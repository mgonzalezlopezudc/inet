//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

namespace inet {
namespace ieee80211 {

Define_Module(HeSoundingCoordinator);

void HeSoundingCoordinator::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        WATCH(service->ndpAnnouncementReceived);
        WATCH(service->ndpReceived);
        WATCH(service->soundingDialogToken);
        WATCH(service->nextSoundingDialogToken);
        WATCH_VECTOR(service->soundingTargets);
        WATCH_EXPR("soundingTargetCount", service->soundingTargets.size());
        WATCH_EXPR("soundingState", service->ndpAnnouncementReceived ? (service->ndpReceived ? "NDP received" : "NDPA received") : "idle");
    }
}

} // namespace ieee80211
} // namespace inet
