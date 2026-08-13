//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.h"

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/QosRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/queue/InProgressFrames.h"

namespace inet {
namespace ieee80211 {

HcfRetryService::Result HcfRetryService::processFailure(
        const HcfOriginatorService::Frame& frame,
        HcfOriginatorService::FailureKind failureKind, FailurePath path,
        IActions& actions) const
{
    Result result;
    if (!actions.isCurrent(frame.identity)) {
        result.disposition = HcfOriginatorService::Disposition::STALE_OR_DUPLICATE;
        return result;
    }

    // IEEE Std 802.11-2024, 10.23.2.2, 10.23.2.4 and 10.3.2.9:
    // the existing recovery owners update their counters before retry-limit
    // evaluation; this stateless facade only sequences those owner actions.
    bool retryLimitReached;
    if (path == FailurePath::RTS_PROTECTION) {
        actions.processRtsFailure(frame);
        retryLimitReached = actions.isRtsRetryLimitReached(frame);
    }
    else {
        actions.processTransmissionFailed(frame, failureKind);
        retryLimitReached = actions.isRetryLimitReached(frame);
    }

    if (path == FailurePath::ORIGINATOR) {
        result.retryCount = actions.getRetryCount(frame);
        actions.reportRateResult(frame, result.retryCount, false,
                retryLimitReached);
        actions.processAckStateFailed(frame);
    }

    if (retryLimitReached) {
        if (path == FailurePath::INTERNAL_COLLISION)
            actions.reportDropping(frame, path);
        actions.processRetryLimitReached(frame);
        actions.retireInProgress(frame);
        actions.retireAckState(frame);
        if (path != FailurePath::INTERNAL_COLLISION)
            actions.reportDropping(frame, path);
        actions.observePacketDropped(frame);
        if (frame.kind == HcfOriginatorService::FrameKind::MANAGEMENT)
            actions.reportManagementResult(frame,
                    HcfOriginatorService::ManagementResultKind::RETRY_LIMIT_REACHED);
        actions.observeLinkBroken(frame);
        result.terminalAction = HcfOriginatorService::TerminalAction::DROP_RETIRE;
    }
    else {
        actions.reportRetainingRetry(frame, path);
        if (path == FailurePath::ORIGINATOR)
            actions.markRetry(frame);
        result.terminalAction = HcfOriginatorService::TerminalAction::RETAIN_RETRY;
    }
    return result;
}

void HcfRetryService::recoverBlockAckRequestFailure(
        InProgressFrames *inProgressFrames,
        QosRecoveryProcedure *recoveryProcedure,
        const std::set<std::pair<MacAddress,
                std::pair<Tid, SequenceControlField>>>& failedFrameIds,
        bool requireValidSequenceNumber)
{
    for (int i = 0; i < inProgressFrames->getLength(); i++) {
        auto packet = inProgressFrames->getFrames(i);
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || header->getType() != ST_DATA_WITH_QOS ||
                (requireValidSequenceNumber &&
                        !header->getSequenceNumber().isValid()))
            continue;
        auto id = std::make_pair(header->getReceiverAddress(),
                std::make_pair(header->getTid(), SequenceControlField(
                        header->getSequenceNumber().get(),
                        header->getFragmentNumber())));
        if (failedFrameIds.count(id) != 0)
            recoveryProcedure->dataFrameTransmissionFailed(packet, header);
    }
}

std::vector<Packet *> HcfRetryService::recoverHtImplicitBlockAckTimeout(
        InProgressFrames *inProgressFrames, QosAckHandler *ackHandler,
        QosRecoveryProcedure *recoveryProcedure, IRateControl *rateControl,
        const std::set<std::pair<MacAddress,
                std::pair<Tid, SequenceControlField>>>& failedFrameIds)
{
    std::vector<Packet *> failedFrames;
    for (int i = 0; i < inProgressFrames->getLength(); i++) {
        auto frame = inProgressFrames->getFrames(i);
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                frame->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || !header->getSequenceNumber().isValid())
            continue;
        auto id = std::make_pair(header->getReceiverAddress(),
                std::make_pair(header->getTid(), SequenceControlField(
                        header->getSequenceNumber().get(),
                        header->getFragmentNumber())));
        if (failedFrameIds.count(id) != 0)
            failedFrames.push_back(frame);
    }

    std::vector<Packet *> retiredFrames;
    for (auto frame : failedFrames) {
        auto header = frame->peekAtFront<Ieee80211DataHeader>();
        recoveryProcedure->dataFrameTransmissionFailed(frame, header);
        bool retryLimitReached =
                recoveryProcedure->isRetryLimitReached(frame, header);
        int retryCount = recoveryProcedure->getRetryCount(frame, header);
        if (rateControl)
            rateControl->frameTransmitted(frame, retryCount, false,
                    retryLimitReached);
        if (retryLimitReached) {
            recoveryProcedure->retryLimitReached(frame, header);
            inProgressFrames->dropFrame(frame);
            ackHandler->dropFrame(header);
            retiredFrames.push_back(frame);
        }
        else {
            auto mutableHeader =
                    frame->removeAtFront<Ieee80211DataHeader>();
            mutableHeader->setRetry(true);
            frame->insertAtFront(mutableHeader);
        }
    }
    return retiredFrames;
}

void HcfRetryService::prepareTriggeredUlRetry(Packet *packet,
        QosRecoveryProcedure *recoveryProcedure)
{
    if (packet == nullptr || recoveryProcedure == nullptr)
        throw cRuntimeError("Cannot prepare an incomplete triggered UL retry");
    auto header = packet->peekAtFront<Ieee80211DataHeader>();
    recoveryProcedure->dataFrameTransmissionFailed(packet, header);
    auto writableHeader = packet->removeAtFront<Ieee80211DataHeader>();
    writableHeader->setRetry(true);
    packet->insertAtFront(writableHeader);
}

} // namespace ieee80211
} // namespace inet
