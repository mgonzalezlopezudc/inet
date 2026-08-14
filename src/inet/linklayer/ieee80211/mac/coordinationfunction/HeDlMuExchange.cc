//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchange.h"

#include <algorithm>

#include "inet/common/packet/Packet.h"

namespace inet {
namespace ieee80211 {

HeDlMuExchange::HeDlMuExchange(uint64_t id, Packet *containerPacket,
        std::vector<HeDlMuMember> members) :
    id(id), containerPacket(containerPacket), members(std::move(members)),
    transmittedMembers(this->members.size(), false)
{
    if (id == 0 || containerPacket == nullptr || this->members.empty())
        throw cRuntimeError("Invalid committed HE DL MU exchange");
    for (const auto& member : this->members) {
        if (member.packet == nullptr || member.peer.isUnspecified())
            throw cRuntimeError("Invalid HE DL MU member");
        if (std::find(users.begin(), users.end(), member.peer) == users.end())
            users.push_back(member.peer);
    }
    completedUsers.assign(users.size(), false);
}

int HeDlMuExchange::findMember(const HeDlMuMember& member) const
{
    auto it = std::find_if(members.begin(), members.end(), [&] (const auto& committed) {
        return committed.packetIdentity == member.packetIdentity &&
                committed.packet == member.packet && committed.peer == member.peer &&
                committed.tid == member.tid &&
                committed.accessCategory == member.accessCategory;
    });
    return it == members.end() ? -1 : std::distance(members.begin(), it);
}

int HeDlMuExchange::findUser(const MacAddress& peer) const
{
    auto it = std::find(users.begin(), users.end(), peer);
    return it == users.end() ? -1 : std::distance(users.begin(), it);
}

bool HeDlMuExchange::recordMemberTransmitted(const HeDlMuMember& member)
{
    auto index = findMember(member);
    if (index < 0 || transmittedMembers[index])
        return false;
    transmittedMembers[index] = true;
    return true;
}

bool HeDlMuExchange::recordUserOutcome(const MacAddress& peer)
{
    auto index = findUser(peer);
    if (index < 0 || completedUsers[index])
        return false;
    completedUsers[index] = true;
    return true;
}

bool HeDlMuExchange::isComplete() const
{
    return std::all_of(completedUsers.begin(), completedUsers.end(),
            [] (bool completed) { return completed; });
}

} // namespace ieee80211
} // namespace inet
