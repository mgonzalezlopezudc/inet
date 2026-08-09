//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MIB_H
#define __INET_IEEE80211MIB_H

#include <ostream>

#include "inet/common/SimpleModule.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211EhtCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HtCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211VhtCapabilities.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"

namespace inet {

namespace physicallayer {
class IIeee80211Mode;
class Ieee80211ModeSet;
class IIeee80211ModeSetProvider;
}

namespace ieee80211 {

class INET_API Ieee80211Mib : public SimpleModule
{
  public:
    enum Mode {
        INFRASTRUCTURE,
        INDEPENDENT,
        MESH
    };

    enum BssStationType {
        ACCESS_POINT,
        STATION
    };

    enum BssMemberStatus {
        NOT_AUTHENTICATED,
        AUTHENTICATED,
        ASSOCIATED
    };

    class INET_API BssData {
      public:
        std::string ssid;
        MacAddress bssid;
    };

    class INET_API BssStationData {
      public:
        BssStationType stationType = static_cast<BssStationType>(-1);
        bool isAssociated = false;
        short associationId = -1;
    };

    class INET_API BssAccessPointData {
      public:
        std::map<MacAddress, BssMemberStatus> stations;
        std::map<MacAddress, short> associationIds;
        struct LinkData {
            double transmitPowerDbm = 15;
            double receivedPowerDbm = NaN;
            double pathLossDb = NaN;
            simtime_t lastUpdate = SIMTIME_ZERO;
            bool valid = false;
        };
        std::map<MacAddress, LinkData> links;
        std::map<MacAddress, Ieee80211HeCapabilities> advertisedHeCapabilities;
        std::map<MacAddress, Ieee80211NegotiatedHeCapabilities> negotiatedHeCapabilities;
        std::map<MacAddress, Ieee80211EhtCapabilities> advertisedEhtCapabilities;
        std::map<MacAddress, Ieee80211NegotiatedEhtCapabilities> negotiatedEhtCapabilities;
        std::map<MacAddress, Ieee80211VhtCapabilities> advertisedVhtCapabilities;
        std::map<MacAddress, Ieee80211NegotiatedVhtCapabilities> negotiatedVhtCapabilities;
        std::map<MacAddress, uint64_t> vhtAssociationGenerations;
        std::map<MacAddress, Ieee80211HtCapabilities> advertisedHtCapabilities;
        std::map<MacAddress, Ieee80211NegotiatedHtCapabilities> negotiatedHtCapabilities;
        std::map<MacAddress, uint64_t> htAssociationGenerations;
        std::map<MacAddress, std::vector<Ieee80211LegacyRate>> advertisedLegacyRates;
    };

  public:
    MacAddress address;
    Mode mode = static_cast<Mode>(-1);
    bool qos = false;

    BssData bssData;
    BssStationData bssStationData;
    BssAccessPointData bssAccessPointData;
    Ieee80211EhtCapabilities localEhtCapabilities;
    Ieee80211EhtOperation ehtOperation;
    Ieee80211HeCapabilities localHeCapabilities;
    Ieee80211HeOperation heOperation;
    Ieee80211VhtCapabilities localVhtCapabilities;
    Ieee80211VhtOperation vhtOperation;
    Ieee80211HtCapabilities localHtCapabilities;
    Ieee80211HtOperation htOperation;
    bool localHtLdpc = false;
    std::vector<Ieee80211LegacyRate> localOperationalRates;
    std::vector<Ieee80211LegacyRate> localBssBasicRates;
    std::vector<Ieee80211LegacyRate> currentBssBasicRates;

  protected:
    physicallayer::IIeee80211ModeSetProvider *modeSetProvider = nullptr;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void updateLocalOperationalRates(const physicallayer::Ieee80211ModeSet *modeSet);
    virtual void updateLocalOperationalRates(const physicallayer::Ieee80211ModeSet *modeSet, const std::string& configuredBasicRateCodes);
    virtual int getNegotiatedHePeerCount() const;
    virtual int getNegotiatedEhtPeerCount() const;
    virtual std::string getHeCapabilitiesSummary() const;
    virtual std::string getHeOperationSummary() const;
    virtual std::string getEhtCapabilitiesSummary() const;
    virtual std::string getEhtOperationSummary() const;

  public:
    static const char *getModeStr(Ieee80211Mib::Mode mode);
    static const char *getStationTypeStr(Ieee80211Mib::BssStationType stationType);
    std::string getSsidStr() const;
    short allocateAssociationId(const MacAddress& address);
    void releaseAssociationId(const MacAddress& address);
    void setStationTransmitPower(const MacAddress& address, double transmitPowerDbm);
    void updateStationReceivedPower(const MacAddress& address, units::values::W receivedPower);
    const BssAccessPointData::LinkData *findStationLink(const MacAddress& address) const;
    short allocateAssociationId(const MacAddress& address);
    void releaseAssociationId(const MacAddress& address);
    short getAssociationId(const MacAddress& address) const;
    MacAddress getStationAddress(short associationId) const;
    void setPeerHeCapabilities(const MacAddress& address, const Ieee80211HeCapabilities& capabilities,
            const Ieee80211HeOperation& operation);
    void removePeerHeCapabilities(const MacAddress& address);
    void removePeerCapabilities(const MacAddress& address);
    Ieee80211SupportedRatesElement getSupportedRatesElement() const;
    Ieee80211ExtendedSupportedRatesElement getExtendedSupportedRatesElement() const;
    void setPeerLegacyRates(const MacAddress& address,
            const Ieee80211SupportedRatesElement& supportedRates,
            const Ieee80211ExtendedSupportedRatesElement& extendedSupportedRates);
    const std::vector<Ieee80211LegacyRate> *findPeerLegacyRates(const MacAddress& address) const;
    std::vector<Ieee80211LegacyRate> getBssBasicLegacyRates() const;
    void installCurrentBssBasicLegacyRates(const Ieee80211SupportedRatesElement& supportedRates,
            const Ieee80211ExtendedSupportedRatesElement& extendedSupportedRates);
    void clearCurrentBssBasicLegacyRates() { currentBssBasicRates.clear(); }
    const Ieee80211NegotiatedHeCapabilities *findNegotiatedHeCapabilities(const MacAddress& address) const;
    bool isHeModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    void setPeerEhtCapabilities(const MacAddress& address, const Ieee80211EhtCapabilities& capabilities,
            const Ieee80211EhtOperation& operation);
    void removePeerEhtCapabilities(const MacAddress& address);
    const Ieee80211NegotiatedEhtCapabilities *findNegotiatedEhtCapabilities(const MacAddress& address) const;
    bool isEhtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    bool isHtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    bool isVhtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    bool isLdpcAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    void setPeerVhtCapabilities(const MacAddress& address, const Ieee80211VhtCapabilities& capabilities,
            const Ieee80211VhtOperation& operation);
    void removePeerVhtCapabilities(const MacAddress& address);
    const Ieee80211NegotiatedVhtCapabilities *findNegotiatedVhtCapabilities(const MacAddress& address) const;
    uint64_t getVhtAssociationGeneration(const MacAddress& address) const;
    void setPeerHtCapabilities(const MacAddress& address, const Ieee80211HtCapabilities& capabilities,
            const Ieee80211HtOperation& operation);
    void removePeerHtCapabilities(const MacAddress& address);
    const Ieee80211NegotiatedHtCapabilities *findNegotiatedHtCapabilities(const MacAddress& address) const;
    uint64_t getHtAssociationGeneration(const MacAddress& address) const;
};

inline std::ostream& operator<<(std::ostream& os, const Ieee80211Mib::BssAccessPointData::LinkData& link)
{
    os << "tx=" << link.transmitPowerDbm
       << "dBm rx=" << link.receivedPowerDbm
       << "dBm pathLoss=" << link.pathLossDb
       << "dB updated=" << link.lastUpdate
       << " valid=" << (link.valid ? "yes" : "no");
    return os;
}

} // namespace ieee80211

} // namespace inet

#endif
