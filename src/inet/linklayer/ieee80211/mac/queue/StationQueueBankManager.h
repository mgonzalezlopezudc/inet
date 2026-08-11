//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_STATIONQUEUEBANKMANAGER_H
#define __INET_STATIONQUEUEBANKMANAGER_H

#include <cstdint>
#include <map>
#include <utility>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/queue/StationQueueBank.h"

namespace inet {
namespace ieee80211 {

/**
 * Manages the dynamic creation and destruction of per-STA queue banks.
 * 
 * Responsibilities:
 * - Create a StationQueueBank for each associated STA
 * - Route packets to the correct STA's queue based on destination MAC
 * - Destroy queue banks on STA disassociation
 * - Provide per-AC queue access to the HE coordination function
 */
class INET_API StationQueueBankManager
{
  public:
    // Map of STA MAC address to queue bank
    using StationQueueBankMap = std::map<MacAddress, StationQueueBank *>;
    using AssociationKey = std::pair<MacAddress, uint64_t>;

  protected:
    StationQueueBankMap banks;
    std::map<MacAddress, uint64_t> epochs;
    std::map<AssociationKey, StationQueueBank *> retiredBanks;
    cModule *queueBanksModule = nullptr;
    cModuleType *queueBankType = nullptr;

  public:
    StationQueueBankManager(cModule *queueBanksModule);
    virtual ~StationQueueBankManager();

    // Create/destroy queue banks
    virtual StationQueueBank *ensureQueueBank(const MacAddress& staAddress, uint64_t associationEpoch);
    virtual bool retireQueueBank(const MacAddress& staAddress, uint64_t associationEpoch);
    virtual void finalizeRetiredQueueBanks();
    virtual StationQueueBank *getQueueBank(const MacAddress &staAddr) const;
    virtual uint64_t getAssociationEpoch(const MacAddress& staAddress) const;

    // Query methods
    virtual bool hasQueueBank(const MacAddress &staAddr) const;
    virtual int getQueueBankCount() const { return banks.size(); }
    virtual const StationQueueBankMap &getQueueBanks() const { return banks; }

    // Statistics
    virtual int getTotalQueuedPackets() const;
    virtual int getTotalQueuedBytes() const;

    // Cleanup
    virtual void clear();
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
