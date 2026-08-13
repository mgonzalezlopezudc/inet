//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTxopCoordinatorService.h"

#include "omnetpp.h"

namespace inet {
namespace ieee80211 {

HeTxopCoordinatorService::GrantSnapshot HeTxopCoordinatorService::prepareGrant(
        AccessCategory accessCategory, bool heMode, bool forcedSingleUser,
        bool hasEligibleFrame, const PreparationActions& actions) const
{
    GrantSnapshot snapshot;
    snapshot.accessCategory = accessCategory;
    if (forcedSingleUser) {
        snapshot.exchangeClass = HcfExchangeClass::FORCED_SINGLE_USER;
        return snapshot;
    }
    if (heMode) {
        if (!actions.prepareUlTrigger || !actions.prepareDlStart)
            throw cRuntimeError("Incomplete HE TXOP preparation actions");
        snapshot.ulTrigger = actions.prepareUlTrigger();
        if (snapshot.ulTrigger) {
            snapshot.exchangeClass = HcfExchangeClass::HE_UL_TRIGGER;
            return snapshot;
        }
        snapshot.dlStart = actions.prepareDlStart();
        if (snapshot.dlStart) {
            switch (snapshot.dlStart->kind) {
                case HeDlMuExchangeProvider::StartKind::HE_SOUNDING:
                    snapshot.exchangeClass = HcfExchangeClass::HE_SOUNDING;
                    return snapshot;
                case HeDlMuExchangeProvider::StartKind::RECOVERY_SINGLE_USER:
                case HeDlMuExchangeProvider::StartKind::ADDBA_SINGLE_USER:
                    snapshot.exchangeClass = HcfExchangeClass::RECOVERY_SINGLE_USER;
                    return snapshot;
                case HeDlMuExchangeProvider::StartKind::HE_DL_MULTIUSER:
                    snapshot.exchangeClass = HcfExchangeClass::HE_DL_MULTIUSER;
                    return snapshot;
                case HeDlMuExchangeProvider::StartKind::SINGLE_USER_FALLBACK:
                    snapshot.exchangeClass = HcfExchangeClass::SINGLE_USER;
                    return snapshot;
            }
        }
    }
    if (actions.prepareSingleUser)
        snapshot.dlStart = actions.prepareSingleUser();
    snapshot.exchangeClass = hasEligibleFrame || snapshot.dlStart ?
            HcfExchangeClass::SINGLE_USER : HcfExchangeClass::CHANNEL_RELEASE;
    return snapshot;
}

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
