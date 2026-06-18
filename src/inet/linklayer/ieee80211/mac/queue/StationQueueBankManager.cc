//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/queue/StationQueueBankManager.h"

#include "inet/common/ModuleAccess.h"

namespace inet {
namespace ieee80211 {

StationQueueBankManager::StationQueueBankManager(cModule *owner)
    : ownerModule(owner)
{
    ASSERT(ownerModule);
    queueBankType = cModuleType::get("inet.linklayer.ieee80211.mac.queue.StationQueueBank");
}

StationQueueBankManager::~StationQueueBankManager()
{
    // The banks are submodules of ownerModule and are deleted by OMNeT++ as
    // part of the normal module teardown. Deleting them from this member
    // destructor would race the parent cModule destructor.
    banks.clear();
}

StationQueueBank *StationQueueBankManager::createQueueBank(const MacAddress &staAddr)
{
    if (banks.find(staAddr) != banks.end()) {
        throw cRuntimeError("Queue bank already exists for STA %s", staAddr.str().c_str());
    }

    std::string bankName = "queueBank_" + staAddr.str();
    StationQueueBank *bank = check_and_cast<StationQueueBank *>(
        queueBankType->create(bankName.c_str(), ownerModule));
    bank->par("staAddress").setStringValue(staAddr.str().c_str());
    bank->finalizeParameters();
    bank->buildInside();
    bank->scheduleStart(simTime());
    bank->callInitialize();

    banks[staAddr] = bank;
    
    EV_INFO << "Created queue bank for STA " << staAddr << "\n";
    
    return bank;
}

void StationQueueBankManager::destroyQueueBank(const MacAddress &staAddr)
{
    auto it = banks.find(staAddr);
    if (it == banks.end()) {
        EV_WARN << "Queue bank not found for STA " << staAddr << "\n";
        return;
    }

    StationQueueBank *bank = it->second;
    bank->clear();  // Drop all queued packets
    bank->callFinish();
    
    cModule *bankModule = check_and_cast<cModule *>(bank);
    bankModule->deleteModule();
    
    banks.erase(it);
    
    EV_INFO << "Destroyed queue bank for STA " << staAddr << "\n";
}

StationQueueBank *StationQueueBankManager::getQueueBank(const MacAddress &staAddr) const
{
    auto it = banks.find(staAddr);
    if (it != banks.end())
        return it->second;
    return nullptr;
}

bool StationQueueBankManager::hasQueueBank(const MacAddress &staAddr) const
{
    return banks.find(staAddr) != banks.end();
}

int StationQueueBankManager::getTotalQueuedPackets() const
{
    int total = 0;
    for (const auto &pair : banks) {
        total += pair.second->getTotalQueuedPackets();
    }
    return total;
}

int StationQueueBankManager::getTotalQueuedBytes() const
{
    int total = 0;
    for (const auto &pair : banks) {
        total += pair.second->getTotalQueuedBytes();
    }
    return total;
}

void StationQueueBankManager::clear()
{
    for (auto it = banks.begin(); it != banks.end(); ) {
        StationQueueBank *bank = it->second;
        bank->clear();
        bank->callFinish();
        
        cModule *bankModule = check_and_cast<cModule *>(bank);
        bankModule->deleteModule();
        
        it = banks.erase(it);
    }
}

} /* namespace ieee80211 */
} /* namespace inet */
