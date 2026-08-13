//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEPEERSTATESERVICE_H
#define __INET_HEPEERSTATESERVICE_H

#include <functional>
#include <map>
#include <optional>

#include "inet/linklayer/ieee80211/mac/Ieee80211HeOmi.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeMuMimoCsiManager.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211AssociationState.h"

namespace inet {
namespace ieee80211 {

class Ieee80211Mac;
class Ieee80211Mib;

enum class HePeerInvalidationReason {
    ASSOCIATION_CHANGED,
    TWT_BOUNDARY,
};

enum class HePeerInvalidationStep {
    CLEAR_DERIVED_STATE,
    RETIRE_ASSOCIATION,
    INVALIDATE_BASE_HCF,
    INVALIDATE_DL_SCHEDULER,
    INVALIDATE_UL_COORDINATOR,
};

class INET_API HePeerInvalidatedEvent : public cObject
{
  public:
    MacAddress peerAddress;
    uint64_t retiredAssociationEpoch = 0;
    HePeerInvalidationReason reason = HePeerInvalidationReason::ASSOCIATION_CHANGED;
    std::vector<HePeerInvalidationStep> steps;
};

class INET_API HePeerOperatingModeChangedEvent : public cObject
{
  public:
    MacAddress peerAddress;
    uint16_t associationId = 0;
    bool hadOldMode = false;
    Ieee80211HeOperatingMode oldMode;
    Ieee80211HeOperatingMode newMode;
};

/** Sole owner of HE state derived from a peer association epoch. */
class INET_API HePeerStateService : private IIeee80211PeerAssociationListener, private cListener
{
  public:
    using InvalidationReason = HePeerInvalidationReason;

    struct Ports {
        std::function<void(const MacAddress&, uint64_t)> retireAssociation;
        std::function<void(const MacAddress&, uint64_t)> ensureAssociation;
        std::function<void()> finalizeRetiredAssociations;
        std::function<void()> releaseDeferredRetirements;
        std::function<void(const MacAddress&)> invalidateBaseHcf;
        std::function<void(const MacAddress&)> invalidateDlScheduler;
        std::function<void(const MacAddress&)> invalidateUlCoordinator;
    };

  private:
    cComponent *eventEmitter = nullptr;
    Ieee80211Mac *mac = nullptr;
    Ieee80211Mib *mib = nullptr;
    Ports ports;
    bool associationListenerRegistered = false;
    std::map<MacAddress, Ieee80211HeOperatingMode> operatingModes;
    std::map<MacAddress, uint64_t> operatingModeGenerations;
    HeMuMimoCsiManager csiManager;

  private:
    static void advanceGeneration(uint64_t& generation);
    virtual void peerAssociationChanged(
            const Ieee80211AssociationState::PeerTransition& transition) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signal,
            cObject *value, cObject *details) override;
    void runInvalidationCascade(const MacAddress& peer, InvalidationReason reason,
            uint64_t retiredAssociationEpoch);

  public:
    ~HePeerStateService();

    void configure(cComponent *eventEmitter, Ieee80211Mac *mac,
            const Ports& ports, simtime_t csiValidityDuration,
            double defaultCsiLeakage, const std::string& csiLeakageOverrides);
    void start();
    void stop();
    void handleTwtBoundary();
    void invalidatePeer(const MacAddress& peer, InvalidationReason reason);
    void releaseDeferredRetirements() { ports.releaseDeferredRetirements(); }

    HcfPeerSnapshot getPeerSnapshot(const MacAddress& peer) const;
    std::vector<HcfPeerSnapshot> getAssociatedPeerSnapshots() const;
    MacAddress getBssid() const;
    uint16_t getAssociationId(const MacAddress& peer) const;

    void updateOperatingMode(const MacAddress& peer,
            const Ieee80211HeOperatingMode& mode);
    bool getOperatingMode(const MacAddress& peer,
            Ieee80211HeOperatingMode& mode) const;

    HeMuMimoCsiManager& getCsiManager() { return csiManager; }
    const HeMuMimoCsiManager& getCsiManager() const { return csiManager; }
    std::string getCsiTableSummary() const;
    bool isAssociationListenerRegisteredForTest() const
        { return associationListenerRegistered; }
};

} // namespace ieee80211
} // namespace inet

#endif
