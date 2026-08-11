//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211PEERCAPABILITYSTATE_H
#define __INET_IEEE80211PEERCAPABILITYSTATE_H

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211EhtCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HtCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211VhtCapabilities.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"

namespace inet {
namespace ieee80211 {

class INET_API Ieee80211PeerCapabilityState final
{
  public:
    class Snapshot final
    {
      private:
        MacAddress address;
        std::optional<Ieee80211HtCapabilities> advertisedHt;
        std::optional<Ieee80211NegotiatedHtCapabilities> negotiatedHt;
        std::optional<Ieee80211VhtCapabilities> advertisedVht;
        std::optional<Ieee80211NegotiatedVhtCapabilities> negotiatedVht;
        std::optional<Ieee80211HeCapabilities> advertisedHe;
        std::optional<Ieee80211NegotiatedHeCapabilities> negotiatedHe;
        std::optional<Ieee80211EhtCapabilities> advertisedEht;
        std::optional<Ieee80211NegotiatedEhtCapabilities> negotiatedEht;
        std::optional<std::vector<Ieee80211LegacyRate>> legacyRates;
        uint64_t generation = 0;
        uint64_t htGeneration = 0;
        uint64_t vhtGeneration = 0;
        uint64_t heGeneration = 0;
        uint64_t ehtGeneration = 0;
        uint64_t legacyRatesGeneration = 0;

      public:
        Snapshot() = default;
        Snapshot(const MacAddress& address,
                const std::optional<Ieee80211HtCapabilities>& advertisedHt,
                const std::optional<Ieee80211NegotiatedHtCapabilities>& negotiatedHt,
                const std::optional<Ieee80211VhtCapabilities>& advertisedVht,
                const std::optional<Ieee80211NegotiatedVhtCapabilities>& negotiatedVht,
                const std::optional<Ieee80211HeCapabilities>& advertisedHe,
                const std::optional<Ieee80211NegotiatedHeCapabilities>& negotiatedHe,
                const std::optional<Ieee80211EhtCapabilities>& advertisedEht,
                const std::optional<Ieee80211NegotiatedEhtCapabilities>& negotiatedEht,
                const std::optional<std::vector<Ieee80211LegacyRate>>& legacyRates,
                uint64_t generation, uint64_t htGeneration, uint64_t vhtGeneration,
                uint64_t heGeneration, uint64_t ehtGeneration, uint64_t legacyRatesGeneration) :
            address(address), advertisedHt(advertisedHt), negotiatedHt(negotiatedHt),
            advertisedVht(advertisedVht), negotiatedVht(negotiatedVht),
            advertisedHe(advertisedHe), negotiatedHe(negotiatedHe), advertisedEht(advertisedEht),
            negotiatedEht(negotiatedEht), legacyRates(legacyRates), generation(generation),
            htGeneration(htGeneration), vhtGeneration(vhtGeneration), heGeneration(heGeneration),
            ehtGeneration(ehtGeneration), legacyRatesGeneration(legacyRatesGeneration) {}
        const MacAddress& getAddress() const { return address; }
        const auto& getAdvertisedHt() const { return advertisedHt; }
        const auto& getNegotiatedHt() const { return negotiatedHt; }
        const auto& getAdvertisedVht() const { return advertisedVht; }
        const auto& getNegotiatedVht() const { return negotiatedVht; }
        const auto& getAdvertisedHe() const { return advertisedHe; }
        const auto& getNegotiatedHe() const { return negotiatedHe; }
        const auto& getAdvertisedEht() const { return advertisedEht; }
        const auto& getNegotiatedEht() const { return negotiatedEht; }
        const auto& getLegacyRates() const { return legacyRates; }
        uint64_t getGeneration() const { return generation; }
        uint64_t getHtGeneration() const { return htGeneration; }
        uint64_t getVhtGeneration() const { return vhtGeneration; }
        uint64_t getHeGeneration() const { return heGeneration; }
        uint64_t getEhtGeneration() const { return ehtGeneration; }
        uint64_t getLegacyRatesGeneration() const { return legacyRatesGeneration; }
        bool isPresent() const;
    };

  private:
    struct Record {
        std::optional<Ieee80211HtCapabilities> advertisedHt;
        std::optional<Ieee80211NegotiatedHtCapabilities> negotiatedHt;
        std::optional<Ieee80211VhtCapabilities> advertisedVht;
        std::optional<Ieee80211NegotiatedVhtCapabilities> negotiatedVht;
        std::optional<Ieee80211HeCapabilities> advertisedHe;
        std::optional<Ieee80211NegotiatedHeCapabilities> negotiatedHe;
        std::optional<Ieee80211EhtCapabilities> advertisedEht;
        std::optional<Ieee80211NegotiatedEhtCapabilities> negotiatedEht;
        std::optional<std::vector<Ieee80211LegacyRate>> legacyRates;
        std::string htKey;
        std::string vhtKey;
        std::string ratesKey;
        uint64_t generation = 0;
        uint64_t htGeneration = 0;
        uint64_t vhtGeneration = 0;
        uint64_t heGeneration = 0;
        uint64_t ehtGeneration = 0;
        uint64_t legacyRatesGeneration = 0;
    };
    std::map<MacAddress, Record> records;

  private:
    static void advanceGeneration(uint64_t& generation);
    static void advance(Record& record, uint64_t *domainGeneration = nullptr);

  public:
    Snapshot getSnapshot(const MacAddress& address) const;
    std::vector<Snapshot> getSnapshots() const;
    void setHt(const MacAddress& address, const Ieee80211HtCapabilities& advertised, const Ieee80211NegotiatedHtCapabilities& negotiated);
    void setVht(const MacAddress& address, const Ieee80211VhtCapabilities& advertised, const Ieee80211NegotiatedVhtCapabilities& negotiated);
    void setHe(const MacAddress& address, const Ieee80211HeCapabilities& advertised, const Ieee80211NegotiatedHeCapabilities& negotiated);
    void setEht(const MacAddress& address, const Ieee80211EhtCapabilities& advertised, const Ieee80211NegotiatedEhtCapabilities& negotiated);
    void setLegacyRates(const MacAddress& address, const std::vector<Ieee80211LegacyRate>& rates);
    void removeHt(const MacAddress& address);
    void removeVht(const MacAddress& address);
    void removeHe(const MacAddress& address);
    void removeEht(const MacAddress& address);
    void removeAll(const MacAddress& address);
};

} // namespace ieee80211
} // namespace inet

#endif
