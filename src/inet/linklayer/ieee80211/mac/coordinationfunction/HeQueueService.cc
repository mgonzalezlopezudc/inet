//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeQueueService.h"

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/queue/InProgressFrames.h"
#include "inet/linklayer/ieee80211/mac/queue/OrigEnqueueTimeTag_m.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

void HeQueueService::configure(cModule *queueBanksModule)
{
    if (queueBanksModule == nullptr)
        throw cRuntimeError("HE queue service requires the queueBanks module");
    queueBankManager = std::make_unique<StationQueueBankManager>(queueBanksModule);
}

void HeQueueService::clear()
{
    deferredRetirements.clear();
    queuesByToken.clear();
    tokensByQueue.clear();
    if (queueBankManager != nullptr)
        queueBankManager->clear();
}

HcfQueueToken HeQueueService::registerQueue(queueing::IPacketQueue *queue,
        const MacAddress& peer, uint64_t associationEpoch,
        AccessCategory accessCategory)
{
    if (queue == nullptr)
        return {};
    auto existing = tokensByQueue.find(queue);
    if (existing != tokensByQueue.end())
        return existing->second;
    if (nextQueueToken == 0)
        throw cRuntimeError("HE queue token space exhausted");
    HcfQueueToken token(nextQueueToken++);
    tokensByQueue[queue] = token;
    queuesByToken[token.getValue()] = {queue, peer, associationEpoch, accessCategory};
    return token;
}

HcfQueueToken HeQueueService::getQueueToken(queueing::IPacketQueue *queue,
        const MacAddress& peer, uint64_t associationEpoch,
        AccessCategory accessCategory)
{
    return registerQueue(queue, peer, associationEpoch, accessCategory);
}

queueing::IPacketQueue *HeQueueService::resolveQueue(HcfQueueToken token) const
{
    auto it = queuesByToken.find(token.getValue());
    return it == queuesByToken.end() ? nullptr : it->second.queue;
}

bool HeQueueService::stagePacket(HcfQueueToken token, HcfPacketIdentity identity,
        queueing::IPacketQueue *destinationQueue)
{
    auto sourceQueue = resolveQueue(token);
    if (sourceQueue == nullptr || destinationQueue == nullptr ||
            sourceQueue == destinationQueue || !identity.isValid())
        return false;
    for (int i = 0; i < sourceQueue->getNumPackets(); ++i) {
        auto packet = sourceQueue->getPacket(i);
        if (HcfPacketIdentity(packet->getId()) != identity)
            continue;
        sourceQueue->removePacket(packet);
        destinationQueue->enqueuePacket(packet);
        return true;
    }
    return false;
}

bool HeQueueService::reinsertPacket(HcfQueueToken token,
        HcfPacketIdentity identity, Packet *packet)
{
    auto queue = resolveQueue(token);
    if (queue == nullptr || packet == nullptr ||
            HcfPacketIdentity(packet->getId()) != identity)
        return false;
    queue->pushPacket(packet, nullptr);
    return true;
}

HeQueueService::PacketReservation HeQueueService::preparePacketReservation(
        HcfQueueToken token, const std::vector<Packet *>& packets) const
{
    auto queue = resolveQueue(token);
    if (queue == nullptr)
        throw cRuntimeError("Cannot reserve packets from an unknown HE queue token");
    PacketReservation reservation;
    reservation.queueToken = token;
    for (auto packet : packets) {
        if (packet == nullptr)
            throw cRuntimeError("Cannot reserve an empty HE queue packet");
        bool found = false;
        for (int i = 0; i < queue->getNumPackets(); ++i)
            found |= queue->getPacket(i) == packet;
        if (!found)
            throw cRuntimeError("HE-TB selected packet changed queue membership before reservation");
        reservation.packetIdentities.emplace_back(packet->getId());
    }
    reservation.active = true;
    return reservation;
}

std::vector<Packet *> HeQueueService::commitPacketReservation(
        PacketReservation& reservation,
        const std::vector<Packet *>& preparedPackets)
{
    if (!reservation.active ||
            reservation.packetIdentities.size() != preparedPackets.size())
        throw cRuntimeError("Cannot commit an inactive or incomplete HE queue reservation");
    auto queue = resolveQueue(reservation.queueToken);
    if (queue == nullptr)
        throw cRuntimeError("HE queue reservation token expired before commit");
    std::vector<Packet *> originals;
    for (auto identity : reservation.packetIdentities) {
        Packet *original = nullptr;
        for (int i = 0; i < queue->getNumPackets(); ++i)
            if (HcfPacketIdentity(queue->getPacket(i)->getId()) == identity) {
                original = queue->getPacket(i);
                break;
            }
        if (original == nullptr)
            throw cRuntimeError("HE-TB reserved packet changed queue membership before commit");
        originals.push_back(original);
    }
    std::vector<std::unique_ptr<Packet>> backups;
    std::vector<Packet *> queueOrder;
    for (int i = 0; i < queue->getNumPackets(); ++i)
        queueOrder.push_back(queue->getPacket(i));
    for (auto original : originals)
        backups.emplace_back(original->dup());
    size_t removedCount = 0;
    try {
        for (size_t i = 0; i < originals.size(); ++i) {
            auto original = originals[i];
            auto prepared = preparedPackets[i];
            queue->removePacket(original);
            removedCount++;
            original->removeAll();
            original->insertAtBack(prepared->peekAll());
            original->setFrontOffset(prepared->getFrontOffset());
            original->setBackOffset(prepared->getBackOffset());
            original->clearTags();
            original->copyTags(*prepared);
            original->getRegionTags() = prepared->getRegionTags();
        }
    }
    catch (...) {
        for (size_t i = 0; i < removedCount; ++i) {
            auto original = originals[i];
            auto backup = backups[i].get();
            original->removeAll();
            original->insertAtBack(backup->peekAll());
            original->setFrontOffset(backup->getFrontOffset());
            original->setBackOffset(backup->getBackOffset());
            original->clearTags();
            original->copyTags(*backup);
            original->getRegionTags() = backup->getRegionTags();
        }
        while (queue->getNumPackets() > 0)
            queue->removePacket(queue->getPacket(0));
        for (auto it = queueOrder.rbegin(); it != queueOrder.rend(); ++it)
            queue->pushPacket(*it, nullptr);
        throw;
    }
    reservation.active = false;
    return originals;
}

void HeQueueService::rollbackPacketReservation(PacketReservation& reservation) const
{
    reservation.active = false;
}

void HeQueueService::restoreCommittedPackets(HcfQueueToken token,
        const std::vector<Packet *>& originals,
        const std::vector<Packet *>& backups,
        const std::vector<Packet *>& queueOrder)
{
    if (originals.size() != backups.size())
        throw cRuntimeError("Incomplete HE queue rollback snapshot");
    auto queue = resolveQueue(token);
    if (queue == nullptr)
        throw cRuntimeError("HE queue token expired before rollback");
    for (size_t i = 0; i < originals.size(); ++i) {
        auto original = originals[i];
        auto backup = backups[i];
        original->removeAll();
        original->insertAtBack(backup->peekAll());
        original->setFrontOffset(backup->getFrontOffset());
        original->setBackOffset(backup->getBackOffset());
        original->clearTags();
        original->copyTags(*backup);
        original->getRegionTags() = backup->getRegionTags();
    }
    while (queue->getNumPackets() > 0)
        queue->removePacket(queue->getPacket(0));
    for (auto it = queueOrder.rbegin(); it != queueOrder.rend(); ++it)
        queue->pushPacket(*it, nullptr);
}

void HeQueueService::revokeAssociationTokens(const MacAddress& peer,
        uint64_t associationEpoch)
{
    for (auto it = queuesByToken.begin(); it != queuesByToken.end(); ) {
        if (it->second.peer == peer && it->second.associationEpoch == associationEpoch) {
            tokensByQueue.erase(it->second.queue);
            it = queuesByToken.erase(it);
        }
        else
            ++it;
    }
}

StationQueueBank *HeQueueService::ensureAssociatedQueueBank(
        const MacAddress& peer, uint64_t associationEpoch)
{
    if (queueBankManager == nullptr)
        return nullptr;
    auto bank = queueBankManager->ensureQueueBank(peer, associationEpoch);
    for (int ac = StationQueueBank::AC_BK; ac <= StationQueueBank::AC_VO; ++ac)
        registerQueue(bank->getQueue(static_cast<StationQueueBank::AccessCategory>(ac)),
                peer, associationEpoch, static_cast<AccessCategory>(ac));
    return bank;
}

queueing::IPacketQueue *HeQueueService::getPerStaQueue(const MacAddress& peer,
        uint64_t associationEpoch, AccessCategory accessCategory)
{
    auto bank = ensureAssociatedQueueBank(peer, associationEpoch);
    return bank == nullptr ? nullptr :
            bank->getQueue(static_cast<StationQueueBank::AccessCategory>(accessCategory));
}

StationQueueBank *HeQueueService::getStationQueueBank(const MacAddress& peer) const
{
    return queueBankManager == nullptr ? nullptr : queueBankManager->getQueueBank(peer);
}

int HeQueueService::getStationQueueBankCount() const
{
    return queueBankManager == nullptr ? 0 : queueBankManager->getQueueBankCount();
}

bool HeQueueService::hasFrameToTransmit(AccessCategory accessCategory) const
{
    if (queueBankManager == nullptr)
        return false;
    for (const auto& entry : queueBankManager->getQueueBanks())
        if (!entry.second->getQueue(static_cast<StationQueueBank::AccessCategory>(accessCategory))->isEmpty())
            return true;
    return false;
}

std::vector<HeQueueService::QueueSnapshot> HeQueueService::getQueueSnapshots(
        AccessCategory accessCategory, queueing::IPacketQueue *sharedQueue)
{
    std::vector<QueueSnapshot> result;
    if (sharedQueue != nullptr) {
        QueueSnapshot snapshot;
        snapshot.token = registerQueue(sharedQueue, MacAddress(), 0, accessCategory);
        snapshot.accessCategory = accessCategory;
        for (int i = 0; i < sharedQueue->getNumPackets(); ++i)
            snapshot.packets.push_back(sharedQueue->getPacket(i));
        result.push_back(snapshot);
    }
    if (queueBankManager == nullptr)
        return result;
    for (const auto& entry : queueBankManager->getQueueBanks()) {
        auto queue = entry.second->getQueue(static_cast<StationQueueBank::AccessCategory>(accessCategory));
        QueueSnapshot snapshot;
        snapshot.peer = entry.first;
        snapshot.associationEpoch = queueBankManager->getAssociationEpoch(entry.first);
        snapshot.accessCategory = accessCategory;
        snapshot.token = registerQueue(queue, snapshot.peer, snapshot.associationEpoch, accessCategory);
        for (int i = 0; i < queue->getNumPackets(); ++i)
            snapshot.packets.push_back(queue->getPacket(i));
        result.push_back(std::move(snapshot));
    }
    return result;
}

HcfQueueToken HeQueueService::findOldestPerStaQueue(AccessCategory accessCategory,
        const std::function<bool(const MacAddress&)>& isEligible)
{
    if (queueBankManager == nullptr || !isEligible)
        return {};
    queueing::IPacketQueue *oldestQueue = nullptr;
    MacAddress oldestPeer;
    uint64_t oldestEpoch = 0;
    simtime_t oldestEnqueueTime = SIMTIME_MAX;
    for (const auto& entry : queueBankManager->getQueueBanks()) {
        auto queue = entry.second->getQueue(static_cast<StationQueueBank::AccessCategory>(accessCategory));
        if (queue->isEmpty() || !isEligible(entry.first))
            continue;
        auto packet = queue->getPacket(0);
        auto enqueueTimeTag = packet->findTag<OrigEnqueueTimeTag>();
        auto enqueueTime = enqueueTimeTag == nullptr ? packet->getArrivalTime() : enqueueTimeTag->getEnqueueTime();
        if (oldestQueue == nullptr || enqueueTime < oldestEnqueueTime) {
            oldestQueue = queue;
            oldestPeer = entry.first;
            oldestEpoch = queueBankManager->getAssociationEpoch(entry.first);
            oldestEnqueueTime = enqueueTime;
        }
    }
    return registerQueue(oldestQueue, oldestPeer, oldestEpoch, accessCategory);
}

bool HeQueueService::retireQueuedPacket(Packet *packet,
        const StationQueueBankManager::AssociationKey& association, Edca *edca)
{
    auto retireFromQueue = [&] (queueing::IPacketQueue *queue, AccessCategory accessCategory) {
        if (queue == nullptr)
            return false;
        for (int index = 0; index < queue->getNumPackets(); ++index) {
            if (queue->getPacket(index) != packet)
                continue;
            if (edca != nullptr) {
                auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                        packet->peekAtFront<Ieee80211MacHeader>());
                if (header != nullptr)
                    edca->getEdcaf(accessCategory)->getAckHandler()->retireFrame(header);
            }
            queue->removePacket(packet);
            deferredRetirements.erase(packet);
            delete packet;
            return true;
        }
        return false;
    };

    if (queueBankManager != nullptr &&
            queueBankManager->getAssociationEpoch(association.first) == association.second) {
        auto bank = queueBankManager->getQueueBank(association.first);
        if (bank != nullptr)
            for (int ac = StationQueueBank::AC_BK; ac <= StationQueueBank::AC_VO; ++ac)
                if (retireFromQueue(bank->getQueue(static_cast<StationQueueBank::AccessCategory>(ac)),
                        static_cast<AccessCategory>(ac)))
                    return true;
    }
    if (edca != nullptr)
        for (int ac = AC_BK; ac <= AC_VO; ++ac)
            if (retireFromQueue(edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue(),
                    static_cast<AccessCategory>(ac)))
                return true;
    return false;
}

bool HeQueueService::retireInProgressPacket(Packet *packet, Edca *edca)
{
    if (edca == nullptr)
        return false;
    for (int ac = AC_BK; ac <= AC_VO; ++ac)
        if (edca->getEdcaf(static_cast<AccessCategory>(ac))->getInProgressFrames()->retireFrame(packet))
            return true;
    return false;
}

void HeQueueService::retireAssociation(const MacAddress& peer,
        uint64_t associationEpoch, Edca *edca, bool frameSequenceRunning)
{
    revokeAssociationTokens(peer, associationEpoch);
    std::vector<Packet *> queuedPackets;
    if (queueBankManager != nullptr) {
        auto bank = queueBankManager->getAssociationEpoch(peer) == associationEpoch ?
                queueBankManager->getQueueBank(peer) : nullptr;
        if (bank != nullptr)
            for (int ac = StationQueueBank::AC_BK; ac <= StationQueueBank::AC_VO; ++ac) {
                auto queue = bank->getQueue(static_cast<StationQueueBank::AccessCategory>(ac));
                for (int i = 0; i < queue->getNumPackets(); ++i)
                    queuedPackets.push_back(queue->getPacket(i));
            }
    }
    if (edca != nullptr)
        for (int ac = AC_BK; ac <= AC_VO; ++ac) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int i = 0; i < queue->getNumPackets(); ++i) {
                auto packet = queue->getPacket(i);
                auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                        packet->peekAtFront<Ieee80211MacHeader>());
                if (header != nullptr && header->getReceiverAddress() == peer)
                    queuedPackets.push_back(packet);
            }
        }
    for (auto packet : queuedPackets)
        retireQueuedPacket(packet, {peer, associationEpoch}, edca);

    std::vector<Packet *> inProgressPackets;
    if (edca != nullptr) {
        for (int ac = AC_BK; ac <= AC_VO; ++ac) {
            auto inProgress = edca->getEdcaf(static_cast<AccessCategory>(ac))->getInProgressFrames();
            for (int i = 0; i < inProgress->getLength(); ++i) {
                auto packet = inProgress->getFrames(i);
                auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                        packet->peekAtFront<Ieee80211MacHeader>());
                if (header != nullptr && header->getReceiverAddress() == peer) {
                    if (frameSequenceRunning)
                        deferredRetirements[packet] = {peer, associationEpoch};
                    else
                        inProgressPackets.push_back(packet);
                }
            }
        }
    }
    for (auto packet : inProgressPackets)
        retireInProgressPacket(packet, edca);
    if (queueBankManager != nullptr)
        queueBankManager->retireQueueBank(peer, associationEpoch);
    finalizeRetiredQueueBanksIfSafe(frameSequenceRunning);
}

void HeQueueService::retireDeferredPackets(Edca *edca)
{
    while (!deferredRetirements.empty()) {
        auto entry = *deferredRetirements.begin();
        deferredRetirements.erase(deferredRetirements.begin());
        if (!retireQueuedPacket(entry.first, entry.second, edca))
            retireInProgressPacket(entry.first, edca);
    }
}

void HeQueueService::finalizeRetiredQueueBanksIfSafe(bool frameSequenceRunning)
{
    if (queueBankManager != nullptr && !frameSequenceRunning)
        queueBankManager->finalizeRetiredQueueBanks();
}

} // namespace ieee80211
} // namespace inet
