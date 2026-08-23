//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211VHTSIGA_H
#define __INET_IEEE80211VHTSIGA_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace physicallayer {

/** Returns int(BSSID[39:47]) using IEEE 802.11 on-wire bit numbering. */
INET_API int computeVhtPartialAidForBssid(const MacAddress& bssid);

/** Returns STA_Partial_AID_VHT from the association ID and BSSID. */
INET_API int computeVhtPartialAidForAssociatedSta(int associationId, const MacAddress& bssid);

/** Rejects VHT-MU Group IDs and out-of-range Partial AIDs at a VHT-SU boundary. */
INET_API void validateVhtSuGroupIdAndPartialAid(unsigned int groupId, unsigned int partialAid);

} // namespace physicallayer
} // namespace inet

#endif
