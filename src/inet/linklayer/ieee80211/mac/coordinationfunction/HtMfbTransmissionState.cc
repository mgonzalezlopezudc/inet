//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtMfbTransmissionState.h"

namespace inet {
namespace ieee80211 {

void HtMfbTransmissionState::setPending(const MacAddress& peer,
        const Ieee80211HtMcsControl& control)
{
    pending.peer = peer;
    pending.control = control;
}

void HtMfbTransmissionState::invalidate(const MacAddress& peer)
{
    if (pending.peer == peer)
        clearPending();
}

bool HtMfbTransmissionState::completeStandaloneTransmission()
{
    if (!standaloneTransmissionInProgress)
        return false;
    standaloneTransmissionInProgress = false;
    return true;
}

} // namespace ieee80211
} // namespace inet
