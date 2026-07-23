//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HETXOPCOORDINATORSERVICE_H
#define __INET_HETXOPCOORDINATORSERVICE_H

#include <functional>

#include "inet/common/INETDefs.h"

namespace inet {
namespace ieee80211 {

/**
 * Stateless HE TXOP admission coordinator.
 *
 * Exchange services retain ownership of their own validation, packet, timer,
 * and frame-sequence state. This class owns only the deterministic ordering
 * after an EDCA win: pending UL, DL MU, inherited SU, or channel release.
 */
class INET_API HeTxopCoordinatorService
{
  public:
    enum Outcome {
        UL_MU_STARTED,
        DL_MU_STARTED,
        SU_STARTED,
        CHANNEL_RELEASED
    };

    struct Actions {
        std::function<bool()> tryStartUlMu;
        std::function<bool()> tryStartDlMu;
        // Returns true when it released the channel because no SU is eligible.
        std::function<bool()> releaseChannelIfNoSu;
        std::function<void()> startSu;
    };

    Outcome start(bool heMode, bool forceSingleUser, const Actions& actions) const;
};

} // namespace ieee80211
} // namespace inet

#endif
