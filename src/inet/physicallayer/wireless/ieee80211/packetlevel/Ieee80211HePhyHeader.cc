//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"

namespace inet {
namespace physicallayer {

Ptr<Ieee80211HePhyHeader> createIeee80211HePhyHeader(Ieee80211HePpduFormat ppduFormat)
{
    switch (ppduFormat) {
        case HE_SINGLE_USER:
            return makeShared<Ieee80211HeSuPhyHeader>();
        case HE_EXTENDED_RANGE_SU:
            return makeShared<Ieee80211HeErSuPhyHeader>();
        case HE_MU_DOWNLINK:
            return makeShared<Ieee80211HeMuPhyHeader>();
        case HE_TRIGGER_BASED_UPLINK:
            return makeShared<Ieee80211HeTbPhyHeader>();
        default:
            throw cRuntimeError("Unknown IEEE 802.11 HE PPDU format: %d", ppduFormat);
    }
}

Ieee80211HePpduFormat getIeee80211HePpduFormat(const Ieee80211HePhyHeader& phyHeader)
{
    if (dynamic_cast<const Ieee80211HeSuPhyHeader *>(&phyHeader) != nullptr)
        return HE_SINGLE_USER;
    else if (dynamic_cast<const Ieee80211HeErSuPhyHeader *>(&phyHeader) != nullptr)
        return HE_EXTENDED_RANGE_SU;
    else if (dynamic_cast<const Ieee80211HeMuPhyHeader *>(&phyHeader) != nullptr)
        return HE_MU_DOWNLINK;
    else if (dynamic_cast<const Ieee80211HeTbPhyHeader *>(&phyHeader) != nullptr)
        return HE_TRIGGER_BASED_UPLINK;
    else
        throw cRuntimeError("Unknown concrete IEEE 802.11 HE PHY header type");
}

} // namespace physicallayer
} // namespace inet
