//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHEULMUEXCHANGECALLBACK_H
#define __INET_IHEULMUEXCHANGECALLBACK_H

#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuPlan.h"

namespace inet {

namespace ieee80211 {

/** Typed provider boundary used by HE and EHT Trigger-based UL frame sequences. */
class INET_API IHeUlMuExchangeCallback
{
  public:
    virtual ~IHeUlMuExchangeCallback() {}

    virtual uint32_t allocateHeUlTriggerId() = 0;
    virtual void heUlMuPlanCommitted(const HeUlMuPlan& plan, uint32_t triggerId) = 0;
    virtual uint16_t getHeUlAssociationId(const MacAddress& address) const = 0;
    virtual const Ptr<Ieee80211CompressedBlockAck> processHeUlTriggeredBlockAckReq(
            Packet *packet, const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
            uint16_t associationId) = 0;
    virtual void processHeUlTriggeredFrame(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header, uint16_t associationId) = 0;
    /** Delivers the single preassociation management MPDU without scheduling
     * a legacy per-frame ACK; the HE-TB exchange owns its acknowledgment. */
    virtual void processHeUlTriggeredManagementFrame(Packet *packet,
            const Ptr<const Ieee80211MgmtHeader>& header, uint16_t staId)
    {
        delete packet;
    }
};

} // namespace ieee80211
} // namespace inet

#endif
