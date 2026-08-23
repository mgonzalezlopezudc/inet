//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211RECEIVEDDATAENCODINGPLAN_H
#define __INET_IEEE80211RECEIVEDDATAENCODINGPLAN_H

#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"

namespace inet {
namespace physicallayer {

/**
 * Reconstructs an HT or VHT-SU data encoding plan exclusively from the
 * receiver-visible data duration, selected data mode, and decoded PHY fields.
 * Sender-side plan or exact VHT APEP metadata is neither accepted nor needed.
 *
 * Throws cRuntimeError when the duration is not an integral number of OFDM
 * symbols or when the received PHY fields disagree with the reconstructed
 * plan.
 */
INET_API Ieee80211DataEncodingPlan reconstructIeee80211ReceivedDataEncodingPlan(
        const IIeee80211DataMode *dataMode,
        const Ptr<const Ieee80211PhyHeader>& phyHeader,
        simtime_t dataDuration);

} // namespace physicallayer
} // namespace inet

#endif
