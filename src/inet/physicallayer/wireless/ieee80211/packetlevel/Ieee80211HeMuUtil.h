//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEMUUTIL_H
#define __INET_IEEE80211HEMUUTIL_H

#include <optional>

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace physicallayer {

/**
 * Reserved HE station ID used for a broadcast/multicast MU allocation.
 * IEEE 802.11-2024 Clause 26.5.1.2 uses STA_ID 2047 for broadcast RUs; the
 * HE-SIG-B User fields carrying STA-ID are defined in Tables 27-29 and 27-30.
 */
constexpr uint16_t HE_STA_ID_BROADCAST = 2047;

/** Returns a deterministic fallback HE station ID when no association ID is available. */
inline uint16_t computeHeMuStaId(const MacAddress& address)
{
    // TODO replace this fallback with the association ID when it is available
    // at the packet-level PHY boundary.
    // The fallback is intentionally limited to the 11-bit STA-ID namespace used
    // by HE-SIG-B User fields; associated STAs should use their AID instead.
    return address.isBroadcast() ? HE_STA_ID_BROADCAST : static_cast<uint16_t>(address.getInt() & 0x7ff);
}

/** Resolves a station ID from the interface MIB without applying a fallback. */
inline std::optional<uint16_t> tryResolveHeMuStaId(const cModule *networkInterface, const MacAddress& address)
{
    if (address.isBroadcast())
        return HE_STA_ID_BROADCAST;
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
    return std::nullopt;
}

/** Resolves a receive-side station ID, retaining a deterministic fallback for PHY-only test NICs. */
inline std::optional<uint16_t> resolveHeMuStaIdForReception(
        const cModule *networkInterface, const MacAddress& address)
{
    if (networkInterface != nullptr && networkInterface->getSubmodule("mib") != nullptr)
        return tryResolveHeMuStaId(networkInterface, address);
    return computeHeMuStaId(address); // compatibility for PHY-only test NICs
}

} // namespace physicallayer
} // namespace inet

#endif
