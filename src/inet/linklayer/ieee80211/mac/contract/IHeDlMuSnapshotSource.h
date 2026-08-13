//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHEDLMUSNAPSHOTSOURCE_H
#define __INET_IHEDLMUSNAPSHOTSOURCE_H

#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"

namespace inet {
namespace ieee80211 {

/** Immutable value snapshot of one queued DL MU candidate packet. */
struct INET_API HeDlMuCandidateSnapshot
{
    HcfPacketIdentity packetIdentity;
    HcfQueueToken queueToken;
    MacAddress queuePeer;
    int queueIndex = -1;
    MacAddress peer;
    AccessCategory accessCategory = AC_BE;
    Tid tid = 0;
    int64_t packetBytes = 0;
    simtime_t enqueueTime = SIMTIME_ZERO;
    bool qosData = false;
    bool unicast = false;
    bool sequenceNumberValid = false;
    bool retryEligible = false;
    bool activeBlockAck = false;
    bool addbaRequired = false;
    bool addbaRequestInProgress = false;
    int blockAckBufferSize = 0;
    int occupiedBlockAckSlots = 0;
    bool twtEligible = true;
    bool hasAdvertisement = false;
    Ieee80211HeCapabilities advertisement;
    bool hasNegotiatedCapabilities = false;
    Ieee80211NegotiatedHeCapabilities negotiatedCapabilities;
    int operatingModeRxNss = 0;
    bool hasFreshCsi = false;
    double pathLossDb = NaN;
    bool hasFreshPathLoss = false;
};

/** One grant-boundary snapshot; contains values only, never queues/managers. */
struct INET_API HeDlMuPreparationSnapshot
{
    AccessCategory accessCategory = AC_BE;
    simtime_t now = SIMTIME_ZERO;
    IIeee80211HeDlScheduler::ScheduleContext common;
    bool sequentialBar = false;
    bool localLdpc = false;
    bool heAccessPoint = false;
    bool localDlMuMimoBeamformer = false;
    bool hasRecoveryFrame = false;
    bool pendingQueueEmpty = true;
    bool pendingHeadMuEligible = false;
    bool hasSingleUserFrameToTransmit = false;
    std::vector<HeDlMuCandidateSnapshot> packets;
};

class INET_API IHeDlMuSnapshotSource
{
  public:
    virtual ~IHeDlMuSnapshotSource() {}
    virtual HeDlMuPreparationSnapshot captureHeDlMuPreparationSnapshot(
            AccessCategory accessCategory) const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
