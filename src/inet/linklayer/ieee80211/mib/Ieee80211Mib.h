//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MIB_H
#define __INET_IEEE80211MIB_H

#include "inet/common/SimpleModule.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {

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
    };

    class INET_API BssAccessPointData {
      public:
        std::map<MacAddress, BssMemberStatus> stations;
        struct LinkData {
            double transmitPowerDbm = 15;
            double receivedPowerDbm = NaN;
            double pathLossDb = NaN;
            simtime_t lastUpdate = SIMTIME_ZERO;
            bool valid = false;
        };
        std::map<MacAddress, LinkData> links;
    };

  public:
    MacAddress address;
    Mode mode = static_cast<Mode>(-1);
    bool qos = false;

    BssData bssData;
    BssStationData bssStationData;
    BssAccessPointData bssAccessPointData;

  protected:
    virtual void initialize(int stage) override;

  public:
    static const char *getModeStr(Ieee80211Mib::Mode mode);
    static const char *getStationTypeStr(Ieee80211Mib::BssStationType stationType);
    std::string getSsidStr() const;
    void setStationTransmitPower(const MacAddress& address, double transmitPowerDbm);
    void updateStationReceivedPower(const MacAddress& address, units::values::W receivedPower);
    const BssAccessPointData::LinkData *findStationLink(const MacAddress& address) const;
};

} // namespace ieee80211

} // namespace inet

#endif
