//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCSELECTION_H
#define __INET_IEEE80211LDPCSELECTION_H

#include "inet/linklayer/ieee80211/mgmt/contract/IIeee80211PeerCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

enum class Ieee80211LdpcFrameContext {
    DATA_OR_MANAGEMENT,
    TXOP_INITIATING_CONTROL,
    CONTROL_RESPONSE,
    OTHER_CONTROL
};

struct Ieee80211LdpcSelectionConfig {
    bool htTxActivated = false;
    bool vhtTxActivated = false;
};

struct Ieee80211LdpcSelectionResult {
    const physicallayer::IIeee80211Mode *mode = nullptr;
    bool noLdpcPreferenceOverridden = false;
};

INET_API const physicallayer::IIeee80211Mode *constrainIeee80211ModeByPeerOperatingMode(
        const physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *candidate,
        const IIeee80211PeerCapabilities *peerCapabilities,
        const MacAddress& receiverAddress,
        bool forcedMode);

INET_API Ieee80211LdpcSelectionResult selectIeee80211FecMode(
        const physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *candidate,
        bool forcedLdpc,
        const Ieee80211LdpcSelectionConfig& config,
        const IIeee80211PeerCapabilities *peerCapabilities,
        const MacAddress& receiverAddress,
        Ieee80211LdpcFrameContext frameContext,
        const physicallayer::IIeee80211Mode *solicitingMode = nullptr);

} // namespace ieee80211
} // namespace inet

#endif
