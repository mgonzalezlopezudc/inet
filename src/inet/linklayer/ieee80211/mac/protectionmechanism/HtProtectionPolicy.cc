//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/protectionmechanism/HtProtectionPolicy.h"

namespace inet {
namespace ieee80211 {

HtProtectionPolicy::Protection HtProtectionPolicy::select(bool isHtMode,
        const MacAddress& receiverAddress,
        const Ieee80211NegotiatedHtCapabilities *negotiatedCapabilities)
{
    return isHtMode && !receiverAddress.isMulticast() &&
            negotiatedCapabilities != nullptr &&
            negotiatedCapabilities->localTxPeerRx.valid &&
            negotiatedCapabilities->operation.protectionMode == Ieee80211HtProtectionMode::NON_HT_MIXED ?
            Protection::LEGACY_RTS_CTS : Protection::NONE;
}

} // namespace ieee80211
} // namespace inet
