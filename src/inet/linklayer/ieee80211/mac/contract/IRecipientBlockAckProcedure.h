//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/IProcedureCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientQosAckPolicy.h"

#ifndef __INET_IRECIPIENTBLOCKACKPROCEDURE_H
#define __INET_IRECIPIENTBLOCKACKPROCEDURE_H

namespace inet {
namespace ieee80211 {

class INET_API IRecipientBlockAckProcedure
{
  public:
    virtual ~IRecipientBlockAckProcedure() {}

    /**
     * Builds the agreement-backed response to a BAR without transmitting it
     * or updating procedure counters. Returns nullptr when no agreement exists.
     */
    virtual const Ptr<Ieee80211BlockAck> buildBlockAck(
            const Ptr<const Ieee80211BlockAckReq>& blockAckReq,
            IRecipientBlockAckAgreementHandler *blockAckAgreementHandler) const = 0;
    virtual void processReceivedBlockAckReq(Packet *packet, const Ptr<const Ieee80211BlockAckReq>& blockAckReq, IRecipientQosAckPolicy *ackPolicy, IRecipientBlockAckAgreementHandler *blockAckAgreementHandler, IProcedureCallback *callback) = 0;
    virtual void processTransmittedBlockAck(const Ptr<const Ieee80211BlockAck>& blockAck) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
