//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFORIGINATORSERVICE_H
#define __INET_HCFORIGINATORSERVICE_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

class Packet;

namespace ieee80211 {

/**
 * Maps common HCF originator outcomes onto the existing ACK, recovery,
 * BlockAck, in-progress-frame, management-result, and rate-control owners.
 *
 * The service is stateless. Packet and header arguments are borrowed for the
 * duration of each call, and no action transfers Packet ownership. The action
 * owner authoritatively decides whether an identity is still current, which
 * makes late and duplicate callbacks no-ops without duplicating mutable retry
 * or BlockAck state here. Action exceptions are propagated; mutations already
 * committed by earlier actions are not rolled back.
 */
class INET_API HcfOriginatorService
{
  public:
    enum class FrameKind {
        DATA,
        MANAGEMENT,
    };

    enum class ExpectedResponse {
        NONE,
        ACK,
        BLOCK_ACK,
    };

    enum class FailureKind {
        ACK_TIMEOUT,
        BLOCK_ACK_MISSING,
    };

    enum class ManagementResultKind {
        ACKNOWLEDGED,
        RETRY_LIMIT_REACHED,
    };

    enum class TerminalAction {
        NONE,
        RETAIN_RETRY,
        DROP_RETIRE,
        ACK_RETIRE,
    };

    enum class Disposition {
        PROCESSED,
        STALE_OR_DUPLICATE,
    };

    enum class BlockAckMemberStatus {
        ACKNOWLEDGED,
        UNACKNOWLEDGED,
        NOT_COVERED,
    };

    enum class BlockAckResultKind {
        FULL,
        PARTIAL,
        NONE_ACKNOWLEDGED,
        NO_CURRENT_MEMBERS,
    };

    struct FrameIdentity {
        const Packet *packet = nullptr;
        MacAddress receiverAddress;
        int tid = -1;
        int sequenceNumber = -1;
        int fragmentNumber = -1;

        bool operator==(const FrameIdentity& other) const;
        bool operator!=(const FrameIdentity& other) const { return !(*this == other); }
    };

    struct Frame {
        Packet *packet = nullptr;
        Ptr<const Ieee80211DataOrMgmtHeader> header;
        FrameIdentity identity;
        FrameKind kind = FrameKind::DATA;
    };

    struct Result {
        Disposition disposition = Disposition::PROCESSED;
        TerminalAction terminalAction = TerminalAction::NONE;
        int retryCount = 0;
    };

    struct BlockAckResult {
        Disposition disposition = Disposition::PROCESSED;
        BlockAckResultKind kind = BlockAckResultKind::NO_CURRENT_MEMBERS;
        unsigned int acknowledgedCount = 0;
        unsigned int unacknowledgedCount = 0;
        unsigned int notCoveredCount = 0;
        unsigned int staleCount = 0;
    };

    class IActions
    {
      public:
        virtual ~IActions() {}

        // This query consults the authoritative in-progress/ACK owner.
        virtual bool isCurrent(const FrameIdentity& identity) const noexcept = 0;

        virtual void processTransmitted(const Frame& frame,
                ExpectedResponse expectedResponse) = 0;
        virtual void processNoResponseSuccess(const Frame& frame) = 0;

        virtual void processTransmissionFailed(const Frame& frame,
                FailureKind failureKind) = 0;
        virtual bool isRetryLimitReached(const Frame& frame) = 0;
        virtual int getRetryCount(const Frame& frame) = 0;
        virtual void reportRateResult(const Frame& frame, int retryCount,
                bool successful, bool retryLimitReached) = 0;
        virtual void processAckStateFailed(const Frame& frame) = 0;
        virtual void processRetryLimitReached(const Frame& frame) = 0;
        virtual void markRetry(const Frame& frame) = 0;

        virtual void processAckRecoverySuccess(const Frame& frame) = 0;
        virtual void processAckStateReceived(const Frame& frame,
                const Ptr<const Ieee80211AckFrame>& ackFrame) = 0;

        virtual void processBlockAckReceived(
                const Ptr<const Ieee80211BlockAck>& blockAck) = 0;
        virtual BlockAckMemberStatus getBlockAckMemberStatus(
                const Frame& frame) = 0;
        virtual void processBlockAckMemberResult(const Frame& frame,
                BlockAckMemberStatus status) = 0;
        virtual void processBlockAckAgreement(
                const Ptr<const Ieee80211BlockAck>& blockAck) = 0;

        virtual void retireInProgress(const Frame& frame) = 0;
        virtual void retireAckState(const Frame& frame) = 0;
        virtual void reportManagementResult(const Frame& frame,
                ManagementResultKind resultKind) = 0;
    };

  private:
    static void validateFrame(const Frame& frame);
    static Result staleResult();
    Result processFailure(const Frame& frame, FailureKind failureKind,
            IActions& actions) const;

  public:
    static Frame makeFrame(Packet *packet,
            const Ptr<const Ieee80211DataOrMgmtHeader>& header);

    Result processTransmitted(const Frame& frame,
            ExpectedResponse expectedResponse, IActions& actions) const;
    Result processAckReceived(const Frame& frame,
            const Ptr<const Ieee80211AckFrame>& ackFrame,
            IActions& actions) const;
    Result processAckTimeout(const Frame& frame, IActions& actions) const;
    Result processBlockAckMissing(const Frame& frame, IActions& actions) const;
    BlockAckResult processBlockAckReceived(
            const Ptr<const Ieee80211BlockAck>& blockAck,
            const std::vector<Frame>& candidateFrames, IActions& actions) const;
};

} // namespace ieee80211
} // namespace inet

#endif
