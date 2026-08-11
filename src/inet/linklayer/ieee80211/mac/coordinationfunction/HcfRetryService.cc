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

} // namespace ieee80211
} // namespace inet
