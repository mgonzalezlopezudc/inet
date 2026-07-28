//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>

#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"

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
        triggerDecisionCommittedSignal = registerSignal("heUlTriggerDecisionCommitted");

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
        for (int ac = AC_BK; ac <= AC_VO; ++ac)
            if (entry.second.backlogBytes[ac] > 0 ||
                    entry.second.backlogEstimates[ac].kind ==
                            Ieee80211HeQueueSizeKind::UNKNOWN) {
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

void HeUlCoordinator::updateBufferStatus(uint16_t aid, const MacAddress& stationAddress,
        AccessCategory ac, uint8_t tid,
        int64_t backlogBytes)
{
    Ieee80211HeQueueSizeEstimate estimate;
    estimate.lowerBoundBytes = std::max<int64_t>(0, backlogBytes);
    estimate.upperBoundBytes = estimate.lowerBoundBytes;
    updateBufferStatus(aid, stationAddress, ac, tid, estimate);
}

void HeUlCoordinator::updateBufferStatus(uint16_t aid, const MacAddress& stationAddress,
        AccessCategory ac, uint8_t tid,
        const Ieee80211HeQueueSizeEstimate& estimate)
{
    // IEEE 802.11-2024 Clause 26.5.2 ("Uplink multi-user operation").
    // HE STAs report their queue backlogs using Buffer Status Reports (BSRs)
    // carried inside the HE Variant QoS Control fields or in BSRP trigger frame responses.
    // The AP caches this AID backlog state to inform its uplink scheduler.
    ASSERT(aid != 0);
    ASSERT(!stationAddress.isUnspecified());
    ASSERT(ac >= AC_BK && ac <= AC_VO);
    auto& status = bufferStatusByAid[aid];
    if (!status.stationAddress.isUnspecified() && status.stationAddress != stationAddress)
        status = BufferStatus {};
    status.stationAddress = stationAddress;
    status.backlogEstimates[ac] = estimate;
    status.backlogBytes[ac] = std::min<uint64_t>(estimate.getConservativeBytes(),
            std::numeric_limits<int64_t>::max());
    status.tid[ac] = tid;
    status.retryPending[ac] = false;
    status.updateTime = simTime();
    emit(bufferStatusUpdatedSignal, (long)aid);
    emit(bufferStatusReportedBytesSignal, (long)status.backlogBytes[ac]);
}

void HeUlCoordinator::updateBufferStatus(uint16_t aid, const MacAddress& stationAddress,
        AccessCategory ac, uint8_t tid,
        int64_t backlogBytes, bool receivedRetry)
{
    // Compatibility overload: Retry describes the MPDU that was successfully
    // received, not future work at the originator. Never cache or schedule it.
    (void)receivedRetry;
    updateBufferStatus(aid, stationAddress, ac, tid, backlogBytes);
}

void HeUlCoordinator::clearStation(const MacAddress& stationAddress)
{
    for (auto it = bufferStatusByAid.begin(); it != bufferStatusByAid.end(); )
        if (it->second.stationAddress == stationAddress)
            it = bufferStatusByAid.erase(it);
        else
            ++it;
}

void HeUlCoordinator::invalidatePeer(const MacAddress& stationAddress)
{
    clearStation(stationAddress);
    if (scheduler != nullptr)
        scheduler->invalidatePeer(stationAddress);
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
        if (status == bufferStatusByAid.end() ||
                status->second.stationAddress != station.first ||
                simTime() - status->second.updateTime > reportMaxAge)
            continue;
        context.freshReports++;
        for (int ac = AC_BK; ac <= AC_VO; ++ac)
            if (status->second.backlogBytes[ac] > 0 ||
                    status->second.backlogEstimates[ac].kind ==
                            Ieee80211HeQueueSizeKind::UNKNOWN) {
                context.backloggedStations++;
                break;
            }
    }
    context.elapsedSinceLastTrigger = hasSentTrigger ? simTime() - lastTriggerTime : SIMTIME_MAX;
    auto triggerType = triggerPolicy->selectTrigger(context);
    EV_DEBUG << "HE UL trigger decision: associated=" << context.associatedStations
             << ", freshReports=" << context.freshReports
             << ", backlogged=" << context.backloggedStations
             << ", selected=" << triggerType << "\n";
    return triggerType;
}

AccessCategory HeUlCoordinator::getPreferredAccessCategory() const
{
    for (int ac = AC_VO; ac >= AC_BK; ac--)
        for (const auto& entry : bufferStatusByAid)
            if (entry.second.backlogBytes[ac] > 0)
                return static_cast<AccessCategory>(ac);
    for (int ac = AC_VO; ac >= AC_BK; ac--)
        for (const auto& entry : bufferStatusByAid)
            if (entry.second.backlogEstimates[ac].kind ==
                    Ieee80211HeQueueSizeKind::UNKNOWN)
                return static_cast<AccessCategory>(ac);
    return AC_BE;
}

IIeee80211HeUlScheduler::Schedule HeUlCoordinator::prepareSchedule(const Ieee80211Mib *mib,
        const IIeee80211HeLinkPhyContext& linkPhyContext, simtime_t maximumLinkEstimateAge,
        Hz centerFrequency, Hz bandwidth, simtime_t txopLimit, simtime_t requestedDuration,
        double sensitivityDbm, double targetRssiMarginDb,
        int estimatedRaContenders, double collisionRate, double idleRate,
        const std::function<bool(const MacAddress&)>& isUlMuDisabled,
        IIeee80211HeUlScheduler::ScheduleContext *preparedContext,
        const physicallayer::Ieee80211HeTbCapacityBoundary *finalizedBoundary,
        bool useUlMuMimoPolicy)
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
    if (finalizedBoundary != nullptr)
        context.finalizedBoundary = *finalizedBoundary;
    context.apSensitivityDbm = sensitivityDbm;
    context.targetRssiMarginDb = targetRssiMarginDb;
    context.estimatedRandomAccessContenders = estimatedRaContenders;
    context.recentRandomAccessCollisionRate = collisionRate;
    context.recentRandomAccessIdleRate = idleRate;
    context.useUlMuMimoPolicy = useUlMuMimoPolicy;
    std::vector<uint16_t> staleReportAids;
    for (const auto& station : mib->bssAccessPointData.stations) {
        if (station.second != Ieee80211Mib::ASSOCIATED)
            continue;
        auto aid = mib->getAssociationId(station.first);
        auto status = bufferStatusByAid.find(aid);
        if (status == bufferStatusByAid.end() ||
                status->second.stationAddress != station.first ||
                simTime() - status->second.updateTime > reportMaxAge) {
            staleReportAids.push_back(aid);
            continue;
        }
        IIeee80211HeUlScheduler::CandidateInfo candidate;
        candidate.staAddress = station.first;
        candidate.associationId = aid;
        candidate.backlogBytes = status->second.backlogBytes;
        candidate.backlogEstimates = status->second.backlogEstimates;
        candidate.hasTypedBacklogEstimates = true;
        candidate.retryPending = false;
        candidate.reportAge = simTime() - status->second.updateTime;
        candidate.hasFreshReport = true;
        candidate.lastService = status->second.lastService;
        bool selected = false;
        for (int ac = AC_VO; ac >= AC_BK; ac--)
            if (candidate.backlogBytes[ac] > 0) {
                candidate.selectedAccessCategory = static_cast<AccessCategory>(ac);
                candidate.selectedTid = status->second.tid[ac];
                selected = true;
                break;
            }
        if (!selected)
            for (int ac = AC_VO; ac >= AC_BK; ac--)
                if (candidate.backlogEstimates[ac].kind ==
                        Ieee80211HeQueueSizeKind::UNKNOWN) {
                    candidate.selectedAccessCategory = static_cast<AccessCategory>(ac);
                    candidate.selectedTid = status->second.tid[ac];
                    break;
                }
        const auto peer = linkPhyContext.getPeerSnapshot(station.first, maximumLinkEstimateAge);
        candidate.pathLossDb = peer.getPathLossDb();
        candidate.hasFreshPathLoss = peer.getHasFreshPathLoss();
        if (auto negotiated = mib->findNegotiatedHeCapabilities(station.first)) {
            candidate.hasNegotiatedHeCapabilities = true;
            candidate.negotiatedHeCapabilities = *negotiated;
            candidate.coding = mib->localHeCapabilities.ldpc &&
                    negotiated->localRxPeerTx.valid && negotiated->mutual.ldpc ?
                    physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
        }
        candidate.ulMuDisabled = isUlMuDisabled && isUlMuDisabled(station.first);
        context.candidates.push_back(candidate);
    }
    if (!context.candidates.empty()) {
        auto anchor = std::min_element(context.candidates.begin(), context.candidates.end(),
                [] (const auto& left, const auto& right) { return left.lastService < right.lastService; });
        anchor->anchor = true;
    }
    if (preparedContext != nullptr)
        *preparedContext = context;
    auto schedule = scheduler->schedule(context);
    schedule.staleReportAids = std::move(staleReportAids);
    schedule.packetExtensionDurationUs = mib->heOperation.defaultPeDurationUs;
    EV_DEBUG << "Prepared HE UL schedule without committing coordinator state: candidates="
             << context.candidates.size() << ", allocations=" << schedule.allocations.size()
             << ", commonDuration=" << schedule.commonDuration << "\n";
    return schedule;
}

void HeUlCoordinator::commitSchedule(const IIeee80211HeUlScheduler::Schedule& schedule)
{
    committedBasicTriggerUsers.clear();
    for (auto aid : schedule.staleReportAids)
        emit(staleReportSignal, (long)aid);
    long scheduledUsers = 0;
    long randomAccessRus = 0;
    for (const auto& allocation : schedule.allocations)
        if (allocation.randomAccess) {
            randomAccessRus++;
            HeUlTriggerDecisionEvent::UserInfo user;
            user.associationId = 0;
            user.selected = true;
            user.ruIndex = allocation.ru.index;
            user.ruToneSize = allocation.ru.toneSize;
            user.ruToneOffset = allocation.ru.toneOffset;
            user.tid = allocation.tid;
            user.accessCategory = allocation.accessCategory;
            committedBasicTriggerUsers.push_back(user);
        }
        else {
            scheduledUsers++;
            auto status = bufferStatusByAid.find(allocation.associationId);
            if (status == bufferStatusByAid.end())
                throw cRuntimeError("Cannot commit HE UL allocation for unknown AID %u",
                        allocation.associationId);
            if (status->second.stationAddress != allocation.staAddress)
                throw cRuntimeError("Cannot commit HE UL allocation for stale AID %u owner",
                        allocation.associationId);
            status->second.lastService = simTime();
            status->second.scheduledBytes[allocation.accessCategory] =
                    allocation.plannedBytes;
            HeUlTriggerDecisionEvent::UserInfo user;
            user.associationId = allocation.associationId;
            user.backlogBytes = std::accumulate(status->second.backlogBytes.begin(),
                    status->second.backlogBytes.end(), INT64_C(0));
            user.reportedBytes = status->second.backlogBytes[allocation.accessCategory];
            user.plannedBytes = allocation.plannedBytes;
            user.selectedBytes = user.plannedBytes;
            user.tid = allocation.tid;
            user.accessCategory = allocation.accessCategory;
            user.selected = true;
            user.ruIndex = allocation.ru.index;
            user.ruToneSize = allocation.ru.toneSize;
            user.ruToneOffset = allocation.ru.toneOffset;
            committedBasicTriggerUsers.push_back(user);
            emit(bufferStatusScheduledBytesSignal,
                    (long)status->second.scheduledBytes[allocation.accessCategory]);
        }
    emit(scheduledUsersSignal, scheduledUsers);
    emit(randomAccessRusSignal, randomAccessRus);
    EV_INFO << "Committed HE UL schedule: scheduledUsers=" << scheduledUsers
            << ", randomAccessRUs=" << randomAccessRus
            << ", commonDuration=" << schedule.commonDuration << "\n";
}

uint32_t HeUlCoordinator::allocateTriggerId()
{
    return allocateIeee80211HeTriggerId();
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

    HeUlTriggerDecisionEvent event;
    event.triggerType = triggerType;
    event.reason = triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER ?
            HeUlTriggerDecisionEvent::BACKLOG_REPORTED :
            triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ?
                    HeUlTriggerDecisionEvent::REPORT_REFRESH_NEEDED :
                    HeUlTriggerDecisionEvent::NDP_FEEDBACK_ENABLED;
    if (triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER)
        event.users = committedBasicTriggerUsers;
    else {
        for (const auto& entry : bufferStatusByAid) {
            HeUlTriggerDecisionEvent::UserInfo user;
            user.associationId = entry.first;
            user.backlogBytes = std::accumulate(entry.second.backlogBytes.begin(),
                    entry.second.backlogBytes.end(), INT64_C(0));
            for (int ac = AC_VO; ac >= AC_BK; --ac)
                if (entry.second.backlogBytes[ac] > 0) {
                    user.accessCategory = static_cast<AccessCategory>(ac);
                    user.tid = entry.second.tid[ac];
                    user.reportedBytes = entry.second.backlogBytes[ac];
                    break;
                }
            event.users.push_back(user);
        }
    }
    emit(triggerDecisionCommittedSignal, &event);
    committedBasicTriggerUsers.clear();
}

HeUlCoordinator::PreparedRandomAccessSelection HeUlCoordinator::prepareRandomAccessRu(
        AccessCategory ac, int randomAccessRuCount)
{
    // IEEE 802.11-2024 Clause 26.5.4 ("Uplink OFDMA random access").
    // HE STAs contend for Random Access RUs (AID=0) using the UORA procedure.
    // The OFDMA Backoff (OBO) counter is decremented by the number of RA-RUs (randomAccessRuCount)
    // present in the Trigger frame.
    PreparedRandomAccessSelection selection;
    selection.accessCategory = ac;
    selection.randomAccessRuCount = randomAccessRuCount;
    if (randomAccessRuCount <= 0)
        return selection;
    int acIndex = getAccessCategoryIndex(ac);
    int ofdmaContentionWindow = ofdmaContentionWindows[acIndex];
    int ofdmaBackoff = ofdmaBackoffs[acIndex];
    ASSERT(ofdmaContentionWindow >= ocwMin && ofdmaContentionWindow <= ocwMax);
    ASSERT(ofdmaBackoff >= 0 && ofdmaBackoff <= ofdmaContentionWindow);
    selection.originalBackoff = ofdmaBackoff;
    if (ofdmaBackoff > randomAccessRuCount) {
        selection.resultingBackoff = ofdmaBackoff - randomAccessRuCount;
        return selection;
    }
    // The random draw is part of commit. Preparation establishes only the
    // normative OBO transition and whether this Trigger permits an attempt.
    selection.resultingBackoff = 0;
    selection.attempt = true;
    return selection;
}

int HeUlCoordinator::commitRandomAccessRu(
        const PreparedRandomAccessSelection& selection)
{
    if (selection.randomAccessRuCount <= 0)
        return -1;
    int acIndex = getAccessCategoryIndex(selection.accessCategory);
    if (ofdmaBackoffs[acIndex] != selection.originalBackoff)
        throw cRuntimeError("HE UORA backoff changed between preparation and commit");
    ofdmaBackoffs[acIndex] = selection.resultingBackoff;
    if (selection.attempt) {
        int selectedRu = intuniform(0, selection.randomAccessRuCount - 1);
        emit(randomAccessAttemptSignal, 1L);
        EV_INFO << "HE UORA attempt: ac=" << (int)selection.accessCategory
                << ", selected RU " << selectedRu
                << " from " << selection.randomAccessRuCount << " advertised RUs\n";
        return selectedRu;
    }
    else
        EV_INFO << "HE UORA deferred: ac=" << (int)selection.accessCategory
                << ", backoff=" << selection.resultingBackoff
                << ", advertisedRUs=" << selection.randomAccessRuCount << "\n";
    return -1;
}

int HeUlCoordinator::selectRandomAccessRu(AccessCategory ac, int randomAccessRuCount)
{
    auto selection = prepareRandomAccessRu(ac, randomAccessRuCount);
    return commitRandomAccessRu(selection);
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
