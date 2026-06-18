//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEMUUTIL_H
#define __INET_IEEE80211HEMUUTIL_H

#include <algorithm>

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace physicallayer {

enum Ieee80211HePpduFormat {
    HE_MU_DOWNLINK = 0,
    HE_TRIGGER_BASED_UPLINK = 1
};

inline simtime_t estimateHeMuUserDuration(B psduLength, int toneSize, int mcs)
{
    static const double efficiency[] =
        {0.5, 1, 1.5, 2, 3, 4, 4.5, 5, 6, 6.6666667, 7.5, 8.3333333};
    double rate = std::max(toneSize, 26) * 78125.0 * efficiency[std::clamp(mcs, 0, 11)];
    return SimTime(48e-6 + psduLength.get<B>() * 8.0 / std::max(rate, 1.0));
}

inline uint16_t computeHeMuStaId(const MacAddress& address)
{
    // TODO replace this fallback with the association ID when it is available
    // at the packet-level PHY boundary.
    return address.isBroadcast() ? 2047 : static_cast<uint16_t>(address.getInt() & 0x7ff);
}

inline uint16_t resolveHeMuStaId(const cModule *networkInterface, const MacAddress& address)
{
    if (networkInterface != nullptr) {
        auto mib = dynamic_cast<const ieee80211::Ieee80211Mib *>(networkInterface->getSubmodule("mib"));
        if (mib != nullptr) {
            if (mib->bssStationData.stationType == ieee80211::Ieee80211Mib::STATION &&
                    mib->bssStationData.associationId > 0)
                return mib->bssStationData.associationId;
            auto aid = mib->getAssociationId(address);
            if (aid > 0)
                return aid;
        }
    }
    return computeHeMuStaId(address);
}

} // namespace physicallayer
} // namespace inet

#endif
