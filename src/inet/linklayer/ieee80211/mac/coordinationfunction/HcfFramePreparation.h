//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFFRAMEPREPARATION_H
#define __INET_HCFFRAMEPREPARATION_H

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/originator/TxopProcedure.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HtCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

/**
 * Stateless base-HCF frame-preparation decisions.
 *
 * This helper consumes already resolved, immutable preparation inputs. It does
 * not inspect or mutate packets, TXOP state, frame-sequence context, timers,
 * or amendment-specific HCF state. Receiver/capability/mode extraction stays
 * in Hcf because it crosses packet, MIB, and PHY-authority boundaries; there
 * is no further side-effect-free preparation decision to extract here.
 */
class INET_API HcfFramePreparation
{
  public:
    // IEEE Std 802.11-2024, 10.27.3: select the legacy protection required by
    // the negotiated HT operation when the first PHY mode is already known.
    static TxopProcedure::InitialProtection selectInitialProtection(
            const MacAddress& receiverAddress,
            physicallayer::Ieee80211PhyFamily phyFamily,
            const Ieee80211NegotiatedHtCapabilities *negotiatedCapabilities);
};

} // namespace ieee80211
} // namespace inet

#endif
