//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mib/Ieee80211AssociationState.h"

#include <limits>

namespace inet {

namespace ieee80211 {

void Ieee80211AssociationState::advanceGeneration(uint64_t& generation)
{
    if (++generation == 0)
        generation = 1;
}

Ieee80211AssociationState::LocalSnapshot Ieee80211AssociationState::getLocalSnapshot() const
{
    return LocalSnapshot(stationType, associated, associationId, ssid, bssid, localGeneration);
}

Ieee80211AssociationState::PeerSnapshot Ieee80211AssociationState::getPeerSnapshot(const MacAddress& address) const
{
    auto memberStatus = peerMemberStatuses.find(address);
    auto associationId = peerAssociationIds.find(address);
    auto generation = peerGenerations.find(address);
    auto associationEpoch = peerAssociationEpochs.find(address);
    return PeerSnapshot(address, memberStatus != peerMemberStatuses.end(), associationId != peerAssociationIds.end(),
            memberStatus == peerMemberStatuses.end() ? NOT_AUTHENTICATED : memberStatus->second,
            associationId == peerAssociationIds.end() ? -1 : associationId->second,
            generation == peerGenerations.end() ? 0 : generation->second,
            associationEpoch == peerAssociationEpochs.end() ? 0 : associationEpoch->second);
}

std::vector<Ieee80211AssociationState::PeerSnapshot> Ieee80211AssociationState::getPeerSnapshots() const
{
    std::vector<PeerSnapshot> snapshots;
    auto memberStatus = peerMemberStatuses.begin();
    auto associationId = peerAssociationIds.begin();
    while (memberStatus != peerMemberStatuses.end() || associationId != peerAssociationIds.end()) {
        MacAddress address;
        if (associationId == peerAssociationIds.end() ||
                (memberStatus != peerMemberStatuses.end() && memberStatus->first < associationId->first))
            address = memberStatus->first;
        else
            address = associationId->first;
        snapshots.push_back(getPeerSnapshot(address));
        if (memberStatus != peerMemberStatuses.end() && memberStatus->first == address)
            ++memberStatus;
        if (associationId != peerAssociationIds.end() && associationId->first == address)
            ++associationId;
    }
    return snapshots;
}

void Ieee80211AssociationState::setStationType(BssStationType stationType)
{
    if (this->stationType != stationType) {
        this->stationType = stationType;
        advanceGeneration(localGeneration);
    }
}

void Ieee80211AssociationState::setBssIdentity(const std::string& ssid, const MacAddress& bssid)
{
    if (this->ssid != ssid || this->bssid != bssid) {
        this->ssid = ssid;
        this->bssid = bssid;
        advanceGeneration(localGeneration);
    }
}

void Ieee80211AssociationState::installLocalAssociation(const std::string& ssid, const MacAddress& bssid, short associationId)
{
    // IEEE Std 802.11-2024, 6.5.8.3.2: a successful non-DMG association has an AID in 1..2007.
    if (associationId < 1 || associationId > 2007)
        throw cRuntimeError("Invalid IEEE 802.11 association ID %d; expected 1..2007", associationId);
    if (this->ssid != ssid || this->bssid != bssid || !associated || this->associationId != associationId) {
        this->ssid = ssid;
        this->bssid = bssid;
        associated = true;
        this->associationId = associationId;
        advanceGeneration(localGeneration);
    }
}

void Ieee80211AssociationState::clearLocalAssociation()
{
    if (associated || associationId != -1) {
        associated = false;
        associationId = -1;
        advanceGeneration(localGeneration);
    }
}

void Ieee80211AssociationState::setPeerMemberStatus(const MacAddress& address, BssMemberStatus memberStatus)
{
    if (memberStatus == ASSOCIATED)
        throw cRuntimeError("Use commitPeerAssociation() to associate an IEEE 802.11 peer");
    auto snapshot = getPeerSnapshot(address);
    if (snapshot.getAssociationEpoch() != 0)
        throw cRuntimeError("Use clearPeerAssociation() to change an associated IEEE 802.11 peer status");
    auto it = peerMemberStatuses.find(address);
    if (it == peerMemberStatuses.end() || it->second != memberStatus) {
        peerMemberStatuses[address] = memberStatus;
        advanceGeneration(peerGenerations[address]);
    }
}

short Ieee80211AssociationState::reservePeerAssociation(const MacAddress& address)
{
    auto existingReservation = peerAssociationReservations.find(address);
    if (existingReservation != peerAssociationReservations.end())
        return existingReservation->second;
    short associationId = getAssociationId(address);
    if (associationId == -1) {
        for (short candidate = 1; candidate <= 2007; candidate++) {
            bool used = false;
            for (const auto& entry : peerAssociationIds)
                if (entry.second == candidate) {
                    used = true;
                    break;
                }
            if (!used)
                for (const auto& entry : peerAssociationReservations)
                    if (entry.second == candidate) {
                        used = true;
                        break;
                    }
            if (!used) {
                associationId = candidate;
                break;
            }
        }
    }
    if (associationId == -1)
        throw cRuntimeError("No IEEE 802.11 association ID is available");
    peerAssociationReservations[address] = associationId;
    return associationId;
}

void Ieee80211AssociationState::releasePeerAssociationReservation(const MacAddress& address, short associationId)
{
    auto it = peerAssociationReservations.find(address);
    if (it == peerAssociationReservations.end())
        return;
    if (it->second != associationId)
        throw cRuntimeError("IEEE 802.11 association reservation mismatch for %s", address.str().c_str());
    peerAssociationReservations.erase(it);
}

Ieee80211AssociationState::PeerTransition Ieee80211AssociationState::commitPeerAssociation(const MacAddress& address)
{
    return commitPeerAssociation(address, reservePeerAssociation(address));
}

Ieee80211AssociationState::PeerTransition Ieee80211AssociationState::commitPeerAssociation(
        const MacAddress& address, short associationId)
{
    auto oldSnapshot = getPeerSnapshot(address);
    if (associationId < 1 || associationId > 2007)
        throw cRuntimeError("Invalid IEEE 802.11 association ID %d; expected 1..2007", associationId);
    auto reservation = peerAssociationReservations.find(address);
    if (reservation != peerAssociationReservations.end() && reservation->second != associationId)
        throw cRuntimeError("IEEE 802.11 association reservation mismatch for %s", address.str().c_str());
    for (const auto& entry : peerAssociationIds)
        if (entry.first != address && entry.second == associationId)
            throw cRuntimeError("IEEE 802.11 association ID %d is already assigned", associationId);
    peerAssociationReservations.erase(address);

    if (nextAssociationEpoch == std::numeric_limits<uint64_t>::max())
        throw cRuntimeError("IEEE 802.11 association epoch space exhausted");
    bool semanticStateChanged = !oldSnapshot.hasMemberStatus() || oldSnapshot.getMemberStatus() != ASSOCIATED ||
            !oldSnapshot.hasAssociationId() || oldSnapshot.getAssociationId() != associationId;
    peerMemberStatuses[address] = ASSOCIATED;
    peerAssociationIds[address] = associationId;
    if (semanticStateChanged)
        advanceGeneration(peerGenerations[address]);
    nextAssociationEpoch++;
    peerAssociationEpochs[address] = nextAssociationEpoch;
    auto newSnapshot = getPeerSnapshot(address);
    return PeerTransition(oldSnapshot, newSnapshot, nextAssociationEpoch);
}

Ieee80211AssociationState::PeerTransition Ieee80211AssociationState::clearPeerAssociation(
        const MacAddress& address, BssMemberStatus memberStatus)
{
    if (memberStatus == ASSOCIATED)
        throw cRuntimeError("Clearing an IEEE 802.11 peer association requires a non-associated status");
    auto oldSnapshot = getPeerSnapshot(address);
    auto oldEpoch = oldSnapshot.getAssociationEpoch();
    bool changed = !oldSnapshot.hasMemberStatus() || oldSnapshot.getMemberStatus() != memberStatus ||
            oldSnapshot.hasAssociationId() || oldEpoch != 0;
    peerMemberStatuses[address] = memberStatus;
    peerAssociationIds.erase(address);
    peerAssociationEpochs.erase(address);
    if (changed)
        advanceGeneration(peerGenerations[address]);
    return PeerTransition(oldSnapshot, getPeerSnapshot(address), oldEpoch);
}

short Ieee80211AssociationState::getAssociationId(const MacAddress& address) const
{
    auto it = peerAssociationIds.find(address);
    return it == peerAssociationIds.end() ? -1 : it->second;
}

MacAddress Ieee80211AssociationState::getStationAddress(short associationId) const
{
    for (const auto& entry : peerAssociationIds)
        if (entry.second == associationId)
            return entry.first;
    return MacAddress::UNSPECIFIED_ADDRESS;
}

} // namespace ieee80211

} // namespace inet
