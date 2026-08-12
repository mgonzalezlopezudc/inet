//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211ASSOCIATIONSTATE_H
#define __INET_IEEE80211ASSOCIATIONSTATE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {

namespace ieee80211 {

class INET_API Ieee80211AssociationState final
{
  public:
    enum BssStationType {
        ACCESS_POINT,
        STATION
    };

    enum BssMemberStatus {
        NOT_AUTHENTICATED,
        AUTHENTICATED,
        ASSOCIATED
    };

    class INET_API LocalSnapshot final {
      private:
        BssStationType stationType;
        bool associated;
        short associationId;
        std::string ssid;
        MacAddress bssid;
        uint64_t generation;

      public:
        LocalSnapshot(BssStationType stationType, bool associated, short associationId,
                const std::string& ssid, const MacAddress& bssid, uint64_t generation) :
            stationType(stationType), associated(associated), associationId(associationId),
            ssid(ssid), bssid(bssid), generation(generation) {}

        BssStationType getStationType() const { return stationType; }
        bool isAssociated() const { return associated; }
        short getAssociationId() const { return associationId; }
        const std::string& getSsid() const { return ssid; }
        const MacAddress& getBssid() const { return bssid; }
        uint64_t getGeneration() const { return generation; }
    };

    class INET_API PeerSnapshot final {
      private:
        MacAddress address;
        bool memberStatusPresent;
        bool associationIdPresent;
        BssMemberStatus memberStatus;
        short associationId;
        uint64_t generation;
        uint64_t associationEpoch;

      public:
        PeerSnapshot(const MacAddress& address, bool memberStatusPresent, bool associationIdPresent,
                BssMemberStatus memberStatus, short associationId, uint64_t generation, uint64_t associationEpoch) :
            address(address), memberStatusPresent(memberStatusPresent), associationIdPresent(associationIdPresent),
            memberStatus(memberStatus), associationId(associationId), generation(generation), associationEpoch(associationEpoch) {}

        const MacAddress& getAddress() const { return address; }
        bool isPresent() const { return memberStatusPresent || associationIdPresent; }
        bool hasMemberStatus() const { return memberStatusPresent; }
        bool hasAssociationId() const { return associationIdPresent; }
        BssMemberStatus getMemberStatus() const { return memberStatus; }
        short getAssociationId() const { return associationId; }
        uint64_t getGeneration() const { return generation; }
        uint64_t getAssociationEpoch() const { return associationEpoch; }
    };

    class INET_API PeerTransition final {
      private:
        PeerSnapshot oldSnapshot;
        PeerSnapshot newSnapshot;
        uint64_t associationEpoch;

      public:
        PeerTransition(const PeerSnapshot& oldSnapshot, const PeerSnapshot& newSnapshot,
                uint64_t associationEpoch) :
            oldSnapshot(oldSnapshot), newSnapshot(newSnapshot), associationEpoch(associationEpoch) {}

        const PeerSnapshot& getOldSnapshot() const { return oldSnapshot; }
        const PeerSnapshot& getNewSnapshot() const { return newSnapshot; }
        uint64_t getAssociationEpoch() const { return associationEpoch; }
    };

  private:
    std::string ssid;
    MacAddress bssid;
    BssStationType stationType = static_cast<BssStationType>(-1);
    bool associated = false;
    short associationId = -1;
    uint64_t localGeneration = 0;
    std::map<MacAddress, BssMemberStatus> peerMemberStatuses;
    std::map<MacAddress, short> peerAssociationIds;
    std::map<MacAddress, short> peerAssociationReservations;
    std::map<MacAddress, uint64_t> peerGenerations;
    std::map<MacAddress, uint64_t> peerAssociationEpochs;
    uint64_t nextAssociationEpoch = 0;

  private:
    static void advanceGeneration(uint64_t& generation);

  public:
    LocalSnapshot getLocalSnapshot() const;
    PeerSnapshot getPeerSnapshot(const MacAddress& address) const;
    std::vector<PeerSnapshot> getPeerSnapshots() const;

    void setStationType(BssStationType stationType);
    void setBssIdentity(const std::string& ssid, const MacAddress& bssid);
    void installLocalAssociation(const std::string& ssid, const MacAddress& bssid, short associationId);
    void clearLocalAssociation();
    void setPeerMemberStatus(const MacAddress& address, BssMemberStatus memberStatus);
    short reservePeerAssociation(const MacAddress& address);
    void releasePeerAssociationReservation(const MacAddress& address, short associationId);
    PeerTransition commitPeerAssociation(const MacAddress& address);
    PeerTransition commitPeerAssociation(const MacAddress& address, short associationId);
    PeerTransition clearPeerAssociation(const MacAddress& address, BssMemberStatus memberStatus);
    short getAssociationId(const MacAddress& address) const;
    MacAddress getStationAddress(short associationId) const;
};

class INET_API IIeee80211PeerAssociationListener
{
  public:
    virtual ~IIeee80211PeerAssociationListener() {}
    virtual void peerAssociationChanged(
            const Ieee80211AssociationState::PeerTransition& transition) = 0;
};

} // namespace ieee80211

} // namespace inet

#endif
