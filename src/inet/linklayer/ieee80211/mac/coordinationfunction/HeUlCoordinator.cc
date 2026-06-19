//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

Define_Module(HeUlCoordinator);

void HeUlCoordinator::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        enabled = par("enabled");
        reportMaxAge = par("reportMaxAge");
        ocwMin = par("ocwMin");
        ocwMax = par("ocwMax");
        ofdmaContentionWindow = ocwMin;
        ofdmaBackoff = intuniform(0, ofdmaContentionWindow);
        scheduler = check_and_cast<IIeee80211HeUlScheduler *>(getParentModule()->getSubmodule("ulScheduler"));
        triggerPolicy = check_and_cast<IIeee80211HeUlTriggerPolicy *>(getParentModule()->getSubmodule("ulTriggerPolicy"));
        basicTriggerSentSignal = registerSignal("heUlBasicTriggerSent");
        bsrpTriggerSentSignal = registerSignal("heUlBsrpTriggerSent");
        bufferStatusUpdatedSignal = registerSignal("heUlBufferStatusUpdated");
        scheduledUsersSignal = registerSignal("heUlScheduledUsers");
        randomAccessRusSignal = registerSignal("heUlRandomAccessRus");
        randomAccessAttemptSignal = registerSignal("heUlRandomAccessAttempt");
        randomAccessSuccessSignal = registerSignal("heUlRandomAccessSuccess");
    }
}

void HeUlCoordinator::updateBufferStatus(uint16_t aid, AccessCategory ac, uint8_t tid,
        int64_t backlogBytes, bool retryPending)
{
    auto& status = bufferStatusByAid[aid];
    status.backlogBytes[ac] = std::max<int64_t>(0, backlogBytes);
    status.tid[ac] = tid;
    status.retryPending = retryPending;
    status.updateTime = simTime();
    emit(bufferStatusUpdatedSignal, (long)aid);
}

void HeUlCoordinator::clearStation(uint16_t aid)
{
    bufferStatusByAid.erase(aid);
}

IIeee80211HeUlTriggerPolicy::TriggerType HeUlCoordinator::selectTrigger(const Ieee80211Mib *mib) const
{
    IIeee80211HeUlTriggerPolicy::Context context;
    for (const auto& station : mib->bssAccessPointData.stations) {
        if (station.second != Ieee80211Mib::ASSOCIATED)
            continue;
        context.associatedStations++;
        auto aid = mib->getAssociationId(station.first);
        auto status = bufferStatusByAid.find(aid);
        if (status == bufferStatusByAid.end() || simTime() - status->second.updateTime > reportMaxAge)
            continue;
        context.freshReports++;
        if (status->second.retryPending)
            context.retryStations++;
        for (auto bytes : status->second.backlogBytes)
            if (bytes > 0) {
                context.backloggedStations++;
                break;
            }
    }
    context.elapsedSinceLastTrigger = hasSentTrigger ? simTime() - lastTriggerTime : SIMTIME_MAX;
    return triggerPolicy->selectTrigger(context);
}

AccessCategory HeUlCoordinator::getPreferredAccessCategory() const
{
    AccessCategory selected = AC_BE;
    for (const auto& entry : bufferStatusByAid)
        for (int ac = AC_VO; ac >= AC_BK; ac--)
            if (entry.second.backlogBytes[ac] > 0) {
                selected = static_cast<AccessCategory>(std::max((int)selected, ac));
                break;
            }
    return selected;
}

IIeee80211HeUlScheduler::Schedule HeUlCoordinator::createSchedule(const Ieee80211Mib *mib,
        Hz centerFrequency, Hz bandwidth, simtime_t txopLimit,
        double sensitivityDbm, double targetRssiMarginDb,
        int estimatedRaContenders, double collisionRate, double idleRate)
{
    IIeee80211HeUlScheduler::ScheduleContext context;
    context.channelCenterFrequency = centerFrequency;
    context.channelBandwidth = bandwidth;
    context.txopLimit = txopLimit;
    context.apSensitivityDbm = sensitivityDbm;
    context.targetRssiMarginDb = targetRssiMarginDb;
    context.estimatedRandomAccessContenders = estimatedRaContenders;
    context.recentRandomAccessCollisionRate = collisionRate;
    context.recentRandomAccessIdleRate = idleRate;
    for (const auto& station : mib->bssAccessPointData.stations) {
        if (station.second != Ieee80211Mib::ASSOCIATED)
            continue;
        auto aid = mib->getAssociationId(station.first);
        auto status = bufferStatusByAid.find(aid);
        if (status == bufferStatusByAid.end() || simTime() - status->second.updateTime > reportMaxAge)
            continue;
        IIeee80211HeUlScheduler::CandidateInfo candidate;
        candidate.staAddress = station.first;
        candidate.associationId = aid;
        candidate.backlogBytes = status->second.backlogBytes;
        candidate.retryPending = status->second.retryPending;
        candidate.reportAge = simTime() - status->second.updateTime;
        candidate.hasFreshReport = true;
        candidate.lastService = status->second.lastService;
        for (int ac = AC_VO; ac >= AC_BK; ac--)
            if (candidate.backlogBytes[ac] > 0) {
                candidate.selectedAccessCategory = static_cast<AccessCategory>(ac);
                candidate.selectedTid = status->second.tid[ac];
                break;
            }
        if (auto link = mib->findStationLink(station.first)) {
            candidate.pathLossDb = link->pathLossDb;
            candidate.hasFreshPathLoss = link->valid;
        }
        context.candidates.push_back(candidate);
    }
    if (!context.candidates.empty()) {
        auto anchor = std::min_element(context.candidates.begin(), context.candidates.end(),
                [] (const auto& left, const auto& right) { return left.lastService < right.lastService; });
        anchor->anchor = true;
    }
    auto schedule = scheduler->schedule(context);
    schedule.packetExtensionDurationUs = mib->heOperation.defaultPeDurationUs;
    bool ldpcSupportedByAll = mib->localHeCapabilities.ldpc;
    for (const auto& allocation : schedule.allocations) {
        if (allocation.randomAccess)
            continue;
        auto negotiated = mib->findNegotiatedHeCapabilities(allocation.staAddress);
        ldpcSupportedByAll = ldpcSupportedByAll && negotiated != nullptr &&
                negotiated->valid && negotiated->intersection.ldpc;
    }
    schedule.coding = ldpcSupportedByAll ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
    long scheduledUsers = 0;
    long randomAccessRus = 0;
    for (const auto& allocation : schedule.allocations)
        if (allocation.randomAccess)
            randomAccessRus++;
        else
            scheduledUsers++;
    emit(scheduledUsersSignal, scheduledUsers);
    emit(randomAccessRusSignal, randomAccessRus);
    for (const auto& allocation : schedule.allocations)
        if (!allocation.randomAccess)
            bufferStatusByAid[allocation.associationId].lastService = simTime();
    return schedule;
}

uint32_t HeUlCoordinator::allocateTriggerId()
{
    if (nextTriggerId == 0)
        nextTriggerId = 1;
    return nextTriggerId++;
}

void HeUlCoordinator::noteTriggerSent(IIeee80211HeUlTriggerPolicy::TriggerType triggerType)
{
    lastTriggerTime = simTime();
    hasSentTrigger = true;
    if (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER)
        emit(bsrpTriggerSentSignal, 1L);
    else if (triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER)
        emit(basicTriggerSentSignal, 1L);
}

int HeUlCoordinator::selectRandomAccessRu(int randomAccessRuCount)
{
    if (randomAccessRuCount <= 0)
        return -1;
    if (ofdmaBackoff >= randomAccessRuCount) {
        ofdmaBackoff -= randomAccessRuCount;
        return -1;
    }
    emit(randomAccessAttemptSignal, 1L);
    return intuniform(0, randomAccessRuCount - 1);
}

void HeUlCoordinator::reportRandomAccessResult(bool success)
{
    if (success)
        ofdmaContentionWindow = ocwMin;
    else
        ofdmaContentionWindow = std::min(ocwMax, 2 * ofdmaContentionWindow + 1);
    ofdmaBackoff = intuniform(0, ofdmaContentionWindow);
    if (success)
        emit(randomAccessSuccessSignal, 1L);
}

} // namespace ieee80211
} // namespace inet
