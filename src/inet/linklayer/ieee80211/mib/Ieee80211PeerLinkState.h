//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211PEERLINKSTATE_H
#define __INET_IEEE80211PEERLINKSTATE_H

#include <map>
#include <optional>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

class INET_API Ieee80211PeerLinkState final
{
  public:
    class Snapshot final
    {
      private:
        MacAddress address;
        double transmitPowerDbm = 15;
        double receivedPowerDbm = NaN;
        double pathLossDb = NaN;
        simtime_t lastUpdate = SIMTIME_ZERO;
        bool valid = false;
        uint64_t generation = 0;

      public:
        Snapshot() = default;
        Snapshot(const MacAddress& address, double transmitPowerDbm, double receivedPowerDbm,
                double pathLossDb, simtime_t lastUpdate, bool valid, uint64_t generation) :
            address(address), transmitPowerDbm(transmitPowerDbm), receivedPowerDbm(receivedPowerDbm),
            pathLossDb(pathLossDb), lastUpdate(lastUpdate), valid(valid), generation(generation) {}
        const MacAddress& getAddress() const { return address; }
        double getTransmitPowerDbm() const { return transmitPowerDbm; }
        double getReceivedPowerDbm() const { return receivedPowerDbm; }
        double getPathLossDb() const { return pathLossDb; }
        simtime_t getLastUpdate() const { return lastUpdate; }
        bool isValid() const { return valid; }
        uint64_t getGeneration() const { return generation; }
    };

  private:
    struct Record {
        double transmitPowerDbm = 15;
        double receivedPowerDbm = NaN;
        double pathLossDb = NaN;
        simtime_t lastUpdate = SIMTIME_ZERO;
        bool valid = false;
        uint64_t generation = 0;
    };
    std::map<MacAddress, Record> records;

  private:
    static void advanceGeneration(uint64_t& generation);

  public:
    void setTransmitPower(const MacAddress& address, double transmitPowerDbm);
    void updateReceivedPower(const MacAddress& address, units::values::W receivedPower, simtime_t updateTime);
    std::optional<Snapshot> getSnapshot(const MacAddress& address) const;
    std::vector<Snapshot> getSnapshots() const;
};

} // namespace ieee80211
} // namespace inet

#endif
