//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFTRANSMISSIONPREPARATIONSERVICE_H
#define __INET_HCFTRANSMISSIONPREPARATIONSERVICE_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

class Packet;

namespace physicallayer {
class IIeee80211Mode;
}

namespace ieee80211 {

/**
 * Orders common HCF transmission preparation without owning protocol state.
 *
 * The source packet, headers, candidate MPDUs, selected mode, and transmitted
 * packet are borrowed for the duration documented by the action call. The
 * existing rate selection, aggregation, ACK/BlockAck, and protection owners
 * remain authoritative. In particular, mode legality and PPDU duration are
 * validated by the selected PHY mode through IActions, and protection duration
 * is computed by the existing SingleProtectionMechanism through IActions.
 *
 * prepare() structurally validates the request before staged owner mutations.
 * Retry state is materialized before retry-sensitive policy and rate selection,
 * and production aggregation planning may move candidates from pending to
 * in-progress state. A later failure does not roll those baseline owner
 * mutations back. Temporary aggregate ownership remains transactional.
 * The source packet is never transferred. A temporary transmitted packet is
 * deleted exactly once after handoff or after a failed handoff. If
 * materializeAggregate() throws, it must clean up any allocation that it did
 * not return.
 */
class INET_API HcfTransmissionPreparationService
{
  public:
    enum class ProtectionMechanism {
        SINGLE,
        MULTIPLE,
    };

    struct Request {
        Packet *packet = nullptr;
        // Must be packet's exact front MAC header unless container is true;
        // container headers may be synthesized from typed metadata.
        Ptr<const Ieee80211MacHeader> header;
        simtime_t ifs;
        ProtectionMechanism protectionMechanism = ProtectionMechanism::SINGLE;
        bool container = false;
        bool durationFinalized = false;
        // Trigger and Multi-STA BlockAck Duration is exempt only from the
        // common SINGLE-protection calculation. MULTIPLE protection retains
        // the baseline Duration update unless durationFinalized is set.
        bool durationExemptForSingleProtection = false;
    };

    struct AggregatePlan {
        std::vector<Packet *> members;
        bool materialize = false;
        bool implicitBlockAck = false;
    };

    struct ProtectionPlan {
        bool updateDuration = false;
        simtime_t duration;
    };

    class IActions
    {
      public:
        virtual ~IActions() {}

        // Eligibility itself precedes retry materialization.
        virtual bool isHtImplicitBlockAckEligible(const Request& request) const = 0;
        // Baseline staged mutation: retry-sensitive policy/rate owners consume
        // the updated source header.
        virtual void applySourceRetryState(Packet *sourcePacket) = 0;

        // planAggregation() may perform owner mutations while enumerating
        // candidates (including retry state on a candidate that later breaks
        // aggregation). Structural validators remain pure.
        virtual AckPolicy selectAckPolicy(const Request& request,
                bool implicitBlockAck) const = 0;
        virtual const physicallayer::IIeee80211Mode *selectMode(
                const Request& request) const = 0;
        virtual void validateMode(const Request& request,
                const physicallayer::IIeee80211Mode *mode) const = 0;
        virtual AggregatePlan planAggregation(const Request& request,
                const physicallayer::IIeee80211Mode *mode,
                AckPolicy ackPolicy, bool implicitBlockAck) const = 0;
        virtual void validateAggregation(const Request& request,
                const physicallayer::IIeee80211Mode *mode,
                const AggregatePlan& aggregatePlan) const = 0;
        virtual void validateProtection(const Request& request,
                const physicallayer::IIeee80211Mode *mode,
                const AggregatePlan& aggregatePlan) const = 0;

        // Called after the selected mode has been attached because the
        // protection authority consumes that packet metadata.
        virtual ProtectionPlan computeProtection(const Request& request,
                const physicallayer::IIeee80211Mode *mode,
                const AggregatePlan& aggregatePlan) const = 0;

        virtual void applyAggregateMemberState(Packet *sourcePacket,
                const AggregatePlan& aggregatePlan) = 0;
        virtual Ptr<const Ieee80211MacHeader> applyAckPolicy(
                Packet *sourcePacket, const Ptr<const Ieee80211MacHeader>& header,
                AckPolicy ackPolicy, const AggregatePlan& aggregatePlan) = 0;
        virtual Ptr<const Ieee80211MacHeader> applyModePreparation(
                Packet *sourcePacket, const Ptr<const Ieee80211MacHeader>& header,
                const physicallayer::IIeee80211Mode *mode) = 0;
        virtual void applyDuration(Packet *sourcePacket,
                const Ptr<const Ieee80211MacHeader>& header,
                simtime_t duration) = 0;
        virtual Packet *materializeAggregate(Packet *sourcePacket,
                const AggregatePlan& aggregatePlan) = 0;
        virtual void setMode(Packet *transmittedPacket,
                const Ptr<const Ieee80211MacHeader>& header,
                const physicallayer::IIeee80211Mode *mode) = 0;
        // Propagates the same selected mode to the source descriptor of an HT
        // implicit-BlockAck temporary aggregate for response-timeout authority.
        virtual void setSourceMode(Packet *sourcePacket,
                const Ptr<const Ieee80211MacHeader>& header,
                const physicallayer::IIeee80211Mode *mode) = 0;
        virtual void recordSelectedMode(Packet *transmittedPacket,
                const physicallayer::IIeee80211Mode *mode) = 0;
        virtual void observeSelectedRate(Packet *transmittedPacket,
                const physicallayer::IIeee80211Mode *mode) = 0;

        // transmitBorrowed() does not take ownership. deleteTemporaryPacket()
        // is the sole terminal action for a materialized aggregate.
        virtual void transmitBorrowed(Packet *transmittedPacket,
                const Ptr<const Ieee80211MacHeader>& header,
                simtime_t ifs) = 0;
        virtual void deleteTemporaryPacket(Packet *packet) noexcept = 0;
    };

  private:
    static void validateRequest(const Request& request);
    static void validateAggregatePlan(const Request& request,
            const AggregatePlan& aggregatePlan);
    static void validateProtectionPlan(const Request& request,
            const ProtectionPlan& protectionPlan);

  public:
    void prepareAndTransmit(const Request& request, IActions& actions) const;
};

} // namespace ieee80211
} // namespace inet

#endif
