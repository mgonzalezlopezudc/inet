//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HTPROTECTIONPOLICY_H
#define __INET_HTPROTECTIONPOLICY_H

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HtCapabilities.h"

namespace inet {
namespace ieee80211 {

class INET_API HtProtectionPolicy
{
  public:
    enum class Protection {
        NONE,
        LEGACY_RTS_CTS,
    };

    // IEEE Std 802.11-2024, 10.27.3 and Table 10-26. This deliberately
    // implements only the negotiated individual-address/non-HT-mixed subset.
    static Protection select(bool isHtMode, const MacAddress& receiverAddress,
            const Ieee80211NegotiatedHtCapabilities *negotiatedCapabilities);
};

} // namespace ieee80211
} // namespace inet

#endif
