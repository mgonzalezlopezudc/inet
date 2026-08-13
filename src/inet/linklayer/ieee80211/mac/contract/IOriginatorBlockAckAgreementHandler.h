//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IORIGINATORBLOCKACKAGREEMENTHANDLER_H
#define __INET_IORIGINATORBLOCKACKAGREEMENTHANDLER_H

#include <functional>
#include <memory>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/linklayer/ieee80211/mac/contract/IBlockAckAgreementHandlerCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IProcedureCallback.h"

namespace inet {
namespace ieee80211 {

class INET_API IOriginatorBlockAckAgreementHandler
{
  public:
    struct PrerequisiteProbe {
        bool required = false;
        long packetId = -1;
        MacAddress receiverAddress = MacAddress::UNSPECIFIED_ADDRESS;
        Tid tid = -1;
        bool agreementPresent = false;
        uint64_t agreementGeneration = 0;
    };

    struct PrerequisiteReservation {
      private:
        std::unique_ptr<Packet> packet;
        std::function<void()> cancelAction;

      public:
        PrerequisiteReservation() = default;
        PrerequisiteReservation(Packet *packet, std::function<void()> cancelAction) :
            packet(packet), cancelAction(std::move(cancelAction)) {}
        PrerequisiteReservation(PrerequisiteReservation&&) = default;
        PrerequisiteReservation& operator=(PrerequisiteReservation&&) = default;
        PrerequisiteReservation(const PrerequisiteReservation&) = delete;
        PrerequisiteReservation& operator=(const PrerequisiteReservation&) = delete;
        ~PrerequisiteReservation() { cancel(); }

        explicit operator bool() const { return packet != nullptr; }
        Packet *getPacket() const { return packet.get(); }
        Packet *releasePacket() { return packet.release(); }
        void cancel()
        {
            packet.reset();
            if (cancelAction) {
                auto action = std::move(cancelAction);
                action();
            }
        }
        void commit() { packet.release(); cancelAction = {}; }
    };

    virtual ~IOriginatorBlockAckAgreementHandler() {}

    virtual void processReceivedBlockAck(const Ptr<const Ieee80211BlockAck>& blockAck, IBlockAckAgreementHandlerCallback *callback) = 0;
    virtual void processTransmittedAddbaReq(const Ptr<const Ieee80211AddbaRequest>& addbaReq,
            IBlockAckAgreementHandlerCallback *callback) = 0;
    virtual bool processQueuedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader,
            IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy, IProcedureCallback *callback) = 0;
    virtual PrerequisiteProbe probeQueuedDataFramePrerequisite(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& dataHeader,
            IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy) { return {}; }
    virtual PrerequisiteReservation reserveQueuedDataFramePrerequisite(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& dataHeader,
            IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy) { return {}; }
    virtual PrerequisiteReservation reserveQueuedDataFramePrerequisite(
            const PrerequisiteProbe& probe, Packet *packet,
            const Ptr<const Ieee80211DataHeader>& dataHeader,
            IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy)
        { return reserveQueuedDataFramePrerequisite(packet, dataHeader, blockAckAgreementPolicy); }
    virtual void processTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy, IProcedureCallback *callback) = 0;
    virtual void processReceivedAddbaResp(const Ptr<const Ieee80211AddbaResponse>& addbaResp, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy, IBlockAckAgreementHandlerCallback *callback) = 0;
    virtual void processReceivedDelba(const Ptr<const Ieee80211Delba>& delba, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy) = 0;
    virtual void processTransmittedDelba(const Ptr<const Ieee80211Delba>& delba) = 0;
    virtual void blockAckAgreementExpired(IProcedureCallback *procedureCallback, IBlockAckAgreementHandlerCallback *agreementHandlerCallback) = 0;

    virtual OriginatorBlockAckAgreement *getAgreement(MacAddress receiverAddr, Tid tid) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
