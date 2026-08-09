//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211VHTDLMUSCHEDULER_H
#define __INET_IIEEE80211VHTDLMUSCHEDULER_H

#include <cmath>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

class INET_API IIeee80211VhtDlMuScheduler
{
  public:
    struct Candidate {
        MacAddress peer;
        uint16_t associationId = 0;
        Tid tid = 0;
        uint64_t associationGeneration = 0;
        uint8_t userPosition = 0;
        int numberOfSpatialStreams = 1;
        int mcs = 0;
        bool ldpc = false;
        simtime_t enqueueTime = SIMTIME_ZERO;
        queueing::IPacketQueue *sourceQueue = nullptr;
        Packet *packet = nullptr;
        B psduLength = B(0);
        double beamformingGainDb = 0;
        double leakagePenaltyDb = 0;
        int soundingNsts = 0;
        int receiverMaxNstsTotal = 0;
        bool associated = false;
        bool negotiatedMuMimo = false;
        bool exactlyOneSpatialStream = false;
        bool freshCsi = false;
        bool activeGroup = false;
        bool activeBlockAckAgreement = false;
        bool unsegmented = false;
    };

    struct Context {
        bool enabled = false;
        bool accessPoint = false;
        bool packetLevelRadio = false;
        Hz channelWidth = Hz(0);
        int transmitDimensions = 0;
        int maxNstsTotal = 8;
        bool shortGi = false;
        uint8_t groupId = 1;
        std::vector<Candidate> candidates;
    };

    virtual ~IIeee80211VhtDlMuScheduler() {}
    static bool isEligible(const Context& context, const Candidate& candidate)
    {
        return context.enabled && context.accessPoint && context.packetLevelRadio &&
                (context.channelWidth == MHz(20) || context.channelWidth == MHz(40) ||
                 context.channelWidth == MHz(80) || context.channelWidth == MHz(160)) &&
                context.transmitDimensions >= 2 && context.transmitDimensions <= 8 &&
                context.maxNstsTotal >= 2 && context.maxNstsTotal <= 8 &&
                context.groupId >= 1 && context.groupId <= 62 && !candidate.peer.isMulticast() &&
                !candidate.peer.isUnspecified() && candidate.associationId > 0 &&
                candidate.associationGeneration > 0 &&
                candidate.userPosition < 4 && candidate.numberOfSpatialStreams >= 1 &&
                candidate.numberOfSpatialStreams <= 4 &&
                candidate.mcs >= 0 && candidate.mcs <= 9 &&
                candidate.sourceQueue != nullptr && candidate.packet != nullptr &&
                candidate.psduLength > B(0) &&
                std::isfinite(candidate.beamformingGainDb) && candidate.beamformingGainDb >= 0 &&
                std::isfinite(candidate.leakagePenaltyDb) && candidate.leakagePenaltyDb >= 0 &&
                candidate.soundingNsts >= candidate.numberOfSpatialStreams && candidate.soundingNsts <= 8 &&
                candidate.receiverMaxNstsTotal >= candidate.numberOfSpatialStreams &&
                candidate.receiverMaxNstsTotal <= 8 &&
                candidate.associated && candidate.negotiatedMuMimo &&
                candidate.freshCsi &&
                candidate.activeGroup && candidate.activeBlockAckAgreement &&
                candidate.unsegmented;
    }
    virtual std::vector<Candidate> schedule(const Context& context) const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
