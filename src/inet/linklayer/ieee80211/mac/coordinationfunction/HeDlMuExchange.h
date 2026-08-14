//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLMUEXCHANGE_H
#define __INET_HEDLMUEXCHANGE_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/HeDlMuExchangeTypes.h"

namespace inet {
namespace ieee80211 {

/** Committed semantic identity and outcome ledger for one HE DL MU exchange. */
class INET_API HeDlMuExchange
{
  private:
    uint64_t id = 0;
    Packet *containerPacket = nullptr;
    std::vector<HeDlMuMember> members;
    std::vector<bool> transmittedMembers;
    std::vector<MacAddress> users;
    std::vector<bool> completedUsers;

    int findMember(const HeDlMuMember& member) const;
    int findUser(const MacAddress& peer) const;

  public:
    HeDlMuExchange(uint64_t id, Packet *containerPacket,
            std::vector<HeDlMuMember> members);

    HeDlMuExchange(const HeDlMuExchange&) = delete;
    HeDlMuExchange& operator=(const HeDlMuExchange&) = delete;
    HeDlMuExchange(HeDlMuExchange&&) = default;
    HeDlMuExchange& operator=(HeDlMuExchange&&) = default;

    uint64_t getId() const { return id; }
    Packet *getContainerPacket() const { return containerPacket; }
    const std::vector<HeDlMuMember>& getMembers() const { return members; }
    bool isContainer(const Packet *packet) const
        { return packet != nullptr && packet == containerPacket; }

    bool recordMemberTransmitted(const HeDlMuMember& member);
    bool recordUserOutcome(const MacAddress& peer);
    bool isComplete() const;
};

} // namespace ieee80211
} // namespace inet

#endif
