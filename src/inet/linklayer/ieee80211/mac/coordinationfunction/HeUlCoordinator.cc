//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"

#include <algorithm>
#include <set>
#include <sstream>

// HE UL coordinator.
//
// Implements the AP-side state-keeping and scheduling support for UL OFDMA:
//   - Buffer Status Report caching (IEEE 802.11-2024 Clause 26.5.2).
//   - Trigger-type selection (Basic / BSRP) via a pluggable policy.
//   - Uplink OFDMA Random Access (UORA) state machine (Clause 26.5.4).
//
// The UORA model keeps per-AC OFDMA contention window (OCW) and backoff (OBO)
// state.  The OCW update rule (reset on success, double on failure) follows
// Clause 26.5.4.3.

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
        ASSERT(ocwMin >= 0);
        ASSERT(ocwMin <= ocwMax);
        for (int ac = AC_BK; ac <= AC_VO; ac++) {
            int index = getAccessCategoryIndex(static_cast<AccessCategory>(ac));
            ofdmaContentionWindows[index] = ocwMin;
            ofdmaBackoffs[index] = intuniform(0, ocwMin);
        }
        scheduler = check_and_cast<IIeee80211HeUlScheduler *>(getParentModule()->getSubmodule("ulScheduler"));
        triggerPolicy = check_and_cast<IIeee80211HeUlTriggerPolicy *>(getParentModule()->getSubmodule("ulTriggerPolicy"));
        basicTriggerSentSignal = registerSignal("heUlBasicTriggerSent");
        bsrpTriggerSentSignal = registerSignal("heUlBsrpTriggerSent");
        bufferStatusUpdatedSignal = registerSignal("heUlBufferStatusUpdated");
        bufferStatusReportedBytesSignal = registerSignal("heUlBufferStatusReportedBytes");
        bufferStatusScheduledBytesSignal = registerSignal("heUlBufferStatusScheduledBytes");
        staleReportSignal = registerSignal("heUlStaleBufferStatus");
        scheduledUsersSignal = registerSignal("heUlScheduledUsers");
        randomAccessRusSignal = registerSignal("heUlRandomAccessRus");
        randomAccessAttemptSignal = registerSignal("heUlRandomAccessAttempt");
        randomAccessSuccessSignal = registerSignal("heUlRandomAccessSuccess");

        WATCH(nextTriggerId);
        WATCH_EXPR("lastTriggerTime", lastTriggerTime.str());
        WATCH(hasSentTrigger);
        WATCH_MAP(bufferStatusByAid);
        WATCH_EXPR("freshReports", getFreshReportCount());
        WATCH_EXPR("backloggedReports", getBackloggedReportCount());
        WATCH_EXPR("elapsedSinceLastTrigger", hasSentTrigger ? simTime() - lastTriggerTime : SIMTIME_MAX);
        WATCH_EXPR("bufferStatusSummary", getBufferStatusSummary());
    }
}

int HeUlCoordinator::getAccessCategoryIndex(AccessCategory ac)
{
    ASSERT(ac >= AC_BK && ac <= AC_VO);
    return static_cast<int>(ac);
}

int HeUlCoordinator::getFreshReportCount() const
{
    int count = 0;
    for (const auto& entry : bufferStatusByAid)
        if (simTime() - entry.second.updateTime <= reportMaxAge)
            count++;
    return count;
}

int HeUlCoordinator::getBackloggedReportCount() const
{
    int count = 0;
    for (const auto& entry : bufferStatusByAid) {
        bool backlogged = false;
        for (auto bytes : entry.second.backlogBytes)
            if (bytes > 0) {
                backlogged = true;
                break;
            }
        if (backlogged)
            count++;
    }
    return count;
}

std::string HeUlCoordinator::getBufferStatusSummary() const
{
    std::stringstream stream;
    stream << "reports=" << bufferStatusByAid.size()
           << ", fresh=" << getFreshReportCount()
           << ", backlogged=" << getBackloggedReportCount()
           << ", ocw=[" << ofdmaContentionWindows[AC_BK] << ","
                         << ofdmaContentionWindows[AC_BE] << ","
                         << ofdmaContentionWindows[AC_VI] << ","
                         << ofdmaContentionWindows[AC_VO] << "]"
           << ", obo=[" << ofdmaBackoffs[AC_BK] << ","
                         << ofdmaBackoffs[AC_BE] << ","
                         << ofdmaBackoffs[AC_VI] << ","
                         << ofdmaBackoffs[AC_VO] << "]";
    return stream.str();
}

void HeUlCoordinator::updateBufferStatus(uint16_t aid, AccessCategory ac, uint8_t tid,
        int64_t backlogBytes, bool retryPending)
{
    // IEEE 802.11-2024 Clause 26.5.2 ("Uplink multi-user operation").
    // HE STAs report their queue backlogs using Buffer Status Reports (BSRs)
    // carried inside the HE Variant QoS Control fields or in BSRP trigger frame responses.
    // The AP caches this AID backlog state to inform its uplink scheduler.
    ASSERT(aid != 0);
    ASSERT(ac >= AC_BK && ac <= AC_VO);
    auto& status = bufferStatusByAid[aid];
    status.backlogBytes[ac] = std::max<int64_t>(0, backlogBytes);
    status.tid[ac] = tid;
    status.retryPending = retryPending;
    status.updateTime = simTime();
    emit(bufferStatusUpdatedSignal, (long)aid);
    emit(bufferStatusReportedBytesSignal, (long)status.backlogBytes[ac]);
}

void HeUlCoordinator::clearStation(uint16_t aid)
{
    bufferStatusByAid.erase(aid);
}

IIeee80211HeUlTriggerPolicy::TriggerType HeUlCoordinator::selectTrigger(const Ieee80211Mib *mib) const
{
    ASSERT(mib != nullptr);
    ASSERT(triggerPolicy != nullptr);
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
    auto triggerType = triggerPolicy->selectTrigger(context);
    EV_DEBUG << "HE UL trigger decision: associated=" << context.associatedStations
             << ", freshReports=" << context.freshReports
             << ", backlogged=" << context.backloggedStations
             << ", retries=" << context.retryStations
             << ", selected=" << triggerType << "\n";
    return triggerType;
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
        Hz centerFrequency, Hz bandwidth, simtime_t txopLimit, simtime_t requestedDuration,
        double sensitivityDbm, double targetRssiMarginDb,
        int estimatedRaContenders, double collisionRate, double idleRate)
{
    ASSERT(mib != nullptr);
    ASSERT(scheduler != nullptr);
    ASSERT(centerFrequency > Hz(0));
    ASSERT(bandwidth > Hz(0));
    ASSERT(requestedDuration > SIMTIME_ZERO);
    ASSERT(estimatedRaContenders >= 0);
    ASSERT(collisionRate >= 0 && collisionRate <= 1);
    ASSERT(idleRate >= 0 && idleRate <= 1);
    IIeee80211HeUlScheduler::ScheduleContext context;
    context.channelCenterFrequency = centerFrequency;
    context.channelBandwidth = bandwidth;
    context.txopLimit = txopLimit;
    context.requestedDuration = requestedDuration;
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
        if (status == bufferStatusByAid.end() || simTime() - status->second.updateTime > reportMaxAge) {
            emit(staleReportSignal, (long)aid);
            continue;
        }
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
    std::set<int> ruIndices;
    std::set<uint16_t> scheduledAids;
    for (const auto& allocation : schedule.allocations) {
        ASSERT(allocation.ru.toneSize > 0);
        ASSERT(ruIndices.insert(allocation.ru.index).second);
        if (allocation.randomAccess)
            ASSERT(allocation.associationId == 0);
        else {
            ASSERT(allocation.associationId != 0);
            ASSERT(scheduledAids.insert(allocation.associationId).second);
        }
    }
    if (!schedule.allocations.empty())
        ASSERT(schedule.commonDuration > SIMTIME_ZERO);
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
    if (schedule.coding == physicallayer::HE_CODING_BCC) {
        schedule.allocations.erase(std::remove_if(schedule.allocations.begin(), schedule.allocations.end(),
                [] (const auto& allocation) {
                    return allocation.ru.toneSize >= 484;
                }), schedule.allocations.end());
        for (auto& allocation : schedule.allocations)
            allocation.mcs = std::min(allocation.mcs, 9);
    }
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
        if (!allocation.randomAccess) {
            auto& status = bufferStatusByAid[allocation.associationId];
            status.lastService = simTime();
            status.scheduledBytes[allocation.accessCategory] = status.backlogBytes[allocation.accessCategory];
            emit(bufferStatusScheduledBytesSignal, (long)status.scheduledBytes[allocation.accessCategory]);
        }
    EV_INFO << "HE UL schedule: candidates=" << context.candidates.size()
             << ", scheduledUsers=" << scheduledUsers
             << ", randomAccessRUs=" << randomAccessRus
             << ", commonDuration=" << schedule.commonDuration << "\n";
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
    ASSERT(triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER ||
            triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ||
            triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER);
    lastTriggerTime = simTime();
    hasSentTrigger = true;
    if (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER)
        emit(bsrpTriggerSentSignal, 1L);
    else if (triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER)
        emit(basicTriggerSentSignal, 1L);
}

int HeUlCoordinator::selectRandomAccessRu(AccessCategory ac, int randomAccessRuCount)
{
    // IEEE 802.11-2024 Clause 26.5.4 ("Uplink OFDMA random access").
    // HE STAs contend for Random Access RUs (AID=0) using the UORA procedure.
    // The OFDMA Backoff (OBO) counter is decremented by the number of RA-RUs (randomAccessRuCount)
    // present in the Trigger frame.
    if (randomAccessRuCount <= 0)
        return -1;
    int acIndex = getAccessCategoryIndex(ac);
    int& ofdmaContentionWindow = ofdmaContentionWindows[acIndex];
    int& ofdmaBackoff = ofdmaBackoffs[acIndex];
    ASSERT(ofdmaContentionWindow >= ocwMin && ofdmaContentionWindow <= ocwMax);
    ASSERT(ofdmaBackoff >= 0 && ofdmaBackoff <= ofdmaContentionWindow);
    if (ofdmaBackoff >= randomAccessRuCount) {
        ofdmaBackoff -= randomAccessRuCount;
        EV_INFO << "HE UORA deferred: ac=" << (int)ac
                 << ", backoff=" << ofdmaBackoff
                 << ", advertisedRUs=" << randomAccessRuCount << "\n";
        return -1;
    }
    // When OBO reaches 0, the STA attempts random access and selects one of the RA-RUs uniformly at random.
    ofdmaBackoff = 0;
    emit(randomAccessAttemptSignal, 1L);
    auto selectedRu = intuniform(0, randomAccessRuCount - 1);
    EV_INFO << "HE UORA attempt: ac=" << (int)ac
             << ", selected RU " << selectedRu
             << " from " << randomAccessRuCount << " advertised RUs\n";
    return selectedRu;
}

void HeUlCoordinator::reportRandomAccessResult(AccessCategory ac, bool success)
{
    // IEEE 802.11-2024 Clause 26.5.4.3 ("OFDMA contention window (OCW) update").
    // If the transmission succeeds, OCW is reset to OCW_min.
    // If the transmission fails (collision/no ACK), OCW is doubled (OCW = min(OCW_max, 2 * OCW + 1)).
    // A new random OBO is then selected in [0, OCW].
    int acIndex = getAccessCategoryIndex(ac);
    int& ofdmaContentionWindow = ofdmaContentionWindows[acIndex];
    int& ofdmaBackoff = ofdmaBackoffs[acIndex];
    if (success)
        ofdmaContentionWindow = ocwMin;
    else
        ofdmaContentionWindow = std::min(ocwMax, 2 * ofdmaContentionWindow + 1);
    ofdmaBackoff = intuniform(0, ofdmaContentionWindow);
    EV_INFO << "HE UORA result: " << (success ? "success" : "failure")
             << ", ac=" << (int)ac
             << ", OCW=" << ofdmaContentionWindow
             << ", nextBackoff=" << ofdmaBackoff << "\n";
    if (success)
        emit(randomAccessSuccessSignal, 1L);
}

} // namespace ieee80211
} // namespace inet
