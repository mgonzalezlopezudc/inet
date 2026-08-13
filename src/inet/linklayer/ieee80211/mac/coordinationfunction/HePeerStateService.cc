//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePeerStateService.h"

#include <limits>
#include <sstream>

#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace ieee80211 {

void HePeerStateService::advanceGeneration(uint64_t& generation)
{
    if (generation == std::numeric_limits<uint64_t>::max())
        throw cRuntimeError("HE peer-state generation exhausted");
    generation++;
    if (generation == 0)
        generation++;
}

HePeerStateService::~HePeerStateService()
{
    stop();
}

void HePeerStateService::configure(cComponent *eventEmitter, Ieee80211Mac *mac,
        const Ports& ports, simtime_t csiValidityDuration,
        double defaultCsiLeakage, const std::string& csiLeakageOverrides)
{
    if (eventEmitter == nullptr || mac == nullptr || !ports.retireAssociation ||
            !ports.ensureAssociation || !ports.finalizeRetiredAssociations ||
            !ports.releaseDeferredRetirements || !ports.invalidateBaseHcf ||
            !ports.invalidateDlScheduler || !ports.invalidateUlCoordinator)
        throw cRuntimeError("HE peer-state service configuration is incomplete");
    this->eventEmitter = eventEmitter;
    this->mac = mac;
    this->ports = ports;
    csiManager.configure(csiValidityDuration, defaultCsiLeakage, csiLeakageOverrides);
}

void HePeerStateService::start()
{
    if (associationListenerRegistered)
        return;
    mib = mac == nullptr ? nullptr : mac->getMib();
    if (mib == nullptr)
        throw cRuntimeError("Cannot start HE peer-state service without an IEEE 802.11 MIB");
    mib->addPeerAssociationListener(this);
    mib->subscribe(PRE_MODEL_CHANGE, this);
    associationListenerRegistered = true;
}

void HePeerStateService::stop()
{
    // PRE_MODEL_CHANGE clears mib before an abnormal parent teardown can leave
    // the MAC's cached MIB pointer null or stale. Normal finish/destruction
    // still unregisters explicitly while the MIB is alive.
    if (associationListenerRegistered && mib != nullptr) {
        mib->removePeerAssociationListener(this);
        if (mib->isSubscribed(PRE_MODEL_CHANGE, this))
            mib->unsubscribe(PRE_MODEL_CHANGE, this);
    }
    associationListenerRegistered = false;
    mib = nullptr;
}

void HePeerStateService::receiveSignal(cComponent *source, simsignal_t signal,
        cObject *value, cObject *details)
{
    if (source == mib && signal == PRE_MODEL_CHANGE &&
            dynamic_cast<cPreModuleDeleteNotification *>(value) != nullptr) {
        associationListenerRegistered = false;
        mib = nullptr;
    }
}

void HePeerStateService::peerAssociationChanged(
        const Ieee80211AssociationState::PeerTransition& transition)
{
    const auto& oldSnapshot = transition.getOldSnapshot();
    const auto& newSnapshot = transition.getNewSnapshot();
    const auto& peer = newSnapshot.getAddress();
    if (oldSnapshot.getAssociationEpoch() != 0)
        runInvalidationCascade(peer, InvalidationReason::ASSOCIATION_CHANGED,
                oldSnapshot.getAssociationEpoch());
    if (newSnapshot.hasMemberStatus() &&
            newSnapshot.getMemberStatus() == Ieee80211Mib::ASSOCIATED)
        ports.ensureAssociation(peer, newSnapshot.getAssociationEpoch());
    ports.finalizeRetiredAssociations();
}

void HePeerStateService::handleTwtBoundary()
{
    if (mib == nullptr)
        return;
    for (const auto& peer : mib->getPeerAssociationSnapshots())
        if (peer.hasMemberStatus() && peer.getMemberStatus() == Ieee80211Mib::ASSOCIATED)
            runInvalidationCascade(peer.getAddress(), InvalidationReason::TWT_BOUNDARY, 0);
}

void HePeerStateService::invalidatePeer(const MacAddress& peer,
        InvalidationReason reason)
{
    auto associationEpoch = mib == nullptr ? 0 :
            mib->getPeerAssociationSnapshot(peer).getAssociationEpoch();
    runInvalidationCascade(peer, reason,
            reason == InvalidationReason::ASSOCIATION_CHANGED ?
                    associationEpoch : 0);
    ports.finalizeRetiredAssociations();
}

void HePeerStateService::runInvalidationCascade(const MacAddress& peer,
        InvalidationReason reason, uint64_t retiredAssociationEpoch)
{
    HePeerInvalidatedEvent event;
    event.peerAddress = peer;
    event.reason = reason;
    event.retiredAssociationEpoch = retiredAssociationEpoch;
    if (reason == InvalidationReason::ASSOCIATION_CHANGED) {
        operatingModes.erase(peer);
        operatingModeGenerations.erase(peer);
        csiManager.invalidatePeer(peer);
        event.steps.push_back(HePeerInvalidationStep::CLEAR_DERIVED_STATE);
        if (retiredAssociationEpoch != 0) {
            ports.retireAssociation(peer, retiredAssociationEpoch);
            event.steps.push_back(HePeerInvalidationStep::RETIRE_ASSOCIATION);
        }
    }
    ports.invalidateBaseHcf(peer);
    event.steps.push_back(HePeerInvalidationStep::INVALIDATE_BASE_HCF);
    ports.invalidateDlScheduler(peer);
    event.steps.push_back(HePeerInvalidationStep::INVALIDATE_DL_SCHEDULER);
    ports.invalidateUlCoordinator(peer);
    event.steps.push_back(HePeerInvalidationStep::INVALIDATE_UL_COORDINATOR);
    eventEmitter->emit(cComponent::registerSignal("hePeerInvalidated"), &event);
}

HcfPeerSnapshot HePeerStateService::getPeerSnapshot(const MacAddress& peer) const
{
    if (mib == nullptr)
        return {};
    auto association = mib->getPeerAssociationSnapshot(peer);
    auto capabilities = mib->getPeerCapabilitySnapshot(peer);
    std::optional<Ieee80211HeOperatingMode> operatingMode;
    auto operatingModeIt = operatingModes.find(peer);
    if (operatingModeIt != operatingModes.end())
        operatingMode = operatingModeIt->second;
    auto getGeneration = [&] (const auto& generations) {
        auto it = generations.find(peer);
        return it == generations.end() ? uint64_t(0) : it->second;
    };
    auto associationId = association.hasAssociationId() && association.getAssociationId() > 0 ?
            static_cast<uint16_t>(association.getAssociationId()) : 0;
    return HcfPeerSnapshot(peer, association.getAssociationEpoch(), associationId,
            capabilities.getNegotiatedHt(), capabilities.getNegotiatedVht(),
            capabilities.getNegotiatedHe(), mac->isTwtPeerEligible(peer), operatingMode,
            capabilities.getGeneration(), getGeneration(operatingModeGenerations),
            csiManager.getPeerGeneration(peer));
}

std::vector<HcfPeerSnapshot> HePeerStateService::getAssociatedPeerSnapshots() const
{
    std::vector<HcfPeerSnapshot> result;
    if (mib == nullptr)
        return result;
    for (const auto& peer : mib->getPeerAssociationSnapshots())
        if (peer.hasMemberStatus() && peer.getMemberStatus() == Ieee80211Mib::ASSOCIATED)
            result.push_back(getPeerSnapshot(peer.getAddress()));
    return result;
}

MacAddress HePeerStateService::getBssid() const
{
    return mib == nullptr ? MacAddress() : mib->getLocalAssociationSnapshot().getBssid();
}

uint16_t HePeerStateService::getAssociationId(const MacAddress& peer) const
{
    if (mib == nullptr)
        return 0;
    auto associationId = mib->getAssociationId(peer);
    return associationId > 0 ? associationId : 0;
}

void HePeerStateService::updateOperatingMode(const MacAddress& peer,
        const Ieee80211HeOperatingMode& mode)
{
    auto existing = operatingModes.find(peer);
    if (existing != operatingModes.end() &&
            existing->second.channelWidth == mode.channelWidth &&
            existing->second.rxNss == mode.rxNss &&
            existing->second.ulMuDisable == mode.ulMuDisable)
        return;

    HePeerOperatingModeChangedEvent event;
    event.peerAddress = peer;
    event.associationId = getAssociationId(peer);
    event.hadOldMode = existing != operatingModes.end();
    if (event.hadOldMode)
        event.oldMode = existing->second;
    event.newMode = mode;
    operatingModes[peer] = mode;
    advanceGeneration(operatingModeGenerations[peer]);
    eventEmitter->emit(cComponent::registerSignal("peerOperatingModeChanged"), &event);
    eventEmitter->emit(cComponent::registerSignal("peerOperatingModeAssociationId"),
            static_cast<unsigned long>(event.associationId));
    eventEmitter->emit(cComponent::registerSignal("peerOperatingModeRxNss"),
            static_cast<unsigned long>(event.newMode.rxNss));
    eventEmitter->emit(cComponent::registerSignal("peerOperatingModeChannelWidth"),
            static_cast<unsigned long>(event.newMode.channelWidth));
    eventEmitter->emit(cComponent::registerSignal("peerOperatingModeUlMuDisable"),
            event.newMode.ulMuDisable ? 1L : 0L);
}

bool HePeerStateService::getOperatingMode(const MacAddress& peer,
        Ieee80211HeOperatingMode& mode) const
{
    auto it = operatingModes.find(peer);
    if (it == operatingModes.end())
        return false;
    mode = it->second;
    return true;
}

std::string HePeerStateService::getCsiTableSummary() const
{
    std::stringstream stream;
    stream << "entries=" << csiManager.getEntryCount()
           << ", valid=" << csiManager.getFreshEntryCount();
    return stream.str();
}

} // namespace ieee80211
} // namespace inet
