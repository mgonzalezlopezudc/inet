//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFAGGREGATIONSERVICE_H
#define __INET_HCFAGGREGATIONSERVICE_H

#include <optional>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/common/FcsMode_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/AmpduTransmissionLedger.h"

namespace inet {

class Packet;

namespace ieee80211 {

/**
 * Plans and builds HCF A-MPDUs and keeps the non-owning
 * transmission-to-constituent mapping used after transmission.
 *
 * The service does not own packets, decide the call-context gate that allows
 * aggregation, or emit HCF signals. Candidate discovery and baseline staged
 * mutations remain explicit owner actions; therefore planning is deliberately
 * nontransactional when an action or the subsequent PHY-duration check fails.
 */
class INET_API HcfAggregationService
{
  public:
    struct TransmissionPlanningRequest {
        Packet *sourcePacket = nullptr;
        const physicallayer::IIeee80211Mode *mode = nullptr;
        physicallayer::Ieee80211PhyFamily phyFamily = physicallayer::Ieee80211PhyFamily::UNSPECIFIED;
        bool aggregationAllowed = false;
        bool implicitBlockAck = false;
    };

    struct TransmissionPlan {
        std::vector<Packet *> members;
        bool materialize = false;
        bool implicitBlockAck = false;
    };

    class ITransmissionPlanningActions {
      public:
        virtual ~ITransmissionPlanningActions() {}
        virtual std::vector<Packet *> getCandidates(Packet *sourcePacket,
                bool implicitBlockAck, long long maxAggregateLength) const = 0;
        virtual long long getMaxAggregateLength(
                const Ptr<const Ieee80211DataHeader>& sourceHeader,
                physicallayer::Ieee80211PhyFamily phyFamily) const = 0;
        virtual void applyRetryState(Packet *candidate) const = 0;
        virtual AckPolicy selectAckPolicy(Packet *candidate,
                const Ptr<const Ieee80211DataHeader>& header) const = 0;
        virtual void applySelectedPolicy(Packet *candidate,
                AckPolicy ackPolicy) const = 0;
        virtual void aggregationTrimmed(size_t originalCount,
                size_t retainedCount, simtime_t durationLimit) const = 0;
    };

    struct HtImplicitSelectionRequest {
        Packet *sourcePacket = nullptr;
        const physicallayer::IIeee80211Mode *mode = nullptr;
        physicallayer::Ieee80211PhyFamily phyFamily = physicallayer::Ieee80211PhyFamily::UNSPECIFIED;
        bool enabled = false;
    };

    class IHtImplicitSelectionActions {
      public:
        virtual ~IHtImplicitSelectionActions() {}
        virtual std::vector<Packet *> getCandidates(Packet *sourcePacket) const = 0;
        virtual long long getMaxAggregateLength(
                const Ptr<const Ieee80211DataHeader>& sourceHeader,
                physicallayer::Ieee80211PhyFamily phyFamily) const = 0;
        virtual AckPolicy selectAckPolicy(Packet *candidate,
                const Ptr<const Ieee80211DataHeader>& header) const = 0;
    };

  private:
    AmpduTransmissionLedger transmissionLedger;
    static Packet *buildAmpduPacket(const std::vector<Packet *>& frames,
            FcsMode fcsMode);

  public:
    static B calculateAmpduLength(const std::vector<Packet *>& frames);
    TransmissionPlan planTransmission(const TransmissionPlanningRequest& request,
            const ITransmissionPlanningActions& actions) const;
    std::vector<Packet *> selectHtImplicitBlockAckFrames(
            const HtImplicitSelectionRequest& request,
            const IHtImplicitSelectionActions& actions) const;

    Packet *materializeTransmission(Packet *ledgerKey,
            const std::vector<Packet *>& subframes, FcsMode fcsMode,
            bool implicitBlockAck);
    bool hasImplicitBlockAck(Packet *packet) const;
    std::optional<AmpduTransmissionLedger::Entry> takeTransmission(Packet *packet);
    bool discardTransmission(Packet *packet);
};

} // namespace ieee80211
} // namespace inet

#endif
