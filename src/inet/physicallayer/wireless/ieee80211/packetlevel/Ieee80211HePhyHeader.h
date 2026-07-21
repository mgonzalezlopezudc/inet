//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEPHYHEADER_H
#define __INET_IEEE80211HEPHYHEADER_H

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

namespace inet {
namespace physicallayer {

INET_API Ptr<Ieee80211HePhyHeader> createIeee80211HePhyHeader(Ieee80211HePpduFormat ppduFormat);
INET_API Ieee80211HePpduFormat getIeee80211HePpduFormat(const Ieee80211HePhyHeader& phyHeader);

} // namespace physicallayer
} // namespace inet

#endif
