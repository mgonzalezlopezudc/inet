//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/EhtUlMuTxOpFs.h"

namespace inet {
namespace ieee80211 {

EhtUlMuTxOpFs::EhtUlMuTxOpFs(HeUlCoordinator *coordinator, HeHcf *callback, const IIeee80211HeUlScheduler::Schedule& schedule,
                             IIeee80211HeUlTriggerPolicy::TriggerType triggerType, physicallayer::Ieee80211ModeSet *modeSet,
                             MacAddress apAddress, bool ehtEnabled)
    : HeUlMuTxOpFs(coordinator, callback, schedule, triggerType, modeSet, apAddress)
{
    this->ehtEnabled = ehtEnabled;
}

EhtUlMuTxOpFs::~EhtUlMuTxOpFs()
{
}

} // namespace ieee80211
} // namespace inet
