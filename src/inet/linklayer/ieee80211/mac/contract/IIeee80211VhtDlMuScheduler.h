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
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"

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
        HcfQueueToken sourceQueueToken;
        HcfPacketIdentity packetIdentity;
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

    /** Candidate indices refer to the immutable provider snapshot. */
    struct CandidateSnapshot {
        unsigned int candidateIndex = 0;
        Candidate candidate;

        explicit CandidateSnapshot(unsigned int candidateIndex, const Candidate& source) :
            candidateIndex(candidateIndex), candidate(source)
        {}
    };

    struct SchedulingContext {
        Context common;
        std::vector<CandidateSnapshot> candidates;

        explicit SchedulingContext(const Context& source) : common(source)
        {
            common.candidates.clear();
            for (size_t i = 0; i < source.candidates.size(); ++i)
                candidates.emplace_back(i, source.candidates[i]);
        }
    };

    virtual ~IIeee80211VhtDlMuScheduler() {}
  private:
    static bool isEligible(const Context& context, const Candidate& candidate,
            bool requireOwnershipIdentities)
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
                (!requireOwnershipIdentities ||
                 (candidate.sourceQueueToken.isValid() &&
                  candidate.packetIdentity.isValid())) &&
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
  public:
    static bool isEligible(const Context& context, const Candidate& candidate)
    {
        return isEligible(context, candidate, true);
    }
    static bool isEligible(const SchedulingContext& context,
            const CandidateSnapshot& candidate)
    {
        return isEligible(context.common, candidate.candidate, false);
    }
    virtual std::vector<unsigned int> schedule(const SchedulingContext& context) const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
