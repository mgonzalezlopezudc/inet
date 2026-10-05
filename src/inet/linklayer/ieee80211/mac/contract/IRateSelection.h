//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IRATESELECTION_H
#define __INET_IRATESELECTION_H

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/contract/Ieee80211ControlMode.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

/**
 * Abstract interface for rate selection. Rate selection decides what bit rate
 * (or MCS) should be used for any particular frame. The rules of rate selection
 * is described in the 802.11 specification in the section titled "Multirate Support".
 */
class INET_API IRateSelection
{
  public:
    static simsignal_t datarateSelectedSignal;

  public:
    virtual ~IRateSelection() {}

    // Snapshot queries change no management state. Prediction ignores recipient overrides.
    virtual ResponseRateContext snapshotResponseRateContext(const Ptr<const Ieee80211MacHeader>& requestHeader,
            ResponseRequestRole role, const std::optional<BssRateContextRef>& explicitContext) const = 0;
    virtual ResponseModeResult computeResponseMode(const ResponseModeInput& input) const = 0;
    // Record actual transmissions only. A null packet clears history after lifecycle teardown.
    virtual void frameTransmitted(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, uint64_t txopGeneration) = 0;

    virtual const physicallayer::IIeee80211Mode *computeResponseCtsFrameMode(Packet *packet, const Ptr<const Ieee80211RtsFrame>& rtsFrame) = 0;
    virtual const physicallayer::IIeee80211Mode *computeResponseAckFrameMode(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader) = 0;

    virtual const physicallayer::IIeee80211Mode *computeMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
