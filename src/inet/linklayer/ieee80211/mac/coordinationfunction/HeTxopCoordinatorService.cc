//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTxopCoordinatorService.h"

#include "omnetpp.h"

namespace inet {
namespace ieee80211 {

HeTxopCoordinatorService::Outcome HeTxopCoordinatorService::start(
        bool heMode, bool forceSingleUser, const Actions& actions) const
{
    if (!actions.startSu || !actions.releaseChannelIfNoSu ||
            (heMode && (!actions.tryStartUlMu || !actions.tryStartDlMu)))
        throw omnetpp::cRuntimeError("Incomplete HE TXOP coordinator actions");

    if (forceSingleUser) {
        actions.startSu();
        return SU_STARTED;
    }
    if (heMode) {
        if (actions.tryStartUlMu())
            return UL_MU_STARTED;
        if (actions.tryStartDlMu())
            return DL_MU_STARTED;
    }
    if (actions.releaseChannelIfNoSu())
        return CHANNEL_RELEASED;
    actions.startSu();
    return SU_STARTED;
}

} // namespace ieee80211
} // namespace inet
