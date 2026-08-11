//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtSoundingPendingState.h"

namespace inet {
namespace ieee80211 {

void HtSoundingPendingState::invalidate(const MacAddress& peer)
{
    if (snapshot.peer == peer)
        clear();
}

} // namespace ieee80211
} // namespace inet
