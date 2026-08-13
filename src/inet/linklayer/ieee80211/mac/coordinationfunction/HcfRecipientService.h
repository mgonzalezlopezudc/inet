//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFRECIPIENTSERVICE_H
#define __INET_HCFRECIPIENTSERVICE_H

#include <utility>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

class Packet;

namespace ieee80211 {

/**
 * Routes common HCF recipient work without owning mutable protocol state.
 *
 * The caller transfers the input Packet to processFrame() or processAmpdu().
 * Header and packet arguments documented as borrowed are valid only for the
 * duration of the action call. deliverData() and deliverManagement() transfer
 * their Packet exactly once. deletePacket() is the terminal transfer for every
 * other path. Terminal actions are noexcept; protocol-action exceptions occur
 * while the service still owns the input and cause deterministic cleanup before
 * the exception is rethrown.
 */
class INET_API HcfRecipientService
{
  public:
    enum class AddressDisposition {
        LOCAL_UNICAST,
        LOCAL_GROUP,
        FOREIGN,
    };

    enum class DropReason {
        NOT_ADDRESSED_TO_US,
        INCORRECTLY_RECEIVED,
        INVALID_IMPLICIT_BLOCK_ACK_MEMBER,
    };

    enum class AggregateResponsePolicy {
        ORDINARY,
        HT_IMPLICIT_BLOCK_ACK,
    };

    struct Result {
        unsigned int decodedMemberCount = 0;
        unsigned int acceptedMemberCount = 0;
        unsigned int droppedMemberCount = 0;
        bool aggregateResponseSent = false;
    };

    class IActions
    {
      public:
        virtual ~IActions() {}

        // The packet and header are borrowed; this action owns address policy.
        virtual AddressDisposition classifyAddress(const Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header) = 0;

        // Observation only; neither argument is transferred.
        virtual void packetReceived(const Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header) noexcept = 0;
        virtual void packetDropped(const Packet *packet, DropReason reason) noexcept = 0;

        // Existing ACK/BA owners process borrowed input and retain their state authority.
        virtual void processImmediateResponse(Packet *packet,
                const Ptr<const Ieee80211DataOrMgmtHeader>& header) = 0;
        virtual bool processHtImplicitBlockAckResponse(Packet *aggregate,
                const std::vector<Ptr<const Ieee80211DataHeader>>& admittedHeaders) = 0;

        // Transfers an ordinary aggregate member into the complete virtual
        // recipient path, preserving amendment preprocessing before common work.
        virtual void processOrdinaryAggregateMember(Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header) = 0;

        // Existing recipient data services receive ownership exactly once.
        virtual void deliverData(Packet *packet,
                const Ptr<const Ieee80211DataHeader>& header,
                bool implicitBlockAckMember) noexcept = 0;
        virtual void deliverManagement(Packet *packet,
                const Ptr<const Ieee80211MgmtHeader>& header) noexcept = 0;

        // Control processing borrows the packet; the service deletes it afterward.
        virtual void processControl(Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header) = 0;

        // Terminal ownership transfer for drops, control frames, and aggregate envelopes.
        virtual void deletePacket(Packet *packet) noexcept = 0;
    };

  private:
    static void deleteWithObservation(Packet *packet, DropReason reason,
            IActions& actions) noexcept;
    static void cleanUpOwnedPackets(Packet *aggregate, Packet *elicitingAggregate,
            std::vector<std::pair<Packet *, Ptr<const Ieee80211MacHeader>>>& members,
            IActions& actions) noexcept;

  public:
    void processFrame(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header, IActions& actions) const;
    Result processAmpdu(Packet *aggregate, AggregateResponsePolicy responsePolicy,
            IActions& actions) const;
};

} // namespace ieee80211
} // namespace inet

#endif
