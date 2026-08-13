//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFRETRYSERVICE_H
#define __INET_HCFRETRYSERVICE_H

#include <set>
#include <utility>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/common/SequenceControlField.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfOriginatorService.h"

namespace inet {

class Packet;

namespace ieee80211 {

class InProgressFrames;
class QosAckHandler;
class QosRecoveryProcedure;
class IRateControl;

/**
 * Stateless sequencing facade for common HCF retry outcomes.
 *
 * Recovery counters, ACK state, BlockAck state, in-progress packets, rate
 * control state, and EDCA/TXOP state remain owned by their existing
 * collaborators. The facade holds no mutable protocol state; it validates
 * current packet identity through IActions and sequences narrow owner actions,
 * terminal ownership, management results, and observations. The compatibility
 * HT implicit BlockAck entry point retains its specialized member selection.
 */
class INET_API HcfRetryService
{
  public:
    enum class FailurePath {
        ORIGINATOR,
        INTERNAL_COLLISION,
        RTS_PROTECTION,
    };

    struct Result {
        HcfOriginatorService::Disposition disposition = HcfOriginatorService::Disposition::PROCESSED;
        HcfOriginatorService::TerminalAction terminalAction = HcfOriginatorService::TerminalAction::NONE;
        int retryCount = 0;
    };

    class INET_API IActions
    {
      public:
        virtual ~IActions() = default;
        virtual bool isCurrent(
                const HcfOriginatorService::FrameIdentity& identity) const noexcept = 0;
        virtual void processTransmissionFailed(
                const HcfOriginatorService::Frame& frame,
                HcfOriginatorService::FailureKind failureKind) = 0;
        virtual void processRtsFailure(const HcfOriginatorService::Frame& frame) = 0;
        virtual bool isRetryLimitReached(
                const HcfOriginatorService::Frame& frame) = 0;
        virtual bool isRtsRetryLimitReached(const HcfOriginatorService::Frame& frame) = 0;
        virtual int getRetryCount(const HcfOriginatorService::Frame& frame) = 0;
        virtual void reportRateResult(const HcfOriginatorService::Frame& frame,
                int retryCount, bool successful, bool retryLimitReached) = 0;
        virtual void processAckStateFailed(
                const HcfOriginatorService::Frame& frame) = 0;
        virtual void processRetryLimitReached(
                const HcfOriginatorService::Frame& frame) = 0;
        virtual void markRetry(const HcfOriginatorService::Frame& frame) = 0;
        virtual void retireInProgress(
                const HcfOriginatorService::Frame& frame) = 0;
        virtual void retireAckState(
                const HcfOriginatorService::Frame& frame) = 0;
        virtual void reportManagementResult(
                const HcfOriginatorService::Frame& frame,
                HcfOriginatorService::ManagementResultKind resultKind) = 0;
        virtual void reportRetainingRetry(const HcfOriginatorService::Frame& frame,
                FailurePath path) = 0;
        virtual void reportDropping(const HcfOriginatorService::Frame& frame,
                FailurePath path) = 0;
        virtual void observePacketDropped(const HcfOriginatorService::Frame& frame) = 0;
        virtual void observeLinkBroken(const HcfOriginatorService::Frame& frame) = 0;
    };

    Result processFailure(const HcfOriginatorService::Frame& frame,
            HcfOriginatorService::FailureKind failureKind, FailurePath path,
            IActions& actions) const;

    static void recoverBlockAckRequestFailure(InProgressFrames *inProgressFrames,
            QosRecoveryProcedure *recoveryProcedure,
            const std::set<std::pair<MacAddress,
                    std::pair<Tid, SequenceControlField>>>& failedFrameIds,
            bool requireValidSequenceNumber);

    static std::vector<Packet *> recoverHtImplicitBlockAckTimeout(
            InProgressFrames *inProgressFrames, QosAckHandler *ackHandler,
            QosRecoveryProcedure *recoveryProcedure,
            IRateControl *rateControl,
            const std::set<std::pair<MacAddress,
                    std::pair<Tid, SequenceControlField>>>& failedFrameIds);

    static void prepareTriggeredUlRetry(Packet *packet,
            QosRecoveryProcedure *recoveryProcedure);
};

} // namespace ieee80211
} // namespace inet

#endif
