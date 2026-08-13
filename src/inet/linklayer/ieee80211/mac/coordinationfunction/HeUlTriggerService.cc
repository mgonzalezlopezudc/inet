//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlTriggerService.h"

#include <algorithm>
#include <cmath>

#include "inet/common/INETMath.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"

namespace inet {
namespace ieee80211 {

HeUlTriggerService::~HeUlTriggerService()
{
    if (triggerTimer != nullptr) {
        if (owner != nullptr)
            owner->cancelAndDelete(triggerTimer);
        else
            delete triggerTimer;
    }
}

void HeUlTriggerService::configure(IActions *actions,
        IHeUlMuExchangeCallback *exchangeCallback, HeUlCoordinator *coordinator,
        simtime_t checkInterval)
{
    if (actions == nullptr || exchangeCallback == nullptr || coordinator == nullptr ||
            checkInterval <= SIMTIME_ZERO)
        throw cRuntimeError("HE UL Trigger service requires actions, callback, coordinator, and a positive interval");
    this->actions = actions;
    this->exchangeCallback = exchangeCallback;
    this->coordinator = coordinator;
    this->checkInterval = checkInterval;
    triggerTimer = new cMessage("heUlTriggerTimer");
}

void HeUlTriggerService::start(cSimpleModule *owner)
{
    if (owner == nullptr || coordinator == nullptr || triggerTimer == nullptr)
        throw cRuntimeError("HE UL Trigger service is not configured");
    this->owner = owner;
    if (coordinator->isEnabled() && !triggerTimer->isScheduled())
        owner->scheduleAfter(checkInterval, triggerTimer);
}

bool HeUlTriggerService::handleTimer(cMessage *message, cSimpleModule *owner)
{
    if (message != triggerTimer)
        return false;
    owner->scheduleAfter(checkInterval, triggerTimer);
    if (!coordinator->isEnabled() || accessRequested || !actions->canRequestHeUlTrigger())
        return true;
    auto triggerType = actions->isNdpFeedbackReportEnabled() ?
            IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER :
            coordinator->selectTrigger(actions->getHeUlMib());
    if (triggerType == IIeee80211HeUlTriggerPolicy::NO_TRIGGER)
        return true;
    pendingTrigger = triggerType;
    accessRequested = true;
    auto accessCategory = triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER ?
            coordinator->getPreferredAccessCategory() : AC_BE;
    actions->requestHeUlChannelAccess(accessCategory);
    return true;
}

bool HeUlTriggerService::hasPendingTrigger() const
{
    return pendingTrigger != IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
}

HeUlScheduleFinalizationResult HeUlTriggerService::finalizeSchedule(
        const IIeee80211HeUlScheduler::Schedule& proposedSchedule,
        Hz centerFrequency, Hz channelBandwidth,
        IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
        physicallayer::Ieee80211HeTriggerResponseFinalizationResult *finalizationSnapshot)
{
    HeUlScheduleFinalizationResult result;
    if (finalizationSnapshot != nullptr)
        *finalizationSnapshot = {};
    result.schedule = proposedSchedule;
    result.schedule.channelBandwidth = channelBandwidth;
    result.schedule.ulLength = 0;
    result.schedule.commonDuration = SIMTIME_ZERO;
    result.schedule.commonDurationExact = false;
    const bool feedbackNdp = triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER;
    if (proposedSchedule.allocations.empty() && !feedbackNdp) {
        result.error = "HE UL schedule has no RU allocations";
        return result;
    }
    if (proposedSchedule.commonDuration <= SIMTIME_ZERO) {
        result.error = "HE UL schedule has no positive duration budget";
        return result;
    }
    if (triggerType != IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        result.error = "HE UL schedule has an invalid Trigger type";
        return result;
    }
    std::vector<physicallayer::Ieee80211HeUserPhyParameters> users;
    users.reserve(feedbackNdp ? 1 : proposedSchedule.allocations.size());
    if (feedbackNdp) {
        physicallayer::Ieee80211HeUserPhyParameters user;
        user.ru = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth, 1).front();
        user.mcs = 0;
        user.numberOfSpatialStreams = 1;
        user.coding = physicallayer::HE_CODING_BCC;
        user.psduLength = B(0);
        user.ndpFeedbackReport = true;
        user.ndpRuToneSetIndex = 1;
        users.push_back(user);
    }
    for (const auto& allocation : proposedSchedule.allocations) {
        physicallayer::Ieee80211HeUserPhyParameters user;
        user.ru = allocation.ru;
        user.mcs = allocation.mcs;
        user.numberOfSpatialStreams = allocation.numberOfSpatialStreams;
        user.streamStartIndex = allocation.streamStartIndex;
        user.staId = allocation.associationId;
        user.coding = allocation.coding;
        user.psduLength = B(1);
        users.push_back(user);
    }
    physicallayer::Ieee80211HeTriggerResponseFinalizationRequest request;
    request.users = users;
    request.centerFrequency = centerFrequency;
    request.channelBandwidth = channelBandwidth;
    request.guardInterval = proposedSchedule.guardInterval;
    request.ltfType = proposedSchedule.ltfType;
    request.packetExtensionDurationUs = proposedSchedule.packetExtensionDurationUs;
    request.noSignalExtension = proposedSchedule.noSignalExtension;
    request.durationBudget = proposedSchedule.commonDuration;
    if (!feedbackNdp && proposedSchedule.ulLength != 0) {
        physicallayer::Ieee80211HeTbCapacityBoundary boundary;
        boundary.channelBandwidth = channelBandwidth;
        boundary.ulLength = proposedSchedule.ulLength;
        boundary.guardInterval = proposedSchedule.guardInterval;
        boundary.ltfType = proposedSchedule.ltfType;
        boundary.preFecPaddingFactor = proposedSchedule.preFecPaddingFactor;
        boundary.ldpcExtraSymbolSegment = proposedSchedule.ldpcExtraSymbolSegment;
        boundary.peDisambiguity = proposedSchedule.peDisambiguity;
        boundary.numberOfHeLtfSymbols = proposedSchedule.numberOfHeLtfSymbols;
        boundary.packetExtensionDurationUs = proposedSchedule.packetExtensionDurationUs;
        request.fixedBoundary = boundary;
    }
    auto finalization = physicallayer::finalizeHeTriggerResponse(request);
    if (!finalization) {
        result.error = finalization.error;
        return result;
    }
    result.schedule.ulLength = finalization.ulLength;
    result.schedule.commonDuration = finalization.commonDuration;
    result.schedule.commonDurationExact = finalization.commonDurationExact;
    result.schedule.numberOfHeLtfSymbols = finalization.parameters.common.numberOfHeLtfSymbols;
    if (!feedbackNdp) {
        result.schedule.preFecPaddingFactor = finalization.parameters.common.preFecPaddingFactor;
        result.schedule.ldpcExtraSymbolSegment = finalization.parameters.common.ldpcExtraSymbol;
        result.schedule.peDisambiguity = finalization.peDisambiguity;
        result.schedule.packetExtensionDurationUs = finalization.parameters.common.packetExtensionDurationUs;
    }
    result.resolvedTxTime = finalization.resolvedTxTime;
    if (finalizationSnapshot != nullptr)
        *finalizationSnapshot = finalization;
    result.valid = true;
    return result;
}

static bool hasServiceRequest(const HeUlBufferStatusSnapshot& status)
{
    return std::any_of(status.backlogEstimates.begin(), status.backlogEstimates.end(),
            [] (const auto& estimate) { return estimate.getConservativeBytes() > 0 ||
                    estimate.kind == Ieee80211HeQueueSizeKind::UNKNOWN; });
}

HeUlCandidatePreparation HeUlTriggerService::buildCandidates(
        const HeUlPreparationSnapshot& snapshot)
{
    HeUlCandidatePreparation result;
    if (!snapshot.phy)
        return result;
    const auto& phy = *snapshot.phy;
    auto& context = result.context;
    const auto bandwidth = phy.getChannelBandwidth();
    const auto fullRu = physicallayer::getHeEqualRuLayout(
            phy.getChannelCenterFrequency(), bandwidth, 1).front();
    context.channelCenterFrequency = phy.getChannelCenterFrequency();
    context.channelBandwidth = bandwidth;
    context.txopLimit = snapshot.txopLimit;
    context.requestedDuration = snapshot.maxHeTbPpduDuration;
    context.apSensitivityDbm = math::mW2dBmW(phy.getReceiveSensitivity().get<mW>());
    context.targetRssiMarginDb = snapshot.targetRssiMarginDb;
    for (const auto& peer : snapshot.peers) {
        if (!peer.bufferStatus || peer.bufferStatus->stationAddress != peer.stationAddress ||
                snapshot.now - peer.bufferStatus->updateTime > snapshot.reportMaxAge) {
            result.staleReportAids.push_back(peer.associationId);
            continue;
        }
        const auto& status = *peer.bufferStatus;
        IIeee80211HeUlScheduler::CandidateInfo candidate;
        candidate.staAddress = peer.stationAddress;
        candidate.associationId = peer.associationId;
        candidate.backlogBytes = status.backlogBytes;
        candidate.backlogEstimates = status.backlogEstimates;
        candidate.hasTypedBacklogEstimates = true;
        candidate.reportAge = snapshot.now - status.updateTime;
        candidate.hasFreshReport = true;
        candidate.lastService = status.lastService;
        for (int ac = AC_VO; ac >= AC_BK; --ac)
            if (candidate.backlogBytes[ac] > 0 ||
                    candidate.backlogEstimates[ac].kind == Ieee80211HeQueueSizeKind::UNKNOWN) {
                candidate.selectedAccessCategory = static_cast<AccessCategory>(ac);
                candidate.selectedTid = status.tid[ac];
                break;
            }
        candidate.pathLossDb = peer.pathLossDb;
        candidate.hasFreshPathLoss = peer.hasFreshPathLoss;
        if (peer.negotiatedCapabilities) {
            candidate.hasNegotiatedHeCapabilities = true;
            candidate.negotiatedHeCapabilities = *peer.negotiatedCapabilities;
            candidate.coding = phy.getLocalHeCapabilities().ldpc &&
                    peer.negotiatedCapabilities->localRxPeerTx.valid &&
                    peer.negotiatedCapabilities->mutual.ldpc ?
                    physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
        }
        candidate.ulMuDisabled = !peer.twtEligible || peer.ulMuDisabled;
        context.candidates.push_back(candidate);
        if (hasServiceRequest(status) && !candidate.ulMuDisabled &&
                peer.negotiatedCapabilities &&
                peer.negotiatedCapabilities->localRxPeerTx.valid) {
            const auto& rx = peer.negotiatedCapabilities->localRxPeerTx;
            if (rx.fullBandwidthUlMuMimo &&
                    rx.supportedRuToneSizes.count(fullRu.toneSize) &&
                    rx.mcsNss.maxMcsPerNss[0] >= 0)
                result.fullBandwidthMuMimoCandidates++;
            if (rx.ofdma && rx.supportedChannelWidths.count(bandwidth) &&
                    !rx.supportedRuToneSizes.empty()) {
                int nss = getMaxNss(rx.mcsNss);
                if (!(phy.getLocalHeCapabilities().ldpc &&
                        peer.negotiatedCapabilities->mutual.ldpc))
                    nss = std::min(nss, 4);
                result.maximumOfdmaUserNss = std::max(result.maximumOfdmaUserNss, nss);
                if (rx.fullBandwidthUlMuMimo)
                    result.totalUlMuMimoNss += std::min(nss, 4);
            }
        }
    }
    if (!context.candidates.empty())
        std::min_element(context.candidates.begin(), context.candidates.end(),
                [] (const auto& a, const auto& b) {
                    return a.lastService < b.lastService;
                })->anchor = true;
    context.estimatedRandomAccessContenders = result.staleReportAids.size();
    context.useUlMuMimoPolicy = snapshot.enableUlMuMimo &&
            result.fullBandwidthMuMimoCandidates >= 2;
    return result;
}

std::optional<HeUlTriggerService::PreparedStart> HeUlTriggerService::prepareStart(AccessCategory accessCategory,
        const HeUlPreparationSnapshot& snapshot)
{
    if (!hasPendingTrigger() || !coordinator->isEnabled() || !snapshot.phy)
        return std::nullopt;
    const auto triggerType = pendingTrigger;
    const auto& phy = *snapshot.phy;
    const auto centerFrequency = phy.getChannelCenterFrequency();
    const auto channelBandwidth = phy.getChannelBandwidth();
    if (triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
            std::any_of(phy.getPuncturedSubchannels().begin(), phy.getPuncturedSubchannels().end(),
                    [] (bool punctured) { return punctured; })) {
        EV_WARN << "HE UL skipping Trigger because the modeled HE-TB Trigger fields do not carry a punctured response bandwidth\n";
        return std::nullopt;
    }

    IIeee80211HeUlScheduler::Schedule schedule;
    IIeee80211HeUlScheduler::ScheduleContext schedulerContext;
    bool schedulerPrepared = false;
    const double sensitivityDbm = math::mW2dBmW(phy.getReceiveSensitivity().get<mW>());
    const simtime_t durationBudget = std::min(snapshot.maxHeTbPpduDuration,
            snapshot.txopLimit > SIMTIME_ZERO ? snapshot.txopLimit : snapshot.maxHeTbPpduDuration);
    if (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ||
            triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        const int maxRus = physicallayer::getHeMaxRuCount(channelBandwidth);
        const auto layout = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth, maxRus);
        int index = 0;
        std::vector<uint16_t> nfrpEligibleAids;
        for (const auto& peer : snapshot.peers) {
            if (triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
                    (index >= maxRus || index >= snapshot.maxMuStations))
                break;
            if (!peer.twtEligible)
                continue;
            if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
                    (!peer.negotiatedCapabilities || !peer.negotiatedCapabilities->localRxPeerTx.valid ||
                     !peer.negotiatedCapabilities->localRxPeerTx.transmitterCanTransmitNdpFeedbackReport))
                continue;
            if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
                nfrpEligibleAids.push_back(peer.associationId);
                continue;
            }
            IIeee80211HeUlScheduler::RuAllocation allocation;
            allocation.staAddress = peer.stationAddress;
            allocation.associationId = peer.associationId;
            allocation.ru = layout[index++];
            allocation.targetRssiDbm = std::lround(sensitivityDbm + snapshot.targetRssiMarginDb);
            schedule.allocations.push_back(allocation);
        }
        if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
            if (nfrpEligibleAids.empty())
                return std::nullopt;
            std::sort(nfrpEligibleAids.begin(), nfrpEligibleAids.end());
            const int count = IIeee80211HeUlScheduler::getNfrpScheduledStaCount(channelBandwidth, false);
            uint16_t selectedStart = nfrpEligibleAids.front();
            int selectedCount = -1;
            for (auto aid : nfrpEligibleAids) {
                const auto candidateStart = std::min<int>(aid, 4096 - count);
                const auto begin = std::lower_bound(nfrpEligibleAids.begin(), nfrpEligibleAids.end(), candidateStart);
                const auto end = std::lower_bound(nfrpEligibleAids.begin(), nfrpEligibleAids.end(), candidateStart + count);
                const int candidateCount = end - begin;
                if (candidateCount > selectedCount || (candidateCount == selectedCount && candidateStart < selectedStart)) {
                    selectedStart = candidateStart;
                    selectedCount = candidateCount;
                }
            }
            schedule.nfrpStartingAid = selectedStart;
            schedule.nfrpFeedbackType = 0;
            schedule.nfrpMultiplexingFlag = false;
            schedule.nfrpTargetRssiDbm = std::lround(sensitivityDbm + snapshot.targetRssiMarginDb);
        }
        while (index < maxRus && triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
            IIeee80211HeUlScheduler::RuAllocation allocation;
            allocation.randomAccess = true;
            allocation.ru = layout[index++];
            allocation.targetRssiDbm = std::lround(sensitivityDbm + snapshot.targetRssiMarginDb);
            schedule.allocations.push_back(allocation);
        }
        schedule.commonDuration = durationBudget;
        for (auto& allocation : schedule.allocations)
            allocation.estimatedDuration = schedule.commonDuration;
    }
    else {
        auto candidates = buildCandidates(snapshot);
        schedulerContext = std::move(candidates.context);
        const int boundaryNss = schedulerContext.useUlMuMimoPolicy ?
                std::min(candidates.totalUlMuMimoNss, 8) :
                candidates.maximumOfdmaUserNss;
        const auto boundaryLayout = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth,
                physicallayer::getHeMaxRuCount(channelBandwidth));
        physicallayer::Ieee80211HeUserPhyParameters bccUser;
        bccUser.ru = boundaryLayout.front(); bccUser.mcs = 0; bccUser.coding = physicallayer::HE_CODING_BCC;
        bccUser.numberOfSpatialStreams = std::min(boundaryNss, 4); bccUser.psduLength = B(1);
        auto ldpcUser = bccUser;
        ldpcUser.ru = boundaryLayout[1]; ldpcUser.coding = physicallayer::HE_CODING_LDPC; ldpcUser.numberOfSpatialStreams = boundaryNss;
        physicallayer::Ieee80211HeTriggerResponseFinalizationRequest request;
        request.users = {bccUser, ldpcUser}; request.centerFrequency = centerFrequency; request.channelBandwidth = channelBandwidth;
        request.guardInterval = phy.getGuardInterval() == physicallayer::HE_GI_3_2_US ? physicallayer::HE_GI_3_2_US : physicallayer::HE_GI_1_6_US;
        request.ltfType = request.guardInterval == physicallayer::HE_GI_3_2_US ? physicallayer::HE_LTF_4X :
                schedulerContext.useUlMuMimoPolicy ? physicallayer::HE_LTF_1X : physicallayer::HE_LTF_2X;
        request.packetExtensionDurationUs = phy.getPacketExtensionDurationUs(); request.durationBudget = durationBudget;
        const auto boundaryFinalization = physicallayer::finalizeHeTriggerResponse(request);
        if (!boundaryFinalization)
            return std::nullopt;
        physicallayer::Ieee80211HeTbCapacityBoundary boundary;
        boundary.channelBandwidth = channelBandwidth; boundary.ulLength = boundaryFinalization.ulLength;
        boundary.guardInterval = request.guardInterval; boundary.ltfType = request.ltfType;
        boundary.preFecPaddingFactor = boundaryFinalization.parameters.common.preFecPaddingFactor;
        boundary.ldpcExtraSymbolSegment = boundaryFinalization.parameters.common.ldpcExtraSymbol;
        boundary.peDisambiguity = boundaryFinalization.peDisambiguity;
        boundary.numberOfHeLtfSymbols = boundaryFinalization.parameters.common.numberOfHeLtfSymbols;
        boundary.packetExtensionDurationUs = boundaryFinalization.parameters.common.packetExtensionDurationUs;
        schedulerContext.finalizedBoundary = boundary;
        schedule = coordinator->prepareSchedule(
                schedulerContext, candidates.staleReportAids);
        schedule.ulLength = boundary.ulLength; schedule.guardInterval = boundary.guardInterval; schedule.ltfType = boundary.ltfType;
        schedule.preFecPaddingFactor = boundary.preFecPaddingFactor; schedule.ldpcExtraSymbolSegment = boundary.ldpcExtraSymbolSegment;
        schedule.peDisambiguity = boundary.peDisambiguity; schedule.numberOfHeLtfSymbols = boundary.numberOfHeLtfSymbols;
        schedule.packetExtensionDurationUs = boundary.packetExtensionDurationUs;
        schedulerPrepared = true;
    }
    if (!schedulerPrepared)
        schedule.packetExtensionDurationUs = phy.getPacketExtensionDurationUs();
    if (phy.getGuardInterval() == physicallayer::HE_GI_3_2_US) {
        schedule.guardInterval = physicallayer::HE_GI_3_2_US; schedule.ltfType = physicallayer::HE_LTF_4X;
    }
    else {
        schedule.guardInterval = physicallayer::HE_GI_1_6_US;
        schedule.ltfType = std::any_of(schedule.allocations.begin(), schedule.allocations.end(),
                [] (const auto& allocation) { return allocation.muMimo; }) ? physicallayer::HE_LTF_1X : physicallayer::HE_LTF_2X;
    }
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        schedule.guardInterval = physicallayer::HE_GI_3_2_US; schedule.ltfType = physicallayer::HE_LTF_4X;
        schedule.coding = physicallayer::HE_CODING_BCC; schedule.packetExtensionDurationUs = 0;
    }
    for (auto& allocation : schedule.allocations)
        if (allocation.randomAccess)
            allocation.coding = physicallayer::HE_CODING_BCC;
    schedule.apTxPowerDbm = std::clamp((int)std::lround(math::mW2dBmW(phy.getMaximumTransmitPower().get<mW>()) -
            10 * std::log10(channelBandwidth.get() / 20e6)), -20, 40);
    if (schedule.allocations.empty() && triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER)
        return std::nullopt;
    physicallayer::Ieee80211HeTriggerResponseFinalizationResult phyFinalization;
    auto finalization = finalizeSchedule(schedule, centerFrequency, channelBandwidth, triggerType, &phyFinalization);
    if (!finalization)
        return std::nullopt;
    HeUlMuPlan::ValidationContext validationContext;
    validationContext.centerFrequency = centerFrequency;
    validationContext.requireSchedulerCandidate = schedulerPrepared;
    for (const auto& peer : snapshot.peers) {
        HeUlMuPlan::StationContract contract;
        contract.station = peer.stationAddress; contract.associationId = peer.associationId;
        if (peer.negotiatedCapabilities) contract.capabilities = *peer.negotiatedCapabilities;
        contract.schedulerCandidate = !schedulerPrepared || std::any_of(schedulerContext.candidates.begin(), schedulerContext.candidates.end(),
                [&] (const auto& candidate) { return candidate.staAddress == peer.stationAddress; });
        contract.ulMuDisabled = peer.ulMuDisabled;
        validationContext.stations.push_back(contract);
    }
    HeUlMuPlanDiagnostic diagnostic;
    auto plan = HeUlMuPlan::create(validationContext, finalization.schedule, triggerType, phyFinalization, diagnostic);
    if (!plan) {
        EV_WARN << "HE UL plan rejected before commit: " << diagnostic.detail << "\n";
        return std::nullopt;
    }
    return PreparedStart(accessCategory, triggerType, *plan,
            schedulerPrepared ?
                    std::optional<IIeee80211HeUlScheduler::ScheduleContext>(schedulerContext) :
                    std::nullopt);
}

bool HeUlTriggerService::commitStart(const PreparedStart& preparedStart)
{
    if (!hasPendingTrigger() || pendingTrigger != preparedStart.triggerType ||
            !coordinator->isEnabled())
        return false;
    accessRequested = false;
    committedScheduleContext = preparedStart.scheduleContext;
    try {
        actions->configureHeUlMuProtection(preparedStart.accessCategory);
        actions->startHeUlMuExchange(preparedStart.accessCategory,
                preparedStart.plan, exchangeCallback);
    }
    catch (...) {
        committedScheduleContext.reset();
        throw;
    }
    planStarted();
    return true;
}

bool HeUlTriggerService::tryStart(AccessCategory accessCategory,
        const HeUlPreparationSnapshot& snapshot)
{
    auto preparedStart = prepareStart(accessCategory, snapshot);
    return preparedStart.has_value() && commitStart(*preparedStart);
}

void HeUlTriggerService::planStarted()
{
    pendingTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
}

const char *HeUlTriggerService::getPendingTriggerName() const
{
    switch (pendingTrigger) {
        case IIeee80211HeUlTriggerPolicy::NO_TRIGGER: return "NO_TRIGGER";
        case IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER: return "BASIC_TRIGGER";
        case IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER: return "BSRP_TRIGGER";
        case IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER: return "NFRP_TRIGGER";
        default: return "UNKNOWN";
    }
}

uint32_t HeUlTriggerService::allocateTriggerId()
{
    return coordinator->allocateTriggerId();
}

void HeUlTriggerService::planCommitted(const HeUlMuPlan& plan, uint32_t triggerId)
{
    if (plan.getTriggerType() == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER) {
        if (!committedScheduleContext)
            throw cRuntimeError("HE UL Basic Trigger committed without its validated scheduler context");
        coordinator->commitSchedule(*committedScheduleContext, plan.getSchedule());
        committedScheduleContext.reset();
    }
    coordinator->noteTriggerSent(plan.getTriggerType(), triggerId);
}

} // namespace ieee80211
} // namespace inet
