//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HETXOPCOORDINATORSERVICE_H
#define __INET_HETXOPCOORDINATORSERVICE_H

#include <functional>
#include <optional>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlTriggerService.h"

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
    /** Immutable, exact outcome captured once at the EDCAF grant boundary. */
    struct GrantSnapshot {
        enum class StartKind {
            CHANNEL_RELEASE,
            FORCED_SINGLE_USER,
            UL_TRIGGER,
            SOUNDING,
            RECOVERY_SINGLE_USER,
            DL_MULTIUSER,
            PREPARED_SINGLE_USER,
            COMMON_SINGLE_USER,
        };
        StartKind startKind = StartKind::CHANNEL_RELEASE;
        AccessCategory accessCategory = AC_BE;
        std::optional<HeUlTriggerService::PreparedStart> ulTrigger;
        std::optional<HeDlMuExchangeProvider::PreparedStart> dlStart;
    };

    /** Side-effect-free preparation seams, evaluated in semantic priority order. */
    struct PreparationActions {
        std::function<std::optional<HeUlTriggerService::PreparedStart>()> prepareUlTrigger;
        std::function<std::optional<HeDlMuExchangeProvider::PreparedStart>()> prepareDlStart;
        std::function<std::optional<HeDlMuExchangeProvider::PreparedStart>()> prepareSingleUser;
    };

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

    GrantSnapshot prepareGrant(AccessCategory accessCategory, bool heMode,
            bool forcedSingleUser, bool hasEligibleFrame,
            const PreparationActions& actions) const;

    Outcome start(bool heMode, bool forceSingleUser, const Actions& actions) const;
};

} // namespace ieee80211
} // namespace inet

#endif
