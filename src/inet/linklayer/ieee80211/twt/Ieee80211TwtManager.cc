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
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        mac = getModuleFromPar<Ieee80211Mac>(par("macModule"), this);
        mib = getModuleFromPar<Ieee80211Mib>(par("mibModule"), this);
        if (enabled && (!mib->qos || !mac->isAxMode()))
            throw cRuntimeError("TWT requires an IEEE 802.11ax QoS station");
    }
}

void Ieee80211TwtManager::handleMessage(cMessage *message)
{
    if (message != servicePeriodTimer)
        throw cRuntimeError("Unknown TWT message");
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
}

bool Ieee80211TwtManager::isAgreementActiveNow(const TwtAgreement& agreement, simtime_t now) const
{
    if (!agreement.active || agreement.wakeDuration <= SIMTIME_ZERO || agreement.nextWakeTime > now)
        return false;
    simtime_t start = agreement.nextWakeTime;
    if (agreement.implicit && agreement.wakeInterval > SIMTIME_ZERO) {
        auto intervals = (now - start) / agreement.wakeInterval;
        start += intervals * agreement.wakeInterval;
    }
    return now >= start && now < start + agreement.wakeDuration;
}

simtime_t Ieee80211TwtManager::getNextEventTime(const TwtAgreement& agreement, simtime_t now) const
{
    if (!agreement.active)
        return SIMTIME_MAX;
    simtime_t start = agreement.nextWakeTime;
    if (agreement.implicit && agreement.wakeInterval > SIMTIME_ZERO && now > start) {
        auto intervals = (now - start) / agreement.wakeInterval;
        start += intervals * agreement.wakeInterval;
        if (now >= start + agreement.wakeDuration)
            start += agreement.wakeInterval;
    }
    if (now < start)
        return start;
    if (now < start + agreement.wakeDuration)
        return start + agreement.wakeDuration;
    return agreement.implicit && agreement.wakeInterval > SIMTIME_ZERO ? start + agreement.wakeInterval : SIMTIME_MAX;
}

TwtAgreement *Ieee80211TwtManager::findAgreement(const MacAddress& peer, uint8_t flowId, bool broadcast, uint8_t broadcastId)
{
    for (auto& agreement : agreements)
        if (agreement.peerAddress == peer && agreement.flowId == flowId && agreement.broadcast == broadcast && agreement.broadcastId == broadcastId)
            return &agreement;
    return nullptr;
}

TwtBroadcastSchedule *Ieee80211TwtManager::findBroadcastScheduleForUpdate(uint8_t broadcastId)
{
    for (auto& schedule : broadcastSchedules)
        if (schedule.broadcastId == broadcastId)
            return &schedule;
    return nullptr;
}

void Ieee80211TwtManager::expireBroadcastSchedules()
{
    broadcastSchedules.erase(std::remove_if(broadcastSchedules.begin(), broadcastSchedules.end(), [] (const auto& schedule) {
        return schedule.expiresAt != SIMTIME_MAX && schedule.expiresAt <= simTime();
    }), broadcastSchedules.end());
}

void Ieee80211TwtManager::installAgreement(const TwtAgreement& agreement)
{
    Enter_Method("installAgreement");
    if (!enabled)
        throw cRuntimeError("Cannot install a TWT agreement while TWT is disabled");
    if (!agreement.broadcast) {
        int count = 0;
        for (const auto& existing : agreements)
            if (!existing.broadcast && existing.peerAddress == agreement.peerAddress)
                count++;
        if (findAgreement(agreement.peerAddress, agreement.flowId, false, 0) == nullptr && count >= maxIndividualAgreementsPerPeer)
            throw cRuntimeError("Too many individual TWT agreements with %s", agreement.peerAddress.str().c_str());
    }
    auto *existing = findAgreement(agreement.peerAddress, agreement.flowId, agreement.broadcast, agreement.broadcastId);
    if (existing)
        *existing = agreement;
    else
        agreements.push_back(agreement);
    updateServicePeriodState();
    rescheduleServicePeriodTimer();
}

void Ieee80211TwtManager::removeAgreement(const MacAddress& peer, uint8_t flowId, bool broadcast, uint8_t broadcastId)
{
    Enter_Method("removeAgreement");
    agreements.erase(std::remove_if(agreements.begin(), agreements.end(), [&] (const auto& agreement) {
        return agreement.peerAddress == peer && agreement.flowId == flowId && agreement.broadcast == broadcast && agreement.broadcastId == broadcastId;
    }), agreements.end());
    updateServicePeriodState();
    rescheduleServicePeriodTimer();
}

bool Ieee80211TwtManager::updateNextWakeTime(const MacAddress& peer, uint8_t flowId, simtime_t nextWakeTime)
{
    Enter_Method("updateNextWakeTime");
    auto *agreement = findAgreement(peer, flowId, false, 0);
    if (agreement == nullptr)
        return false;
    agreement->nextWakeTime = nextWakeTime;
    agreement->active = true;
    updateServicePeriodState();
    rescheduleServicePeriodTimer();
    return true;
}

bool Ieee80211TwtManager::isPeerEligible(const MacAddress& peer) const
{
    if (!enabled || mib->bssStationData.stationType != Ieee80211Mib::ACCESS_POINT)
        return true;
    bool hasAgreement = false;
    for (const auto& agreement : agreements) {
        if (agreement.peerAddress != peer)
            continue;
        hasAgreement = true;
        if (isAgreementActiveNow(agreement, simTime()) && (!agreement.announced || agreement.peerAwakeAnnounced))
            return true;
    }
    for (const auto& schedule : broadcastSchedules) {
        if (schedule.members.count(peer) == 0)
            continue;
        hasAgreement = true;
        if (isAgreementActiveNow(schedule, simTime()) && (!schedule.announced || schedule.peerAwakeAnnounced))
            return true;
    }
    return !hasAgreement;
}

void Ieee80211TwtManager::notifyPeerAwake(const MacAddress& peer)
{
    Enter_Method("notifyPeerAwake");
    for (auto& agreement : agreements)
        if (agreement.peerAddress == peer && isAgreementActiveNow(agreement, simTime()))
            agreement.peerAwakeAnnounced = true;
    for (auto& schedule : broadcastSchedules)
        if (schedule.members.count(peer) != 0 && isAgreementActiveNow(schedule, simTime()))
            schedule.peerAwakeAnnounced = true;
}

void Ieee80211TwtManager::installBroadcastSchedule(const TwtBroadcastSchedule& schedule)
{
    Enter_Method("installBroadcastSchedule");
    if (!enabled)
        throw cRuntimeError("Cannot install a broadcast TWT schedule while TWT is disabled");
    if (!schedule.active || schedule.wakeInterval <= SIMTIME_ZERO || schedule.wakeDuration <= SIMTIME_ZERO)
        throw cRuntimeError("Invalid broadcast TWT schedule");
    auto *existing = findBroadcastScheduleForUpdate(schedule.broadcastId);
    if (existing) {
        auto members = existing->members;
        *existing = schedule;
        existing->members.insert(members.begin(), members.end());
    }
    else
        broadcastSchedules.push_back(schedule);
    rescheduleServicePeriodTimer();
}

void Ieee80211TwtManager::removeBroadcastSchedule(uint8_t broadcastId)
{
    Enter_Method("removeBroadcastSchedule");
    broadcastSchedules.erase(std::remove_if(broadcastSchedules.begin(), broadcastSchedules.end(), [=] (const auto& schedule) {
        return schedule.broadcastId == broadcastId;
    }), broadcastSchedules.end());
    rescheduleServicePeriodTimer();
}

bool Ieee80211TwtManager::findBroadcastSchedule(uint8_t broadcastId, TwtBroadcastSchedule& schedule) const
{
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
    auto *schedule = findBroadcastScheduleForUpdate(broadcastId);
    if (schedule == nullptr)
        return false;
    schedule->members.insert(peer);
    return true;
}

void Ieee80211TwtManager::removeBroadcastMember(uint8_t broadcastId, const MacAddress& peer)
{
    Enter_Method("removeBroadcastMember");
    if (auto *schedule = findBroadcastScheduleForUpdate(broadcastId))
        schedule->members.erase(peer);
}

void Ieee80211TwtManager::updateServicePeriodState()
{
    if (!enabled)
        return;
    bool awake = false;
    expireBroadcastSchedules();
    for (auto& agreement : agreements) {
        if (isAgreementActiveNow(agreement, simTime())) {
            awake = true;
            if (!agreement.announced)
                agreement.peerAwakeAnnounced = true;
            else if (mib->bssStationData.stationType == Ieee80211Mib::STATION && !agreement.peerAwakeAnnounced) {
                mac->sendTwtPsPoll(agreement.peerAddress);
                agreement.peerAwakeAnnounced = true;
            }
        }
        else
            agreement.peerAwakeAnnounced = false;
    }
    for (auto& schedule : broadcastSchedules) {
        if (isAgreementActiveNow(schedule, simTime()) &&
                (mib->bssStationData.stationType == Ieee80211Mib::ACCESS_POINT || schedule.members.count(mib->address) != 0))
            awake = true;
        else
            schedule.peerAwakeAnnounced = false;
    }
    if (mib->bssStationData.stationType == Ieee80211Mib::STATION && awake != stationAwake) {
        if (stationAwake)
            awakeTime += simTime() - lastRadioStateChange;
        else
            sleepTime += simTime() - lastRadioStateChange;
        lastRadioStateChange = simTime();
        stationAwake = awake;
        mac->setTwtRadioAwake(awake);
    }
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
    if (next != SIMTIME_MAX)
        scheduleAt(std::max(next, simTime()), servicePeriodTimer);
}

} // namespace ieee80211
} // namespace inet
