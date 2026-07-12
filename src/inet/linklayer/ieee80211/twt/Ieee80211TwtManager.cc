//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/twt/Ieee80211TwtManager.h"

#include <algorithm>

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace ieee80211 {

Define_Module(Ieee80211TwtManager);

Ieee80211TwtManager::~Ieee80211TwtManager()
{
    cancelAndDelete(servicePeriodTimer);
}

void Ieee80211TwtManager::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        enabled = par("enabled");
        maxIndividualAgreementsPerPeer = par("maxIndividualAgreementsPerPeer");
        servicePeriodTimer = new cMessage("twtServicePeriodTimer");
        lastRadioStateChange = simTime();
        WATCH(enabled);
        WATCH(stationAwake);
        WATCH(awakeTime);
        WATCH(sleepTime);
        WATCH(agreements);
        WATCH(broadcastSchedules);

        // Assert that configuration parameters are valid
        ASSERT(maxIndividualAgreementsPerPeer > 0);

        EV_INFO << "Initializing TWT Manager (Local Stage). Enabled: " << enabled 
                << ", Max agreements per peer: " << maxIndividualAgreementsPerPeer << "\n";
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        mac = getModuleFromPar<Ieee80211Mac>(par("macModule"), this);
        mib = getModuleFromPar<Ieee80211Mib>(par("mibModule"), this);

        // Assert that dependent modules are successfully resolved
        ASSERT(mac != nullptr);
        ASSERT(mib != nullptr);

        // IEEE 802.11ax-2024, Section 26.8.1: Target Wake Time (TWT) requires 
        // support for HE (High Efficiency) / 802.11ax capabilities and QoS STAs.
        if (enabled && (!mib->qos || !mac->isAxMode()))
            throw cRuntimeError("TWT requires an IEEE 802.11ax QoS station");

        EV_INFO << "TWT Manager Link Layer Stage Initialized. Node MAC Address: " 
                << mib->address << ", Station Type: " 
                << (mib->bssStationData.stationType == Ieee80211Mib::ACCESS_POINT ? "AP" : "Non-AP STA") << "\n";
    }
}

void Ieee80211TwtManager::handleMessage(cMessage *message)
{
    // Assert that the message received is indeed the service period timer
    ASSERT(message == servicePeriodTimer);
    if (message != servicePeriodTimer)
        throw cRuntimeError("Unknown TWT message");

    EV_INFO << "TWT service period timer fired at " << simTime() << ". Updating TWT state and rescheduling.\n";
    updateServicePeriodState();
    rescheduleServicePeriodTimer();
}

void Ieee80211TwtManager::finish()
{
    recordScalar("twtAgreementCount", (double)agreements.size());
    recordScalar("twtBroadcastScheduleCount", (double)broadcastSchedules.size());
    if (enabled && mib != nullptr && mib->bssStationData.stationType == Ieee80211Mib::STATION) {
        if (stationAwake)
            awakeTime += simTime() - lastRadioStateChange;
        else
            sleepTime += simTime() - lastRadioStateChange;
    }
    recordScalar("twtAwakeTime", awakeTime);
    recordScalar("twtSleepTime", sleepTime);

    EV_INFO << "TWT Simulation Finished. Total Agreements: " << agreements.size() 
            << ", Total Broadcast Schedules: " << broadcastSchedules.size() 
            << ", Total Awake Time: " << awakeTime << ", Total Sleep Time: " << sleepTime << "\n";
}

bool Ieee80211TwtManager::isAgreementActiveNow(const TwtAgreement& agreement, simtime_t now) const
{
    // Assert parameters are non-negative
    ASSERT(agreement.wakeDuration >= SIMTIME_ZERO);
    ASSERT(agreement.wakeInterval >= SIMTIME_ZERO);

    if (!agreement.active || agreement.wakeDuration <= SIMTIME_ZERO || agreement.nextWakeTime > now)
        return false;

    // IEEE 802.11ax-2024 Section 26.8.2: For implicit TWT agreements, 
    // successive TWT service periods start at periodic intervals of the TWT Wake Interval.
    simtime_t start = agreement.nextWakeTime;
    if (agreement.implicit && agreement.wakeInterval > SIMTIME_ZERO) {
        int64_t intervals = (now - start) / agreement.wakeInterval;
        start += intervals * agreement.wakeInterval;
    }

    // Check if the current time lies within the computed service period (SP) window [start, start + wakeDuration)
    return now >= start && now < start + agreement.wakeDuration;
}

simtime_t Ieee80211TwtManager::getNextEventTime(const TwtAgreement& agreement, simtime_t now) const
{
    // Assert parameters are non-negative
    ASSERT(agreement.wakeDuration >= SIMTIME_ZERO);
    ASSERT(agreement.wakeInterval >= SIMTIME_ZERO);

    if (!agreement.active)
        return SIMTIME_MAX;

    // Determine the start time of the relevant TWT Service Period (SP)
    simtime_t start = agreement.nextWakeTime;
    if (agreement.implicit && agreement.wakeInterval > SIMTIME_ZERO && now > start) {
        int64_t intervals = (now - start) / agreement.wakeInterval;
        start += intervals * agreement.wakeInterval;
        // If we already passed the current SP duration, the next relevant SP is the subsequent interval
        if (now >= start + agreement.wakeDuration)
            start += agreement.wakeInterval;
    }

    // If the next SP has not started yet, return its start time
    if (now < start)
        return start;
    // If we are currently active within the wake duration, return its end time
    if (now < start + agreement.wakeDuration)
        return start + agreement.wakeDuration;

    // Otherwise, return the start of the next periodic wake interval (for implicit TWT)
    return agreement.implicit && agreement.wakeInterval > SIMTIME_ZERO ? start + agreement.wakeInterval : SIMTIME_MAX;
}

TwtAgreement *Ieee80211TwtManager::findAgreement(const MacAddress& peer, uint8_t flowId, bool broadcast, uint8_t broadcastId)
{
    // Assert target peer address is valid/specified
    ASSERT(!peer.isUnspecified());

    for (auto& agreement : agreements)
        if (agreement.peerAddress == peer && agreement.flowId == flowId && agreement.broadcast == broadcast && agreement.broadcastId == broadcastId)
            return &agreement;
    return nullptr;
}

TwtBroadcastSchedule *Ieee80211TwtManager::findBroadcastScheduleForUpdate(uint8_t broadcastId)
{
    // Helper to find and update a broadcast TWT schedule by its broadcast ID
    for (auto& schedule : broadcastSchedules)
        if (schedule.broadcastId == broadcastId)
            return &schedule;
    return nullptr;
}

void Ieee80211TwtManager::expireBroadcastSchedules()
{
    // Expire/cleanup schedules whose expiration time has passed
    auto initialSize = broadcastSchedules.size();
    broadcastSchedules.erase(std::remove_if(broadcastSchedules.begin(), broadcastSchedules.end(), [this] (const auto& schedule) {
        bool expired = schedule.expiresAt != SIMTIME_MAX && schedule.expiresAt <= simTime();
        if (expired) {
            EV_INFO << "Broadcast TWT schedule ID " << (int)schedule.broadcastId 
                    << " expired at " << schedule.expiresAt << " (Current time: " << simTime() << ")\n";
        }
        return expired;
    }), broadcastSchedules.end());

    if (broadcastSchedules.size() != initialSize) {
        EV_INFO << "Expired " << (initialSize - broadcastSchedules.size()) << " TWT broadcast schedules.\n";
    }
}

void Ieee80211TwtManager::installAgreement(const TwtAgreement& agreement)
{
    Enter_Method("installAgreement");

    // Assert target peer address and duration/interval are valid
    ASSERT(!agreement.peerAddress.isUnspecified());
    ASSERT(agreement.wakeDuration >= SIMTIME_ZERO);
    ASSERT(agreement.wakeInterval >= SIMTIME_ZERO);

    if (!enabled)
        throw cRuntimeError("Cannot install a TWT agreement while TWT is disabled");

    if (!agreement.broadcast) {
        // IEEE 802.11ax-2024 Section 26.8.1.1: TWT Flow Identifier is 3 bits, 
        // meaning maximum active individual TWT agreements between a pair of STAs cannot exceed 8.
        ASSERT(agreement.flowId < 8);

        int count = 0;
        for (const auto& existing : agreements)
            if (!existing.broadcast && existing.peerAddress == agreement.peerAddress)
                count++;
        if (findAgreement(agreement.peerAddress, agreement.flowId, false, 0) == nullptr && count >= maxIndividualAgreementsPerPeer)
            throw cRuntimeError("Too many individual TWT agreements with %s", agreement.peerAddress.str().c_str());
    }

    EV_INFO << "Installing TWT Agreement. Peer: " << agreement.peerAddress
            << ", Type: " << (agreement.broadcast ? "Broadcast" : "Individual")
            << ", FlowId/BroadcastId: " << (agreement.broadcast ? (int)agreement.broadcastId : (int)agreement.flowId)
            << ", wakeInterval: " << agreement.wakeInterval
            << ", wakeDuration: " << agreement.wakeDuration
            << ", nextWakeTime: " << agreement.nextWakeTime 
            << ", implicit: " << agreement.implicit 
            << ", announced: " << agreement.announced << "\n";

    auto *existing = findAgreement(agreement.peerAddress, agreement.flowId, agreement.broadcast, agreement.broadcastId);
    if (existing) {
        EV_INFO << "Updating existing TWT agreement.\n";
        *existing = agreement;
    }
    else {
        EV_INFO << "Creating new TWT agreement entry.\n";
        agreements.push_back(agreement);
    }
    updateServicePeriodState();
    rescheduleServicePeriodTimer();
}

void Ieee80211TwtManager::removeAgreement(const MacAddress& peer, uint8_t flowId, bool broadcast, uint8_t broadcastId)
{
    Enter_Method("removeAgreement");

    // Assert target peer address is valid
    ASSERT(!peer.isUnspecified());

    EV_INFO << "Removing TWT Agreement. Peer: " << peer
            << ", Type: " << (broadcast ? "Broadcast" : "Individual")
            << ", FlowId/BroadcastId: " << (broadcast ? (int)broadcastId : (int)flowId) << "\n";

    auto initialSize = agreements.size();
    agreements.erase(std::remove_if(agreements.begin(), agreements.end(), [&] (const auto& agreement) {
        return agreement.peerAddress == peer && agreement.flowId == flowId && agreement.broadcast == broadcast && agreement.broadcastId == broadcastId;
    }), agreements.end());

    if (agreements.size() < initialSize) {
        EV_INFO << "Successfully removed TWT agreement. New count: " << agreements.size() << "\n";
    } else {
        EV_INFO << "TWT agreement was not found. No entries removed.\n";
    }

    updateServicePeriodState();
    rescheduleServicePeriodTimer();
}

bool Ieee80211TwtManager::updateNextWakeTime(const MacAddress& peer, uint8_t flowId, simtime_t nextWakeTime)
{
    Enter_Method("updateNextWakeTime");

    // Assert valid parameters
    ASSERT(!peer.isUnspecified());
    ASSERT(nextWakeTime >= SIMTIME_ZERO);

    auto *agreement = findAgreement(peer, flowId, false, 0);
    if (agreement == nullptr) {
        EV_INFO << "Cannot update next wake time: TWT agreement not found for peer " << peer << ", flowId " << (int)flowId << "\n";
        return false;
    }

    EV_INFO << "Updating TWT agreement next wake time for peer " << peer 
            << ", flowId " << (int)flowId 
            << " from " << agreement->nextWakeTime << " to " << nextWakeTime << "\n";

    agreement->nextWakeTime = nextWakeTime;
    agreement->active = true;
    updateServicePeriodState();
    rescheduleServicePeriodTimer();
    return true;
}

bool Ieee80211TwtManager::isPeerEligible(const MacAddress& peer) const
{
    // Assert target peer address is valid
    ASSERT(!peer.isUnspecified());

    if (!enabled || mib->bssStationData.stationType != Ieee80211Mib::ACCESS_POINT)
        return true;

    // IEEE 802.11ax-2024 Section 26.8.2: 
    // - For an announced TWT (Flow Type = 0), the AP (TWT responding STA) shall not transmit 
    //   frames to the non-AP STA (TWT requesting STA) within a TWT SP until it has received 
    //   a PS-Poll frame or a U-APSD trigger frame from it.
    // - For an unannounced TWT (Flow Type = 1), the AP may transmit frames to the non-AP STA 
    //   at the start of a TWT SP since the STA is assumed to be awake.
    // - If no agreement exists, transmission is unrestricted by TWT.
    bool hasAgreement = false;
    for (const auto& agreement : agreements) {
        if (agreement.peerAddress != peer)
            continue;
        hasAgreement = true;
        if (isAgreementActiveNow(agreement, simTime()) && (!agreement.announced || agreement.peerAwakeAnnounced)) {
            EV_DEBUG << "Peer " << peer << " is eligible for transmission (individual TWT active, announced=" 
                     << agreement.announced << ", awakeAnnounced=" << agreement.peerAwakeAnnounced << ")\n";
            return true;
        }
    }
    for (const auto& schedule : broadcastSchedules) {
        if (schedule.members.count(peer) == 0)
            continue;
        hasAgreement = true;
        if (isAgreementActiveNow(schedule, simTime()) && (!schedule.announced || schedule.peerAwakeAnnounced)) {
            EV_DEBUG << "Peer " << peer << " is eligible for transmission (broadcast TWT active, announced=" 
                     << schedule.announced << ", awakeAnnounced=" << schedule.peerAwakeAnnounced << ")\n";
            return true;
        }
    }

    if (hasAgreement) {
        EV_DEBUG << "Peer " << peer << " is NOT eligible for transmission (TWT active but announced/awake condition not met)\n";
    }
    return !hasAgreement;
}

void Ieee80211TwtManager::notifyPeerAwake(const MacAddress& peer)
{
    Enter_Method("notifyPeerAwake");

    // Assert peer address is valid
    ASSERT(!peer.isUnspecified());

    EV_INFO << "Peer " << peer << " notified awake at " << simTime() << ". Updating active TWT agreements.\n";

    bool foundActive = false;
    for (auto& agreement : agreements) {
        if (agreement.peerAddress == peer && isAgreementActiveNow(agreement, simTime())) {
            EV_INFO << "Marking individual TWT flow " << (int)agreement.flowId << " as awake/announced for peer " << peer << "\n";
            agreement.peerAwakeAnnounced = true;
            foundActive = true;
        }
    }
    for (auto& schedule : broadcastSchedules) {
        if (schedule.members.count(peer) != 0 && isAgreementActiveNow(schedule, simTime())) {
            EV_INFO << "Marking broadcast TWT schedule " << (int)schedule.broadcastId << " as awake/announced for member " << peer << "\n";
            schedule.peerAwakeAnnounced = true;
            foundActive = true;
        }
    }

    if (!foundActive) {
        EV_INFO << "No active TWT service period found for peer " << peer << " at this time.\n";
    }
}

void Ieee80211TwtManager::installBroadcastSchedule(const TwtBroadcastSchedule& schedule)
{
    Enter_Method("installBroadcastSchedule");

    // Assert validity of schedule parameters
    ASSERT(schedule.broadcast);
    ASSERT(schedule.wakeInterval > SIMTIME_ZERO);
    ASSERT(schedule.wakeDuration > SIMTIME_ZERO);

    if (!enabled)
        throw cRuntimeError("Cannot install a broadcast TWT schedule while TWT is disabled");
    if (!schedule.active || schedule.wakeInterval <= SIMTIME_ZERO || schedule.wakeDuration <= SIMTIME_ZERO)
        throw cRuntimeError("Invalid broadcast TWT schedule");

    EV_INFO << "Installing Broadcast TWT Schedule. BroadcastId: " << (int)schedule.broadcastId
            << ", wakeInterval: " << schedule.wakeInterval
            << ", wakeDuration: " << schedule.wakeDuration
            << ", nextWakeTime: " << schedule.nextWakeTime
            << ", expiresAt: " << schedule.expiresAt << "\n";

    auto *existing = findBroadcastScheduleForUpdate(schedule.broadcastId);
    if (existing) {
        EV_INFO << "Updating existing broadcast TWT schedule. Merging current member list (size: " 
                << existing->members.size() << ").\n";
        auto members = existing->members;
        *existing = schedule;
        existing->members.insert(members.begin(), members.end());
    }
    else {
        EV_INFO << "Creating new broadcast TWT schedule entry.\n";
        broadcastSchedules.push_back(schedule);
    }
    rescheduleServicePeriodTimer();
}

void Ieee80211TwtManager::removeBroadcastSchedule(uint8_t broadcastId)
{
    Enter_Method("removeBroadcastSchedule");

    EV_INFO << "Removing broadcast TWT schedule. BroadcastId: " << (int)broadcastId << "\n";

    auto initialSize = broadcastSchedules.size();
    broadcastSchedules.erase(std::remove_if(broadcastSchedules.begin(), broadcastSchedules.end(), [=] (const auto& schedule) {
        return schedule.broadcastId == broadcastId;
    }), broadcastSchedules.end());

    if (broadcastSchedules.size() < initialSize) {
        EV_INFO << "Successfully removed broadcast TWT schedule. New count: " << broadcastSchedules.size() << "\n";
    } else {
        EV_INFO << "Broadcast TWT schedule was not found. No schedules removed.\n";
    }

    rescheduleServicePeriodTimer();
}

bool Ieee80211TwtManager::findBroadcastSchedule(uint8_t broadcastId, TwtBroadcastSchedule& schedule) const
{
    // Helper to search for a broadcast schedule by its ID
    for (const auto& candidate : broadcastSchedules)
        if (candidate.broadcastId == broadcastId) {
            schedule = candidate;
            return true;
        }
    return false;
}

std::vector<TwtBroadcastSchedule> Ieee80211TwtManager::getBroadcastSchedules() const
{
    return broadcastSchedules;
}

bool Ieee80211TwtManager::addBroadcastMember(uint8_t broadcastId, const MacAddress& peer)
{
    Enter_Method("addBroadcastMember");

    // Assert target peer address is valid
    ASSERT(!peer.isUnspecified());

    auto *schedule = findBroadcastScheduleForUpdate(broadcastId);
    if (schedule == nullptr) {
        EV_INFO << "Cannot add member to broadcast schedule: ID " << (int)broadcastId << " not found.\n";
        return false;
    }

    EV_INFO << "Adding peer " << peer << " to broadcast TWT schedule ID " << (int)broadcastId << "\n";
    schedule->members.insert(peer);
    return true;
}

void Ieee80211TwtManager::removeBroadcastMember(uint8_t broadcastId, const MacAddress& peer)
{
    Enter_Method("removeBroadcastMember");

    // Assert target peer address is valid
    ASSERT(!peer.isUnspecified());

    if (auto *schedule = findBroadcastScheduleForUpdate(broadcastId)) {
        EV_INFO << "Removing peer " << peer << " from broadcast TWT schedule ID " << (int)broadcastId << "\n";
        schedule->members.erase(peer);
    } else {
        EV_INFO << "Cannot remove member from broadcast schedule: ID " << (int)broadcastId << " not found.\n";
    }
}

void Ieee80211TwtManager::updateServicePeriodState()
{
    if (!enabled)
        return;

    expireBroadcastSchedules();
    bool awake = agreements.empty() && broadcastSchedules.empty();

    // IEEE 802.11ax-2024 Section 26.8.2.1:
    // - For an unannounced TWT agreement, the STA is assumed to be awake at the start of the SP.
    // - For an announced TWT agreement, the STA must first announce its awake state by sending 
    //   a PS-Poll or a U-APSD trigger frame before the AP can transmit frames to it.
    for (auto& agreement : agreements) {
        if (isAgreementActiveNow(agreement, simTime())) {
            awake = true;
            if (!agreement.announced) {
                agreement.peerAwakeAnnounced = true;
            }
            else if (mib->bssStationData.stationType == Ieee80211Mib::STATION && !agreement.peerAwakeAnnounced) {
                // Announced TWT: non-AP station sends PS-Poll to AP to announce awake status
                EV_INFO << "Announced TWT Service Period started. Station sending TWT PS-Poll to " << agreement.peerAddress << "\n";
                mac->sendTwtPsPoll(agreement.peerAddress);
                agreement.peerAwakeAnnounced = true;
            }
        }
        else {
            agreement.peerAwakeAnnounced = false;
        }
    }

    for (auto& schedule : broadcastSchedules) {
        if (isAgreementActiveNow(schedule, simTime()) &&
                (mib->bssStationData.stationType == Ieee80211Mib::ACCESS_POINT || schedule.members.count(mib->address) != 0)) {
            awake = true;
        }
        else {
            schedule.peerAwakeAnnounced = false;
        }
    }

    if (mib->bssStationData.stationType == Ieee80211Mib::STATION && awake != stationAwake) {
        simtime_t duration = simTime() - lastRadioStateChange;
        if (stationAwake) {
            awakeTime += duration;
            EV_INFO << "Station transitioning from AWAKE to SLEEP. Awake duration: " << duration << "\n";
        }
        else {
            sleepTime += duration;
            EV_INFO << "Station transitioning from SLEEP to AWAKE. Sleep duration: " << duration << "\n";
        }
        lastRadioStateChange = simTime();
        stationAwake = awake;
        mac->setTwtRadioAwake(awake);
    }
    // Re-evaluate queued traffic at every service-period boundary. This is
    // required on the AP too: peer eligibility can change while the AP radio
    // itself remains continuously awake.
    mac->twtServicePeriodChanged();
}

void Ieee80211TwtManager::rescheduleServicePeriodTimer()
{
    if (!enabled)
        return;

    simtime_t next = SIMTIME_MAX;
    for (const auto& agreement : agreements)
        next = std::min(next, getNextEventTime(agreement, simTime()));
    for (const auto& schedule : broadcastSchedules) {
        next = std::min(next, getNextEventTime(schedule, simTime()));
        next = std::min(next, schedule.expiresAt);
    }

    if (servicePeriodTimer->isScheduled())
        cancelEvent(servicePeriodTimer);

    if (next != SIMTIME_MAX) {
        simtime_t scheduledTime = std::max(next, simTime());
        EV_INFO << "Scheduling next TWT service period timer at " << scheduledTime 
                << " (delay: " << (scheduledTime - simTime()) << ")\n";
        scheduleAt(scheduledTime, servicePeriodTimer);
    } else {
        EV_INFO << "No future TWT service periods or schedule expirations. Timer not scheduled.\n";
    }
}

} // namespace ieee80211
} // namespace inet
