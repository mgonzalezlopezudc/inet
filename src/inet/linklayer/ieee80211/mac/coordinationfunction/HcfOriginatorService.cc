//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfOriginatorService.h"

#include "inet/common/packet/Packet.h"

namespace inet {
namespace ieee80211 {

bool HcfOriginatorService::FrameIdentity::operator==(
        const FrameIdentity& other) const
{
    return packet == other.packet && receiverAddress == other.receiverAddress &&
            tid == other.tid && sequenceNumber == other.sequenceNumber &&
            fragmentNumber == other.fragmentNumber;
}

void HcfOriginatorService::validateFrame(const Frame& frame)
{
    if (frame.packet == nullptr || frame.header == nullptr)
        throw cRuntimeError("HCF originator service requires a packet and data or management header");
    if (frame.identity.packet != frame.packet)
        throw cRuntimeError("HCF originator identity does not refer to the submitted packet");
    auto packetHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
            frame.packet->peekAtFront<Ieee80211MacHeader>());
    if (packetHeader.get() != frame.header.get())
        throw cRuntimeError("HCF originator header does not belong to the submitted packet");
    auto expectedFrame = makeFrame(frame.packet, frame.header);
    if (expectedFrame.kind != frame.kind || expectedFrame.identity != frame.identity)
        throw cRuntimeError("HCF originator frame identity does not match its MAC header");
}

HcfOriginatorService::Result HcfOriginatorService::staleResult()
{
    Result result;
    result.disposition = Disposition::STALE_OR_DUPLICATE;
    return result;
}

HcfOriginatorService::Frame HcfOriginatorService::makeFrame(Packet *packet,
        const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    if (packet == nullptr || header == nullptr)
        throw cRuntimeError("HCF originator service requires a packet and data or management header");
    auto packetHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
            packet->peekAtFront<Ieee80211MacHeader>());
    if (packetHeader.get() != header.get())
        throw cRuntimeError("HCF originator header does not belong to the submitted packet");

    Frame frame;
    frame.packet = packet;
    frame.header = header;
    frame.identity.packet = packet;
    frame.identity.receiverAddress = header->getReceiverAddress();
    frame.identity.sequenceNumber = header->getSequenceNumber().isValid() ?
            header->getSequenceNumber().get() : -1;
    frame.identity.fragmentNumber = header->getFragmentNumber();
    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
        frame.kind = FrameKind::DATA;
        frame.identity.tid = dataHeader->getTid();
    }
    else {
        frame.kind = FrameKind::MANAGEMENT;
        frame.identity.tid = -1;
    }
    return frame;
}

HcfOriginatorService::Result HcfOriginatorService::processTransmitted(
        const Frame& frame, ExpectedResponse expectedResponse,
        IActions& actions) const
{
    validateFrame(frame);
    if (!actions.isCurrent(frame.identity))
        return staleResult();

    actions.processTransmitted(frame, expectedResponse);
    Result result;
    // IEEE Std 802.11-2024, 10.3.2.11: an MPDU that does not solicit an
    // immediate response completes without entering the ACK retry path.
    if (expectedResponse == ExpectedResponse::NONE &&
            frame.kind == FrameKind::DATA) {
        actions.processNoResponseSuccess(frame);
        actions.retireInProgress(frame);
        result.terminalAction = TerminalAction::ACK_RETIRE;
    }
    return result;
}

HcfOriginatorService::Result HcfOriginatorService::processAckReceived(
        const Frame& frame, const Ptr<const Ieee80211AckFrame>& ackFrame,
        IActions& actions) const
{
    validateFrame(frame);
    if (ackFrame == nullptr)
        throw cRuntimeError("HCF originator ACK result requires an ACK frame");
    if (!actions.isCurrent(frame.identity))
        return staleResult();

    // IEEE Std 802.11-2024, 10.3.2.11, 10.23.2.2 and 10.3.4.4:
    // successful ACK feedback precedes retry-state reset and retirement.
    Result result;
    result.retryCount = frame.kind == FrameKind::MANAGEMENT || frame.header->getRetry() ?
            actions.getRetryCount(frame) : 0;
    actions.reportRateResult(frame, result.retryCount, true, false);
    actions.processAckRecoverySuccess(frame);
    actions.processAckStateReceived(frame, ackFrame);
    if (frame.kind == FrameKind::MANAGEMENT)
        actions.reportManagementResult(frame, ManagementResultKind::ACKNOWLEDGED);
    actions.retireInProgress(frame);
    actions.retireAckState(frame);
    result.terminalAction = TerminalAction::ACK_RETIRE;
    return result;
}

HcfOriginatorService::Result HcfOriginatorService::processFailure(
        const Frame& frame, FailureKind failureKind, IActions& actions) const
{
    validateFrame(frame);
    if (!actions.isCurrent(frame.identity))
        return staleResult();

    // IEEE Std 802.11-2024, 10.23.2.2 and 10.3.4.4: failed data and
    // management exchanges update recovery state before retry/give-up.
    actions.processTransmissionFailed(frame, failureKind);
    bool retryLimitReached = actions.isRetryLimitReached(frame);
    Result result;
    result.retryCount = actions.getRetryCount(frame);
    actions.reportRateResult(frame, result.retryCount, false, retryLimitReached);
    actions.processAckStateFailed(frame);
    if (retryLimitReached) {
        actions.processRetryLimitReached(frame);
        actions.retireInProgress(frame);
        actions.retireAckState(frame);
        if (frame.kind == FrameKind::MANAGEMENT)
            actions.reportManagementResult(frame,
                    ManagementResultKind::RETRY_LIMIT_REACHED);
        result.terminalAction = TerminalAction::DROP_RETIRE;
    }
    else {
        actions.markRetry(frame);
        result.terminalAction = TerminalAction::RETAIN_RETRY;
    }
    return result;
}

HcfOriginatorService::Result HcfOriginatorService::processAckTimeout(
        const Frame& frame, IActions& actions) const
{
    return processFailure(frame, FailureKind::ACK_TIMEOUT, actions);
}

HcfOriginatorService::Result HcfOriginatorService::processBlockAckMissing(
        const Frame& frame, IActions& actions) const
{
    return processFailure(frame, FailureKind::BLOCK_ACK_MISSING, actions);
}

HcfOriginatorService::BlockAckResult HcfOriginatorService::processBlockAckReceived(
        const Ptr<const Ieee80211BlockAck>& blockAck,
        const std::vector<Frame>& candidateFrames, IActions& actions) const
{
    if (blockAck == nullptr)
        throw cRuntimeError("HCF originator BlockAck result requires a BlockAck frame");
    for (unsigned int i = 0; i < candidateFrames.size(); i++) {
        validateFrame(candidateFrames[i]);
        for (unsigned int j = 0; j < i; j++)
            if (candidateFrames[i].identity == candidateFrames[j].identity)
                throw cRuntimeError("HCF originator BlockAck candidates contain a duplicate MPDU identity");
    }

    BlockAckResult result;
    std::vector<const Frame *> currentFrames;
    for (const auto& frame : candidateFrames) {
        if (actions.isCurrent(frame.identity))
            currentFrames.push_back(&frame);
        else
            result.staleCount++;
    }
    if (currentFrames.empty()) {
        result.disposition = Disposition::STALE_OR_DUPLICATE;
        return result;
    }

    // IEEE Std 802.11-2024, 10.25.3 and 10.25.6.8: BlockAck completes
    // acknowledged MPDUs while covered-unacknowledged MPDUs remain for retry.
    actions.processBlockAckReceived(blockAck);
    std::vector<const Frame *> acknowledgedFrames;
    for (auto frame : currentFrames) {
        auto status = actions.getBlockAckMemberStatus(*frame);
        if (status == BlockAckMemberStatus::ACKNOWLEDGED) {
            result.acknowledgedCount++;
            acknowledgedFrames.push_back(frame);
        }
        else if (status == BlockAckMemberStatus::UNACKNOWLEDGED) {
            result.unacknowledgedCount++;
            actions.processTransmissionFailed(*frame,
                    FailureKind::BLOCK_ACK_MISSING);
        }
        else
            result.notCoveredCount++;
        actions.processBlockAckMemberResult(*frame, status);
    }
    actions.processBlockAckAgreement(blockAck);
    for (auto frame : acknowledgedFrames) {
        actions.retireInProgress(*frame);
        actions.retireAckState(*frame);
    }

    if (result.acknowledgedCount == 0)
        result.kind = BlockAckResultKind::NONE_ACKNOWLEDGED;
    else if (result.unacknowledgedCount == 0 && result.notCoveredCount == 0)
        result.kind = BlockAckResultKind::FULL;
    else
        result.kind = BlockAckResultKind::PARTIAL;
    return result;
}

} // namespace ieee80211
} // namespace inet
