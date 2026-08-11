//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MIB_H
#define __INET_IEEE80211MIB_H

#include <ostream>
#include <optional>

#include "inet/common/SimpleModule.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211AssociationState.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211PeerCapabilityState.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211PeerLinkState.h"
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

class INET_API Ieee80211PeerAssociationChangedEvent final : public cObject
{
  private:
    Ieee80211AssociationState::PeerTransition transition;

  public:
    explicit Ieee80211PeerAssociationChangedEvent(
            const Ieee80211AssociationState::PeerTransition& transition) : transition(transition) {}

    const Ieee80211AssociationState::PeerSnapshot& getOldSnapshot() const { return transition.getOldSnapshot(); }
    const Ieee80211AssociationState::PeerSnapshot& getNewSnapshot() const { return transition.getNewSnapshot(); }
    uint64_t getAssociationEpoch() const { return transition.getAssociationEpoch(); }
};

class INET_API Ieee80211Mib : public SimpleModule
{
  public:
    static simsignal_t peerAssociationChangedSignal;

    enum Mode {
        INFRASTRUCTURE,
        INDEPENDENT,
        MESH
    };

    using BssStationType = Ieee80211AssociationState::BssStationType;
    using BssMemberStatus = Ieee80211AssociationState::BssMemberStatus;
    using LocalAssociationSnapshot = Ieee80211AssociationState::LocalSnapshot;
    using PeerAssociationSnapshot = Ieee80211AssociationState::PeerSnapshot;

    static constexpr BssStationType ACCESS_POINT = Ieee80211AssociationState::ACCESS_POINT;
    static constexpr BssStationType STATION = Ieee80211AssociationState::STATION;
    static constexpr BssMemberStatus NOT_AUTHENTICATED = Ieee80211AssociationState::NOT_AUTHENTICATED;
    static constexpr BssMemberStatus AUTHENTICATED = Ieee80211AssociationState::AUTHENTICATED;
    static constexpr BssMemberStatus ASSOCIATED = Ieee80211AssociationState::ASSOCIATED;

    using PeerCapabilitySnapshot = Ieee80211PeerCapabilityState::Snapshot;
    using PeerLinkSnapshot = Ieee80211PeerLinkState::Snapshot;

  public:
    MacAddress address;
    Mode mode = static_cast<Mode>(-1);
    bool qos = false;

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

  private:
    Ieee80211AssociationState associationState;
    Ieee80211PeerCapabilityState peerCapabilityState;
    Ieee80211PeerLinkState peerLinkState;
    std::vector<IIeee80211PeerAssociationListener *> peerAssociationListeners;

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
    virtual std::string getPeerCapabilitySummary() const;
    virtual std::string getPeerLinkSummary() const;

  public:
    static const char *getModeStr(Ieee80211Mib::Mode mode);
    static const char *getStationTypeStr(Ieee80211Mib::BssStationType stationType);
    std::string getSsidStr() const;
    short allocateAssociationId(const MacAddress& address);
    void releaseAssociationId(const MacAddress& address);
    std::string getSsid() const { return getLocalAssociationSnapshot().getSsid(); }
    MacAddress getBssid() const { return getLocalAssociationSnapshot().getBssid(); }
    BssStationType getStationType() const { return getLocalAssociationSnapshot().getStationType(); }
    bool isAssociated() const { return getLocalAssociationSnapshot().isAssociated(); }
    short getLocalAssociationId() const { return getLocalAssociationSnapshot().getAssociationId(); }
    LocalAssociationSnapshot getLocalAssociationSnapshot() const;
    PeerAssociationSnapshot getPeerAssociationSnapshot(const MacAddress& address) const;
    std::vector<PeerAssociationSnapshot> getPeerAssociationSnapshots() const;
    std::string getPeerAssociationSummary() const;
    bool hasPeerMemberStatus(const MacAddress& address) const { return getPeerAssociationSnapshot(address).hasMemberStatus(); }
    bool isPeerAssociated(const MacAddress& address) const;
    bool isPeerNotAuthenticated(const MacAddress& address) const;
    void setBssStationType(BssStationType stationType);
    void setBssIdentity(const std::string& ssid, const MacAddress& bssid);
    void installLocalAssociation(const std::string& ssid, const MacAddress& bssid, short associationId);
    void clearLocalAssociation();
    void setPeerMemberStatus(const MacAddress& address, BssMemberStatus memberStatus);
    PeerAssociationSnapshot commitPeerAssociation(const MacAddress& address);
    PeerAssociationSnapshot clearPeerAssociation(const MacAddress& address, BssMemberStatus memberStatus);
    void addPeerAssociationListener(IIeee80211PeerAssociationListener *listener);
    void removePeerAssociationListener(IIeee80211PeerAssociationListener *listener);
    void setStationTransmitPower(const MacAddress& address, double transmitPowerDbm);
    void updateStationReceivedPower(const MacAddress& address, units::values::W receivedPower);
    std::optional<PeerLinkSnapshot> getPeerLinkSnapshot(const MacAddress& address) const;
    std::vector<PeerLinkSnapshot> getPeerLinkSnapshots() const;
    PeerCapabilitySnapshot getPeerCapabilitySnapshot(const MacAddress& address) const;
    std::vector<PeerCapabilitySnapshot> getPeerCapabilitySnapshots() const;
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
    std::optional<std::vector<Ieee80211LegacyRate>> getPeerLegacyRates(const MacAddress& address) const;
    std::vector<Ieee80211LegacyRate> getBssBasicLegacyRates() const;
    void installCurrentBssBasicLegacyRates(const Ieee80211SupportedRatesElement& supportedRates,
            const Ieee80211ExtendedSupportedRatesElement& extendedSupportedRates);
    void clearCurrentBssBasicLegacyRates() { currentBssBasicRates.clear(); }
    std::optional<Ieee80211NegotiatedHeCapabilities> getNegotiatedHeCapabilities(const MacAddress& address) const;
    bool isHeModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    void setPeerEhtCapabilities(const MacAddress& address, const Ieee80211EhtCapabilities& capabilities,
            const Ieee80211EhtOperation& operation);
    void removePeerEhtCapabilities(const MacAddress& address);
    std::optional<Ieee80211NegotiatedEhtCapabilities> getNegotiatedEhtCapabilities(const MacAddress& address) const;
    bool isEhtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    bool isHtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    bool isVhtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    bool isLdpcAllowedForPeer(const physicallayer::IIeee80211Mode *mode, const MacAddress& address) const;
    void setPeerVhtCapabilities(const MacAddress& address, const Ieee80211VhtCapabilities& capabilities,
            const Ieee80211VhtOperation& operation);
    void removePeerVhtCapabilities(const MacAddress& address);
    std::optional<Ieee80211NegotiatedVhtCapabilities> getNegotiatedVhtCapabilities(const MacAddress& address) const;
    uint64_t getVhtAssociationGeneration(const MacAddress& address) const;
    void setPeerHtCapabilities(const MacAddress& address, const Ieee80211HtCapabilities& capabilities,
            const Ieee80211HtOperation& operation);
    void removePeerHtCapabilities(const MacAddress& address);
    std::optional<Ieee80211NegotiatedHtCapabilities> getNegotiatedHtCapabilities(const MacAddress& address) const;
    uint64_t getHtAssociationGeneration(const MacAddress& address) const;
};

} // namespace ieee80211

} // namespace inet

#endif
