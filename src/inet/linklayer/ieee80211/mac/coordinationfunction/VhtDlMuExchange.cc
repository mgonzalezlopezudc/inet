//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchange.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

VhtDlMuExchange::VhtDlMuExchange(VhtDlMuExchangeId id, Packet *containerPacket,
        std::vector<std::vector<Packet *>> userPackets) :
    id(id), containerPacket(containerPacket), userPackets(std::move(userPackets)),
    completedUsers(this->userPackets.size(), false)
{
    if (id == 0 || containerPacket == nullptr || this->userPackets.empty())
        throw cRuntimeError("Invalid committed VHT DL MU exchange");
    for (const auto& packets : this->userPackets) {
        if (packets.empty() || std::any_of(packets.begin(), packets.end(),
                [] (const Packet *packet) { return packet == nullptr; }))
            throw cRuntimeError("Invalid VHT DL MU user packet identity");
        failedPackets.emplace_back(packets.size(), false);
    }
}

bool VhtDlMuExchange::containsPacket(const Packet *packet) const
{
    if (packet == nullptr)
        return false;
    if (packet == containerPacket)
        return true;
    for (const auto& packets : userPackets)
        if (std::find(packets.begin(), packets.end(), packet) != packets.end())
            return true;
    return false;
}

bool VhtDlMuExchange::recordFailedPacket(const Packet *packet)
{
    if (packet == nullptr)
        return false;
    for (size_t i = 0; i < userPackets.size(); ++i)
        for (size_t j = 0; j < userPackets[i].size(); ++j)
            if (userPackets[i][j] == packet) {
                if (failedPackets[i][j])
                    return false;
                failedPackets[i][j] = true;
                return true;
            }
    return false;
}

bool VhtDlMuExchange::recordUserResult(unsigned int userIndex,
        VhtDlMuUserResult result)
{
    if (userIndex >= completedUsers.size())
        return false;
    if (result == VhtDlMuUserResult::TRANSMITTED)
        return true;
    if (completedUsers[userIndex])
        return false;
    completedUsers[userIndex] = true;
    return true;
}

bool VhtDlMuExchange::isComplete() const
{
    return std::all_of(completedUsers.begin(), completedUsers.end(),
            [] (bool completed) { return completed; });
}

} // namespace ieee80211
} // namespace inet
