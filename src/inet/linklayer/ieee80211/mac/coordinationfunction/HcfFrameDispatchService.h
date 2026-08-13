//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFFRAMEDISPATCHSERVICE_H
#define __INET_HCFFRAMEDISPATCHSERVICE_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

class Packet;
namespace physicallayer { class IIeee80211Mode; }

namespace ieee80211 {

/**
 * Stateless common HCF frame-subtype dispatcher.
 *
 * Packets and headers are borrowed for each synchronous call. The service
 * never stores, deletes, or transfers them. Actions retain all mutable ACK,
 * BlockAck, recovery, management, and power-save state. Action exceptions are
 * propagated and already completed actions are not rolled back.
 */
class INET_API HcfFrameDispatchService
{
  public:
    enum class ExpectedResponse {
        NONE,
        ACK,
        BLOCK_ACK,
    };

    enum class FailureKind {
        ACK_TIMEOUT,
        BLOCK_ACK_MISSING,
    };

    struct MultiTidResponseContext {
        bool validResponseAid = false;
        uint16_t responseAid = 0;
        bool heMultiTidAggregation = false;
        bool legacyMultiTidBlockAck = false;
    };

    class IRecipientActions
    {
      public:
        virtual ~IRecipientActions() {}

        virtual void recipientPsPoll(const Ptr<const Ieee80211PsPollFrame>& frame) = 0;
        virtual void recipientRts(Packet *packet, const Ptr<const Ieee80211RtsFrame>& frame) = 0;
        virtual MultiTidResponseContext getMultiTidResponseContext(
                const Ptr<const Ieee80211MultiTidBlockAckReq>& frame) = 0;
        virtual void recipientBlockAckRequest(Packet *packet,
                const Ptr<const Ieee80211BlockAckReq>& frame,
                MultiTidBlockAckResponseFormat responseFormat,
                uint16_t responseAid) = 0;
        virtual void recipientStaleAck(const Ptr<const Ieee80211AckFrame>& frame) = 0;
        virtual void recipientAddbaRequest(const Ptr<const Ieee80211AddbaRequest>& frame) = 0;
        virtual void recipientAddbaResponse(const Ptr<const Ieee80211AddbaResponse>& frame) = 0;
        virtual void recipientDelba(const Ptr<const Ieee80211Delba>& frame) = 0;
    };

    class IResponseActions
    {
      public:
        virtual ~IResponseActions() {}
        virtual const physicallayer::IIeee80211Mode *selectCtsResponseMode(
                Packet *packet, const Ptr<const Ieee80211RtsFrame>& frame) = 0;
        virtual const physicallayer::IIeee80211Mode *selectBlockAckResponseMode(
                Packet *packet, const Ptr<const Ieee80211BlockAckReq>& frame) = 0;
        virtual const physicallayer::IIeee80211Mode *selectImplicitBlockAckResponseMode(
                Packet *responsePacket, Packet *receivedPacket,
                const Ptr<const Ieee80211BlockAck>& responseFrame,
                const Ptr<const Ieee80211DataHeader>& receivedFrame) = 0;
        virtual const physicallayer::IIeee80211Mode *selectAckResponseMode(
                Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& frame) = 0;
        virtual void transmittedCtsResponse(const Ptr<const Ieee80211CtsFrame>& frame) = 0;
        virtual void transmittedBlockAckResponse(const Ptr<const Ieee80211BlockAck>& frame) = 0;
        virtual void transmittedAckResponse(const Ptr<const Ieee80211AckFrame>& frame) = 0;
    };

    class IOriginatorTransmitActions
    {
      public:
        virtual ~IOriginatorTransmitActions() {}
        virtual void transmittedGroup(Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header) = 0;
        virtual void transmittedData(Packet *packet,
                const Ptr<const Ieee80211DataHeader>& header,
                ExpectedResponse expectedResponse) = 0;
        virtual bool isManagementAckNeeded(const Ptr<const Ieee80211MgmtHeader>& frame) = 0;
        virtual void transmittedManagementAck(const Ptr<const Ieee80211MgmtHeader>& frame) = 0;
        virtual void transmittedAddbaRequest(const Ptr<const Ieee80211AddbaRequest>& frame) = 0;
        virtual void transmittedAddbaResponse(const Ptr<const Ieee80211AddbaResponse>& frame) = 0;
        virtual void transmittedDelba(const Ptr<const Ieee80211Delba>& frame) = 0;
        virtual void transmittedBlockAckRequest(const Ptr<const Ieee80211BlockAckReq>& frame) = 0;
        virtual void transmittedRts(const Ptr<const Ieee80211RtsFrame>& frame) = 0;
        virtual void failedDataOrManagement(
                Packet *packet,
                const Ptr<const Ieee80211DataOrMgmtHeader>& frame,
                FailureKind failureKind) = 0;
        virtual void failedBlockAckRequest(
                Packet *packet,
                const Ptr<const Ieee80211BlockAckReq>& frame) = 0;
    };

    class IOriginatorReceiveActions
    {
      public:
        virtual ~IOriginatorReceiveActions() {}
        virtual void originatorAddbaResponse(const Ptr<const Ieee80211AddbaResponse>& frame) = 0;
        virtual void originatorAck(Packet *packet,
                const Ptr<const Ieee80211AckFrame>& frame,
                Packet *lastTransmittedPacket,
                const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader) = 0;
        virtual void originatorBlockAck(const Ptr<const Ieee80211BlockAck>& frame) = 0;
        virtual void originatorCts(const Ptr<const Ieee80211CtsFrame>& frame) = 0;
        virtual void originatorIgnoredControl(const Ptr<const Ieee80211MacHeader>& frame) = 0;
        virtual void originatorData(const Ptr<const Ieee80211DataHeader>& frame,
                const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader) = 0;
    };

  public:
    void dispatchRecipientControl(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header, IRecipientActions& actions) const;
    void dispatchRecipientManagement(const Ptr<const Ieee80211MgmtHeader>& header,
            IRecipientActions& actions) const;
    const physicallayer::IIeee80211Mode *selectImmediateResponseMode(
            Packet *responsePacket,
            const Ptr<const Ieee80211MacHeader>& responseHeader,
            Packet *receivedPacket,
            const Ptr<const Ieee80211MacHeader>& receivedHeader,
            IResponseActions& actions) const;
    void dispatchTransmittedControlResponse(
            const Ptr<const Ieee80211MacHeader>& header, IResponseActions& actions) const;
    void dispatchTransmitted(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header, IOriginatorTransmitActions& actions) const;
    void dispatchTransmittedData(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header, IOriginatorTransmitActions& actions) const;
    void dispatchTransmittedManagement(const Ptr<const Ieee80211MgmtHeader>& header,
            IOriginatorTransmitActions& actions) const;
    void dispatchTransmittedControl(const Ptr<const Ieee80211MacHeader>& header,
            IOriginatorTransmitActions& actions) const;
    void dispatchOriginatorFailure(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header,
            IOriginatorTransmitActions& actions) const;
    void dispatchOriginatorReceived(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header,
            Packet *lastTransmittedPacket,
            const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader,
            IOriginatorReceiveActions& actions) const;
    void dispatchOriginatorReceivedManagement(
            const Ptr<const Ieee80211MgmtHeader>& header, IOriginatorReceiveActions& actions) const;
    void dispatchOriginatorReceivedControl(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header,
            Packet *lastTransmittedPacket,
            const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader,
            IOriginatorReceiveActions& actions) const;
};

} // namespace ieee80211
} // namespace inet

#endif
