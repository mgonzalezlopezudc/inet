//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtGroupIdManager.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

Define_Module(VhtGroupIdManager);

void VhtGroupIdManager::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        pendingTimeout = par("pendingTimeout");
        if (pendingTimeout <= SIMTIME_ZERO)
            throw cRuntimeError("VHT Group ID pending timeout must be positive");
    }
}

VhtGroupIdManager::~VhtGroupIdManager()
{
    cancelAndDelete(expiryTimer);
}

void VhtGroupIdManager::handleMessage(cMessage *message)
{
    if (message != expiryTimer)
        throw cRuntimeError("Unexpected message for VHT Group ID manager");
    expirePending();
    rescheduleExpiryTimer();
}

void VhtGroupIdManager::expirePending()
{
    for (auto it = entries.begin(); it != entries.end(); )
        it = it->second.state == State::PENDING && simTime() >= it->second.expiryTime ?
                entries.erase(it) : std::next(it);
}

void VhtGroupIdManager::rescheduleExpiryTimer()
{
    simtime_t earliest = -1;
    for (const auto& entry : entries)
        if (entry.second.state == State::PENDING &&
                (earliest < SIMTIME_ZERO || entry.second.expiryTime < earliest))
            earliest = entry.second.expiryTime;
    if (earliest >= SIMTIME_ZERO) {
        if (expiryTimer == nullptr) {
            expiryTimer = new cMessage("vhtGroupIdPendingExpiry");
            take(expiryTimer);
        }
        cancelEvent(expiryTimer);
        scheduleAt(std::max(simTime(), earliest), expiryTimer);
    }
    else if (expiryTimer != nullptr) {
        cancelAndDelete(expiryTimer);
        expiryTimer = nullptr;
    }
}

bool VhtGroupIdManager::isMember(const Ieee80211VhtGroupIdManagement& action,
        uint8_t groupId)
{
    if (groupId > 63 || action.getMembershipStatusArraySize() != 8)
        return false;
    return (action.getMembershipStatus(groupId / 8) & (uint8_t(1) << (groupId % 8))) != 0;
}

uint8_t VhtGroupIdManager::getUserPosition(
        const Ieee80211VhtGroupIdManagement& action, uint8_t groupId)
{
    if (groupId > 63 || action.getUserPositionArraySize() != 16)
        throw cRuntimeError("Malformed VHT Group ID Management user-position array");
    return (action.getUserPosition(groupId / 4) >> (2 * (groupId % 4))) & 0x3;
}

void VhtGroupIdManager::setMembership(Ieee80211VhtGroupIdManagement& action,
        uint8_t groupId, uint8_t userPosition)
{
    if (groupId == 0 || groupId == 63 || userPosition > 3 ||
            action.getMembershipStatusArraySize() != 8 ||
            action.getUserPositionArraySize() != 16)
        throw cRuntimeError("Invalid constrained VHT Group ID membership");
    auto membership = action.getMembershipStatus(groupId / 8);
    action.setMembershipStatus(groupId / 8,
            membership | (uint8_t(1) << (groupId % 8)));
    auto positions = action.getUserPosition(groupId / 4);
    auto shift = 2 * (groupId % 4);
    action.setUserPosition(groupId / 4,
            (positions & ~(uint8_t(3) << shift)) | (userPosition << shift));
}

IVhtGroupIdManager::State VhtGroupIdManager::getState(const MacAddress& peer,
        uint8_t groupId, uint64_t associationGeneration, Hz channelWidth) const
{
    auto it = entries.find(peer);
    if (it == entries.end())
        return State::ABSENT;
    const auto& entry = it->second;
    if (entry.groupId != groupId || entry.associationGeneration != associationGeneration ||
            entry.channelWidth != channelWidth ||
            (entry.state == State::PENDING && simTime() >= entry.expiryTime))
        return State::ABSENT;
    return entry.state;
}

bool VhtGroupIdManager::isActive(const MacAddress& peer, uint8_t groupId,
        uint8_t userPosition, uint64_t associationGeneration, Hz channelWidth) const
{
    auto it = entries.find(peer);
    return getState(peer, groupId, associationGeneration, channelWidth) == State::ACTIVE &&
            it->second.userPosition == userPosition;
}

void VhtGroupIdManager::beginPending(const MacAddress& peer, uint8_t groupId,
        uint8_t userPosition, uint64_t associationGeneration, Hz channelWidth)
{
    if (peer.isMulticast() || peer.isUnspecified() || groupId == 0 || groupId == 63 ||
            userPosition > 3 || associationGeneration == 0 || channelWidth != MHz(20))
        throw cRuntimeError("Invalid constrained VHT Group ID pending state");
    entries[peer] = {State::PENDING, groupId, userPosition,
            associationGeneration, channelWidth, simTime() + pendingTimeout};
    rescheduleExpiryTimer();
}

bool VhtGroupIdManager::acknowledge(const MacAddress& peer, uint8_t groupId,
        uint64_t associationGeneration, Hz channelWidth)
{
    auto it = entries.find(peer);
    if (it == entries.end() || getState(peer, groupId,
            associationGeneration, channelWidth) != State::PENDING)
        return false;
    it->second.state = State::ACTIVE;
    it->second.expiryTime = -1;
    rescheduleExpiryTimer();
    return true;
}

bool VhtGroupIdManager::consume(const MacAddress& peer,
        const Ptr<const Ieee80211VhtGroupIdManagement>& action,
        uint64_t associationGeneration, Hz channelWidth)
{
    constexpr uint8_t GROUP_ID = 1;
    if (peer.isMulticast() || peer.isUnspecified() || action == nullptr ||
            associationGeneration == 0 || channelWidth != MHz(20) ||
            !isMember(*action, GROUP_ID)) {
        invalidatePeer(peer);
        return false;
    }
    entries[peer] = {State::ACTIVE, GROUP_ID, getUserPosition(*action, GROUP_ID),
            associationGeneration, channelWidth, -1};
    localMembership = Membership{peer, GROUP_ID, getUserPosition(*action, GROUP_ID),
            associationGeneration, channelWidth};
    if (localMembershipListener != nullptr)
        localMembershipListener->localVhtGroupMembershipChanged(localMembership);
    rescheduleExpiryTimer();
    return true;
}

void VhtGroupIdManager::invalidatePeer(const MacAddress& peer)
{
    entries.erase(peer);
    if (localMembership.has_value() && localMembership->peer == peer) {
        localMembership.reset();
        if (localMembershipListener != nullptr)
            localMembershipListener->localVhtGroupMembershipChanged(localMembership);
    }
    rescheduleExpiryTimer();
}

void VhtGroupIdManager::invalidateAll()
{
    entries.clear();
    if (localMembership.has_value()) {
        localMembership.reset();
        if (localMembershipListener != nullptr)
            localMembershipListener->localVhtGroupMembershipChanged(localMembership);
    }
    rescheduleExpiryTimer();
}

void VhtGroupIdManager::setLocalMembershipListener(ILocalMembershipListener *listener)
{
    localMembershipListener = listener;
    if (listener != nullptr)
        listener->localVhtGroupMembershipChanged(localMembership);
}

} // namespace ieee80211
} // namespace inet
