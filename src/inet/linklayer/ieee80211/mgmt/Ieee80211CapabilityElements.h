//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211CAPABILITYELEMENTS_H
#define __INET_IEEE80211CAPABILITYELEMENTS_H

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

/**
 * Populates the HT and VHT capability elements from the authoritative local
 * mode set. VHT support implies HT support, as required for a VHT STA.
 */
INET_API void populateIeee80211CapabilityElements(const Ptr<Ieee80211MgmtFrame>& frame,
        const physicallayer::Ieee80211ModeSet *modeSet,
        int maximumSpatialStreams, bool htLdpcRxSupported, bool vhtLdpcRxSupported);

} // namespace ieee80211
} // namespace inet

#endif
