//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/queue/StationQueueBankManager.h"

#include "inet/common/ModuleAccess.h"

namespace inet {
namespace ieee80211 {

StationQueueBankManager::StationQueueBankManager(cModule *queueBanksModule)
    : queueBanksModule(queueBanksModule)
{
    ASSERT(queueBanksModule);
    queueBankType = cModuleType::get("inet.linklayer.ieee80211.mac.queue.StationQueueBank");
}

StationQueueBankManager::~StationQueueBankManager()
{
    // The banks are submodules of queueBanksModule and are deleted by OMNeT++ as
    // part of the normal module teardown. Deleting them from this member
    // destructor would race the parent cModule destructor.
    banks.clear();
}

StationQueueBank *StationQueueBankManager::ensureQueueBank(const MacAddress& staAddress, uint64_t associationEpoch)
{
    if (associationEpoch == 0)
        throw cRuntimeError("Cannot create a queue bank with association epoch 0");
    auto existing = banks.find(staAddress);
    if (existing != banks.end()) {
        if (epochs.at(staAddress) == associationEpoch)
            return existing->second;
        retireQueueBank(staAddress, epochs.at(staAddress));
    }

    std::string bankName = "queueBank_" + staAddress.str() + "_" + std::to_string(associationEpoch);
    StationQueueBank *bank = check_and_cast<StationQueueBank *>(
        queueBankType->create(bankName.c_str(), queueBanksModule));
    bank->par("staAddress").setStringValue(staAddress.str().c_str());
    bank->finalizeParameters();
    bank->buildInside();
    bank->scheduleStart(simTime());
    bank->callInitialize();

    banks[staAddress] = bank;
    epochs[staAddress] = associationEpoch;
    
    EV_INFO << "Created queue bank for STA " << staAddress << " association epoch " << associationEpoch << "\n";
    
    return bank;
}

bool StationQueueBankManager::retireQueueBank(const MacAddress& staAddress, uint64_t associationEpoch)
{
    auto it = banks.find(staAddress);
    if (it == banks.end() || epochs.at(staAddress) != associationEpoch)
        return false;
    retiredBanks[{staAddress, associationEpoch}] = it->second;
    banks.erase(it);
    epochs.erase(staAddress);
    EV_INFO << "Retired queue bank for STA " << staAddress << " association epoch " << associationEpoch << "\n";
    return true;
}

void StationQueueBankManager::finalizeRetiredQueueBanks()
{
    for (const auto& entry : retiredBanks) {
        auto bank = entry.second;
        bank->clear();
        bank->callFinish();
        check_and_cast<cModule *>(bank)->deleteModule();
    }
    retiredBanks.clear();
}

uint64_t StationQueueBankManager::getAssociationEpoch(const MacAddress& staAddress) const
{
    auto it = epochs.find(staAddress);
    return it == epochs.end() ? 0 : it->second;
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
    for (const auto& entry : banks)
        retiredBanks[{entry.first, epochs.at(entry.first)}] = entry.second;
    banks.clear();
    epochs.clear();
    finalizeRetiredQueueBanks();
}

} /* namespace ieee80211 */
} /* namespace inet */
