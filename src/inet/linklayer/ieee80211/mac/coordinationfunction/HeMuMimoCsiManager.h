//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEMUMIMOCSIMANAGER_H
#define __INET_HEMUMIMOCSIMANAGER_H

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <functional>
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

class HeMuMimoCsiManager
{
  public:
    struct CsiEntry {
        simtime_t acquisitionTime = SIMTIME_ZERO;
        simtime_t expiryTime = SIMTIME_ZERO;
        bool valid = false;
        std::map<MacAddress, double> leakages; // Map co-scheduled peer -> leakage
    };

  protected:
    simtime_t validityDuration = SimTime(0.1);
    double defaultLeakage = 0.1;
    std::map<std::pair<int, int>, double> overridesMap;
    std::map<std::pair<MacAddress, Hz>, CsiEntry> csiTable;
    std::function<simtime_t()> timeProvider = []() { return simTime(); };

  public:
    HeMuMimoCsiManager() {}
    virtual ~HeMuMimoCsiManager() {}

    void setTimeProvider(std::function<simtime_t()> provider) { timeProvider = provider; }

    void configure(simtime_t validityDuration, double defaultLeakage, const std::string& overridesStr)
    {
        this->validityDuration = validityDuration;
        this->defaultLeakage = defaultLeakage;
        overridesMap.clear();
        std::stringstream ss(overridesStr);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (token.empty())
                continue;
            int aidA = 0, aidB = 0;
            double val = 0.0;
            if (sscanf(token.c_str(), "%d-%d:%lf", &aidA, &aidB, &val) == 3) {
                overridesMap[{aidA, aidB}] = val;
                overridesMap[{aidB, aidA}] = val;
            }
        }
    }

    void updateCsi(const MacAddress& address, Hz bandwidth,
                   const std::vector<MacAddress>& allAssociatedStations,
                   const std::function<int(const MacAddress&)>& getAid)
    {
        CsiEntry entry;
        entry.acquisitionTime = timeProvider();
        entry.expiryTime = timeProvider() + validityDuration;
        entry.valid = true;
        int aidU = getAid(address);
        for (const auto& other : allAssociatedStations) {
            if (other == address)
                continue;
            int aidV = getAid(other);
            double leakage = defaultLeakage;
            auto it = overridesMap.find({aidU, aidV});
            if (it != overridesMap.end()) {
                leakage = it->second;
            }
            entry.leakages[other] = leakage;
        }
        csiTable[{address, bandwidth}] = entry;
    }

    void invalidateCsi(const MacAddress& address, Hz bandwidth)
    {
        csiTable.erase({address, bandwidth});
    }

    bool hasFreshCsi(const MacAddress& address, Hz bandwidth) const
    {
        auto it = csiTable.find({address, bandwidth});
        if (it == csiTable.end())
            return false;
        return it->second.valid && timeProvider() <= it->second.expiryTime;
    }

    double getLeakage(const MacAddress& selectedSta, const MacAddress& coScheduledSta, Hz bandwidth) const
    {
        auto it = csiTable.find({selectedSta, bandwidth});
        if (it == csiTable.end())
            return defaultLeakage;
        auto lit = it->second.leakages.find(coScheduledSta);
        if (lit == it->second.leakages.end())
            return defaultLeakage;
        return lit->second;
    }

    void clear()
    {
        csiTable.clear();
    }
};

} // namespace ieee80211
} // namespace inet

#endif
