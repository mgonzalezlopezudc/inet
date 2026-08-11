//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFramePreparation.h"

#include "inet/linklayer/ieee80211/mac/protectionmechanism/HtProtectionPolicy.h"

namespace inet {
namespace ieee80211 {

TxopProcedure::InitialProtection HcfFramePreparation::selectInitialProtection(
        const MacAddress& receiverAddress,
        physicallayer::Ieee80211PhyFamily phyFamily,
        const Ieee80211NegotiatedHtCapabilities *negotiatedCapabilities)
{
    bool isHtMode = phyFamily == physicallayer::Ieee80211PhyFamily::HT;
    auto protection = HtProtectionPolicy::select(isHtMode, receiverAddress,
            negotiatedCapabilities);
    return protection == HtProtectionPolicy::Protection::LEGACY_RTS_CTS ?
            TxopProcedure::InitialProtection::LEGACY_RTS_CTS :
            TxopProcedure::InitialProtection::NONE;
}

} // namespace ieee80211
} // namespace inet
