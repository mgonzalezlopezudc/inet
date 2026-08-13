// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_IHCFTXRXINTERCEPTOR_H
#define __INET_IHCFTXRXINTERCEPTOR_H

#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"

namespace inet {
namespace ieee80211 {

/** Typed amendment hook invoked at fixed points in the common HCF pipeline. */
class INET_API IHcfTxRxInterceptor
{
  public:
    enum class Disposition { CONSUMED, CONTINUE_COMMON, REJECTED };
    enum class PacketOwnership { RETAINED_BY_CALLER, TRANSFERRED };

    struct Result {
        Disposition disposition = Disposition::CONTINUE_COMMON;
        PacketOwnership ownership = PacketOwnership::RETAINED_BY_CALLER;

        static Result continueCommon() { return {}; }
        static Result consumed() { return {Disposition::CONSUMED, PacketOwnership::TRANSFERRED}; }
        static Result consumedRetained() { return {Disposition::CONSUMED, PacketOwnership::RETAINED_BY_CALLER}; }
        static Result rejected() { return {Disposition::REJECTED, PacketOwnership::TRANSFERRED}; }
        static Result rejectedRetained() { return {Disposition::REJECTED, PacketOwnership::RETAINED_BY_CALLER}; }
    };

    virtual ~IHcfTxRxInterceptor() {}

    static void validateResult(const Result& result)
    {
        if (result.disposition == Disposition::CONTINUE_COMMON &&
                result.ownership != PacketOwnership::RETAINED_BY_CALLER)
            throw cRuntimeError("Invalid HCF interceptor result: common continuation must retain packet ownership");
    }

    virtual Result processPhyIndication(Packet *) { return Result::continueCommon(); }
    virtual Result processRejectedHeaderlessResponse(Packet *,
            IReceiveStep::HeaderlessResponseFamily) { return Result::continueCommon(); }
    virtual Result processRecipientFrame(Packet *, const Ptr<const Ieee80211MacHeader>&) { return Result::continueCommon(); }
    virtual Result processTransmittedFrame(Packet *) { return Result::continueCommon(); }
    virtual Result processReceivedResponse(Packet *, Packet *) { return Result::continueCommon(); }
    virtual Result processFailedFrame(Packet *) { return Result::continueCommon(); }
    virtual Result processTransmissionComplete(Packet *, const Ptr<const Ieee80211MacHeader>&) { return Result::continueCommon(); }
    virtual Result processTransmittedControl(const Ptr<const Ieee80211MacHeader>&, AccessCategory) { return Result::continueCommon(); }
    virtual Result processTransmitRequest(Packet *, simtime_t) { return Result::continueCommon(); }
};

} // namespace ieee80211
} // namespace inet

#endif
