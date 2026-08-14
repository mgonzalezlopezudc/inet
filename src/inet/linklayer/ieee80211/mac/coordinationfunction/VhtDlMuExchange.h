//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTDLMUEXCHANGE_H
#define __INET_VHTDLMUEXCHANGE_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/VhtDlMuExchangeTypes.h"

namespace inet {

class Packet;

namespace ieee80211 {

/** Committed semantic identity and outcome ledger for one VHT DL MU exchange. */
class INET_API VhtDlMuExchange
{
  private:
    VhtDlMuExchangeId id = NO_VHT_DL_MU_EXCHANGE;
    Packet *containerPacket = nullptr;
    std::vector<std::vector<Packet *>> userPackets;
    std::vector<std::vector<bool>> failedPackets;
    std::vector<bool> completedUsers;

  public:
    VhtDlMuExchange(VhtDlMuExchangeId id, Packet *containerPacket,
            std::vector<std::vector<Packet *>> userPackets);

    VhtDlMuExchange(const VhtDlMuExchange&) = delete;
    VhtDlMuExchange& operator=(const VhtDlMuExchange&) = delete;
    VhtDlMuExchange(VhtDlMuExchange&&) = default;
    VhtDlMuExchange& operator=(VhtDlMuExchange&&) = default;

    VhtDlMuExchangeId getId() const { return id; }
    Packet *getContainerPacket() const { return containerPacket; }
    const std::vector<std::vector<Packet *>>& getUserPackets() const
        { return userPackets; }

    bool containsPacket(const Packet *packet) const;
    bool recordFailedPacket(const Packet *packet);
    bool recordUserResult(unsigned int userIndex,
            VhtDlMuUserResult result);
    bool isComplete() const;
};

} // namespace ieee80211
} // namespace inet

#endif
