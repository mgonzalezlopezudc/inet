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

namespace inet {

class Packet;

namespace ieee80211 {

class InProgressFrames;
class QosAckHandler;
class QosRecoveryProcedure;
class IRateControl;

/**
 * Applies the retry/recovery procedure for timed-out HT implicit BlockAck
 * members.
 *
 * Packet ownership, retry-limit signals, and EDCA/TXOP ownership remain with
 * Hcf and the existing recovery/queue collaborators. This service only
 * centralizes the member matching and per-member retry/retirement sequence.
 */
class INET_API HcfRetryService
{
  public:
    static std::vector<Packet *> recoverHtImplicitBlockAckTimeout(
            InProgressFrames *inProgressFrames, QosAckHandler *ackHandler,
            QosRecoveryProcedure *recoveryProcedure,
            IRateControl *rateControl,
            const std::set<std::pair<MacAddress,
                    std::pair<Tid, SequenceControlField>>>& failedFrameIds);
};

} // namespace ieee80211
} // namespace inet

#endif
