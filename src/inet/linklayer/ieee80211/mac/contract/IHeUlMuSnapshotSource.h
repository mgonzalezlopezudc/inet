//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHEULMUSNAPSHOTSOURCE_H
#define __INET_IHEULMUSNAPSHOTSOURCE_H

#include <array>
#include <optional>
#include <vector>

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h"

namespace inet {
namespace ieee80211 {

struct INET_API HeUlBufferStatusSnapshot
{
    MacAddress stationAddress;
    std::array<int64_t, 4> backlogBytes = {};
    std::array<Ieee80211HeQueueSizeEstimate, 4> backlogEstimates;
    std::array<uint8_t, 4> tid = {};
    simtime_t updateTime = SIMTIME_ZERO;
    simtime_t lastService = SIMTIME_ZERO;
};

/** Immutable AP-side view of one associated peer for one UL Trigger decision. */
struct INET_API HeUlPeerPreparationSnapshot
{
    MacAddress stationAddress;
    uint16_t associationId = 0;
    bool twtEligible = true;
    bool ulMuDisabled = false;
    std::optional<Ieee80211NegotiatedHeCapabilities> negotiatedCapabilities;
    double pathLossDb = NaN;
    bool hasFreshPathLoss = false;
    std::optional<HeUlBufferStatusSnapshot> bufferStatus;
};

/** Immutable inputs consumed by HeUlTriggerService after one EDCAF grant. */
struct INET_API HeUlPreparationSnapshot
{
    AccessCategory accessCategory = AC_BE;
    simtime_t now = SIMTIME_ZERO;
    std::optional<Ieee80211HeLinkPhySnapshot> phy;
    std::vector<HeUlPeerPreparationSnapshot> peers;
    simtime_t txopLimit = SIMTIME_ZERO;
    simtime_t maxHeTbPpduDuration = SIMTIME_ZERO;
    simtime_t reportMaxAge = SIMTIME_ZERO;
    double targetRssiMarginDb = 0;
    int maxMuStations = 0;
    bool enableUlMuMimo = false;
};

class INET_API IHeUlMuSnapshotSource
{
  public:
    virtual ~IHeUlMuSnapshotSource() {}
    virtual HeUlPreparationSnapshot captureHeUlPreparationSnapshot(
            AccessCategory accessCategory) const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
