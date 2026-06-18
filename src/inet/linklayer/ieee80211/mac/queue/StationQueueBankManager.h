//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_STATIONQUEUEBANKMANAGER_H
#define __INET_STATIONQUEUEBANKMANAGER_H

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
 * - Provide aggregate facades for EDCA per-AC access
 */
class INET_API StationQueueBankManager
{
  public:
    // Map of STA MAC address to queue bank
    using StationQueueBankMap = std::map<MacAddress, StationQueueBank *>;

  protected:
    StationQueueBankMap banks;
    cModule *ownerModule = nullptr;
    cModuleType *queueBankType = nullptr;

  public:
    StationQueueBankManager(cModule *owner);
    virtual ~StationQueueBankManager();

    // Create/destroy queue banks
    virtual StationQueueBank *createQueueBank(const MacAddress &staAddr);
    virtual void destroyQueueBank(const MacAddress &staAddr);
    virtual StationQueueBank *getQueueBank(const MacAddress &staAddr) const;

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
