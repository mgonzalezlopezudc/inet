//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEMUUTIL_H
#define __INET_IEEE80211HEMUUTIL_H

#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace physicallayer {

inline uint16_t computeHeMuStaId(const MacAddress& address)
{
    // TODO replace this fallback with the association ID when it is available
    // at the packet-level PHY boundary.
    return address.isBroadcast() ? 2047 : static_cast<uint16_t>(address.getInt() & 0x7ff);
}

} // namespace physicallayer
} // namespace inet

#endif
