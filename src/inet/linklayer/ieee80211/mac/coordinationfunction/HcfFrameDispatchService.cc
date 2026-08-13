//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFrameDispatchService.h"

#include "inet/common/packet/Packet.h"

namespace inet {
namespace ieee80211 {

void HcfFrameDispatchService::dispatchRecipientControl(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header, IRecipientActions& actions) const
{
    if (auto frame = dynamicPtrCast<const Ieee80211PsPollFrame>(header))
        actions.recipientPsPoll(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211RtsFrame>(header))
        actions.recipientRts(packet, frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211BlockAckReq>(header)) {
        // IEEE Std 802.11-2024, 10.25.3, 10.25.5 and 10.25.6.
        MultiTidBlockAckResponseFormat format = MultiTidBlockAckResponseFormat::NONE;
        uint16_t responseAid = 0;
        if (auto multiTidFrame = dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(frame)) {
            auto context = actions.getMultiTidResponseContext(multiTidFrame);
            responseAid = context.validResponseAid ? context.responseAid : 0;
            if (context.validResponseAid && context.heMultiTidAggregation)
                format = MultiTidBlockAckResponseFormat::HE_MULTI_STA;
            else if (context.legacyMultiTidBlockAck)
                format = MultiTidBlockAckResponseFormat::LEGACY_MULTI_TID;
        }
        actions.recipientBlockAckRequest(packet, frame, format, responseAid);
    }
    else if (auto frame = dynamicPtrCast<const Ieee80211AckFrame>(header))
        actions.recipientStaleAck(frame);
    else
        throw cRuntimeError("Unknown control frame");
}

void HcfFrameDispatchService::dispatchRecipientManagement(
        const Ptr<const Ieee80211MgmtHeader>& header, IRecipientActions& actions) const
{
    if (auto frame = dynamicPtrCast<const Ieee80211AddbaRequest>(header))
        // IEEE Std 802.11-2024, 10.25.2.
        actions.recipientAddbaRequest(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211AddbaResponse>(header))
        actions.recipientAddbaResponse(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211Delba>(header))
        actions.recipientDelba(frame);
}

const physicallayer::IIeee80211Mode *HcfFrameDispatchService::selectImmediateResponseMode(
        Packet *responsePacket, const Ptr<const Ieee80211MacHeader>& responseHeader,
        Packet *receivedPacket, const Ptr<const Ieee80211MacHeader>& receivedHeader,
        IResponseActions& actions) const
{
    if (auto frame = dynamicPtrCast<const Ieee80211RtsFrame>(receivedHeader))
        return actions.selectCtsResponseMode(receivedPacket, frame);
    if (auto frame = dynamicPtrCast<const Ieee80211BlockAckReq>(receivedHeader))
        return actions.selectBlockAckResponseMode(receivedPacket, frame);
    if (auto response = dynamicPtrCast<const Ieee80211BlockAck>(responseHeader))
        if (auto received = dynamicPtrCast<const Ieee80211DataHeader>(receivedHeader))
            return actions.selectImplicitBlockAckResponseMode(responsePacket,
                    receivedPacket, response, received);
    if (auto frame = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(receivedHeader))
        return actions.selectAckResponseMode(receivedPacket, frame);
    throw cRuntimeError("Unknown received frame type");
}

void HcfFrameDispatchService::dispatchTransmittedControlResponse(
        const Ptr<const Ieee80211MacHeader>& header, IResponseActions& actions) const
{
    if (auto frame = dynamicPtrCast<const Ieee80211CtsFrame>(header))
        actions.transmittedCtsResponse(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211BlockAck>(header))
        actions.transmittedBlockAckResponse(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211AckFrame>(header))
        actions.transmittedAckResponse(frame);
    else
        throw cRuntimeError("Unknown control response frame");
}

void HcfFrameDispatchService::dispatchTransmitted(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header, IOriginatorTransmitActions& actions) const
{
    if (header->getReceiverAddress().isMulticast())
        // IEEE Std 802.11-2024, 10.3.2.11.
        actions.transmittedGroup(packet, header);
    else if (auto data = dynamicPtrCast<const Ieee80211DataHeader>(header))
        dispatchTransmittedData(packet, data, actions);
    else if (auto management = dynamicPtrCast<const Ieee80211MgmtHeader>(header))
        dispatchTransmittedManagement(management, actions);
    else
        dispatchTransmittedControl(header, actions);
}

void HcfFrameDispatchService::dispatchTransmittedData(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header, IOriginatorTransmitActions& actions) const
{
    ExpectedResponse response = ExpectedResponse::NONE;
    if (header->getAckPolicy() == NORMAL_ACK)
        response = ExpectedResponse::ACK;
    else if (header->getAckPolicy() == BLOCK_ACK)
        response = ExpectedResponse::BLOCK_ACK;
    actions.transmittedData(packet, header, response);
}

void HcfFrameDispatchService::dispatchTransmittedManagement(
        const Ptr<const Ieee80211MgmtHeader>& header, IOriginatorTransmitActions& actions) const
{
    if (actions.isManagementAckNeeded(header))
        actions.transmittedManagementAck(header);
    if (auto frame = dynamicPtrCast<const Ieee80211AddbaRequest>(header))
        actions.transmittedAddbaRequest(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211AddbaResponse>(header))
        actions.transmittedAddbaResponse(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211Delba>(header))
        actions.transmittedDelba(frame);
}

void HcfFrameDispatchService::dispatchTransmittedControl(
        const Ptr<const Ieee80211MacHeader>& header, IOriginatorTransmitActions& actions) const
{
    if (auto frame = dynamicPtrCast<const Ieee80211BlockAckReq>(header))
        actions.transmittedBlockAckRequest(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211RtsFrame>(header))
        actions.transmittedRts(frame);
    else
        throw cRuntimeError("Unknown control frame");
}

void HcfFrameDispatchService::dispatchOriginatorFailure(
        Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header,
        IOriginatorTransmitActions& actions) const
{
    if (auto data = dynamicPtrCast<const Ieee80211DataHeader>(header))
        actions.failedDataOrManagement(packet, data,
                data->getAckPolicy() == BLOCK_ACK ? FailureKind::BLOCK_ACK_MISSING :
                FailureKind::ACK_TIMEOUT);
    else if (auto management = dynamicPtrCast<const Ieee80211MgmtHeader>(header))
        actions.failedDataOrManagement(packet, management, FailureKind::ACK_TIMEOUT);
    else if (auto blockAckRequest = dynamicPtrCast<const Ieee80211BlockAckReq>(header))
        actions.failedBlockAckRequest(packet, blockAckRequest);
    else
        throw cRuntimeError("Unknown frame");
}

void HcfFrameDispatchService::dispatchOriginatorReceived(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header,
        Packet *lastTransmittedPacket,
        const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader,
        IOriginatorReceiveActions& actions) const
{
    if (auto data = dynamicPtrCast<const Ieee80211DataHeader>(header))
        actions.originatorData(data, lastTransmittedHeader);
    else if (auto management = dynamicPtrCast<const Ieee80211MgmtHeader>(header))
        dispatchOriginatorReceivedManagement(management, actions);
    else
        dispatchOriginatorReceivedControl(packet, header, lastTransmittedPacket,
                lastTransmittedHeader, actions);
}

void HcfFrameDispatchService::dispatchOriginatorReceivedManagement(
        const Ptr<const Ieee80211MgmtHeader>& header, IOriginatorReceiveActions& actions) const
{
    if (auto frame = dynamicPtrCast<const Ieee80211AddbaResponse>(header))
        actions.originatorAddbaResponse(frame);
    else
        throw cRuntimeError("Unknown management frame");
}

void HcfFrameDispatchService::dispatchOriginatorReceivedControl(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header,
        Packet *lastTransmittedPacket,
        const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader,
        IOriginatorReceiveActions& actions) const
{
    if (auto frame = dynamicPtrCast<const Ieee80211AckFrame>(header))
        actions.originatorAck(packet, frame, lastTransmittedPacket,
                lastTransmittedHeader);
    else if (auto frame = dynamicPtrCast<const Ieee80211BlockAck>(header))
        actions.originatorBlockAck(frame);
    else if (auto frame = dynamicPtrCast<const Ieee80211CtsFrame>(header))
        actions.originatorCts(frame);
    else if (dynamicPtrCast<const Ieee80211RtsFrame>(header) ||
            header->getType() == ST_DATA_WITH_QOS ||
            dynamicPtrCast<const Ieee80211BlockAckReq>(header))
        actions.originatorIgnoredControl(header);
    else
        throw cRuntimeError("Unknown control frame");
}

} // namespace ieee80211
} // namespace inet
