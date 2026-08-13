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
 * Shared specialized retry helpers used by HCF and HE triggered uplink.
 */
class INET_API HcfRetryService
{
  public:
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
