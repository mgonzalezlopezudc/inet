//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEQUEUESERVICE_H
#define __INET_HEQUEUESERVICE_H

#include <map>
#include <memory>
#include <vector>

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"
#include "inet/linklayer/ieee80211/mac/queue/StationQueueBankManager.h"

namespace inet {
namespace queueing { class IPacketQueue; }
namespace ieee80211 {

class Edca;

/** Owns HE per-STA queue banks and association-epoch packet retirement. */
class INET_API HeQueueService
{
  public:
    struct QueueSnapshot {
        HcfQueueToken token;
        MacAddress peer;
        uint64_t associationEpoch = 0;
        AccessCategory accessCategory = AC_BE;
        std::vector<Packet *> packets;
    };

    struct PacketReservation {
        HcfQueueToken queueToken;
        std::vector<HcfPacketIdentity> packetIdentities;
        bool active = false;
    };

  private:
    struct QueueRecord {
        queueing::IPacketQueue *queue = nullptr;
        MacAddress peer;
        uint64_t associationEpoch = 0;
        AccessCategory accessCategory = AC_BE;
    };

    std::unique_ptr<StationQueueBankManager> queueBankManager;
    std::map<Packet *, StationQueueBankManager::AssociationKey> deferredRetirements;
    std::map<uint64_t, QueueRecord> queuesByToken;
    std::map<queueing::IPacketQueue *, HcfQueueToken> tokensByQueue;
    uint64_t nextQueueToken = 1;

  private:
    HcfQueueToken registerQueue(queueing::IPacketQueue *queue,
            const MacAddress& peer, uint64_t associationEpoch,
            AccessCategory accessCategory);
    void revokeAssociationTokens(const MacAddress& peer,
            uint64_t associationEpoch);
    bool retireQueuedPacket(Packet *packet,
            const StationQueueBankManager::AssociationKey& association, Edca *edca);
    bool retireInProgressPacket(Packet *packet, Edca *edca);

  public:
    void configure(cModule *queueBanksModule);
    void clear();

    StationQueueBank *ensureAssociatedQueueBank(const MacAddress& peer,
            uint64_t associationEpoch);
    void retireAssociation(const MacAddress& peer, uint64_t associationEpoch,
            Edca *edca, bool frameSequenceRunning);
    void finalizeRetiredQueueBanksIfSafe(bool frameSequenceRunning);
    void retireDeferredPackets(Edca *edca);
    void deferRetirement(Packet *packet, const MacAddress& peer,
            uint64_t associationEpoch)
        { deferredRetirements[packet] = {peer, associationEpoch}; }

    queueing::IPacketQueue *getPerStaQueue(const MacAddress& peer,
            uint64_t associationEpoch, AccessCategory accessCategory);
    StationQueueBank *getStationQueueBank(const MacAddress& peer) const;
    int getStationQueueBankCount() const;
    int getTotalQueuedPackets() const
        { return queueBankManager == nullptr ? 0 : queueBankManager->getTotalQueuedPackets(); }
    int getTotalQueuedBytes() const
        { return queueBankManager == nullptr ? 0 : queueBankManager->getTotalQueuedBytes(); }
    bool hasFrameToTransmit(AccessCategory accessCategory) const;

    std::vector<QueueSnapshot> getQueueSnapshots(AccessCategory accessCategory,
            queueing::IPacketQueue *sharedQueue = nullptr);
    queueing::IPacketQueue *resolveQueue(HcfQueueToken token) const;
    bool stagePacket(HcfQueueToken token, HcfPacketIdentity identity,
            queueing::IPacketQueue *destinationQueue);
    bool reinsertPacket(HcfQueueToken token, HcfPacketIdentity identity,
            Packet *packet);
    PacketReservation preparePacketReservation(HcfQueueToken token,
            const std::vector<Packet *>& packets) const;
    std::vector<Packet *> commitPacketReservation(PacketReservation& reservation,
            const std::vector<Packet *>& preparedPackets);
    void restoreCommittedPackets(HcfQueueToken token,
            const std::vector<Packet *>& originals,
            const std::vector<Packet *>& backups,
            const std::vector<Packet *>& queueOrder);
    void rollbackPacketReservation(PacketReservation& reservation) const;
    HcfQueueToken getQueueToken(queueing::IPacketQueue *queue,
            const MacAddress& peer, uint64_t associationEpoch,
            AccessCategory accessCategory);
    HcfQueueToken findOldestPerStaQueue(AccessCategory accessCategory,
            const std::function<bool(const MacAddress&)>& isEligible);
};

} // namespace ieee80211
} // namespace inet

#endif
