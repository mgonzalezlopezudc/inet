//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeCoordinator.h"

#include <algorithm>
#include <limits>
#include <set>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

HeDlMuExchangeCoordinator::ReservationRollbackGuard::ReservationRollbackGuard(
        HeDlMuExchangeCoordinator& provider, uint64_t transactionToken) :
    provider(&provider), transactionToken(transactionToken)
{
}

HeDlMuExchangeCoordinator::ReservationRollbackGuard::~ReservationRollbackGuard()
{
    if (provider != nullptr)
        provider->rollbackReservation(transactionToken);
}

void HeDlMuExchangeCoordinator::configure(IActions *actions,
        HeSoundingService *soundingProvider)
{
    if (actions == nullptr || soundingProvider == nullptr)
        throw cRuntimeError("HE DL MU coordinator requires typed actions and a sounding service");
    this->actions = actions;
    this->soundingProvider = soundingProvider;
}

void HeDlMuExchangeCoordinator::restorePendingProtection()
{
    if (!pendingProtectionAccessCategory || !pendingProtectionSnapshot)
        return;
    actions->restoreHeDlMuProtection(*pendingProtectionAccessCategory,
            *pendingProtectionSnapshot);
    pendingProtectionAccessCategory.reset();
    pendingProtectionSnapshot.reset();
}

IIeee80211HeDlScheduler::ScheduleContext HeDlMuExchangeCoordinator::buildScheduleContext(
        const HeDlMuPreparationSnapshot& snapshot)
{
    auto context = snapshot.common;
    context.candidates.clear();
    context.anchorSta = MacAddress();
    std::set<MacAddress> seen;
    for (const auto& packet : snapshot.packets) {
        if (!packet.unicast || !packet.qosData || !packet.twtEligible ||
                !packet.activeBlockAck ||
                (packet.sequenceNumberValid && !packet.retryEligible) ||
                seen.count(packet.peer) != 0)
            continue;
        int availableSlots = std::max(0,
                packet.blockAckBufferSize - packet.occupiedBlockAckSlots);
        if (availableSlots == 0)
            continue;
        const auto *negotiated = packet.hasNegotiatedCapabilities ?
                &packet.negotiatedCapabilities : nullptr;
        if (negotiated != nullptr &&
                (!negotiated->localTxPeerRx.valid ||
                 !negotiated->localTxPeerRx.ofdma ||
                 negotiated->localTxPeerRx.supportedChannelWidths.count(
                         context.channelBandwidth) == 0 ||
                 (!snapshot.sequentialBar &&
                  (!negotiated->localTxPeerRx.receiverCanReceiveMuBarTrigger ||
                   !negotiated->localRxPeerTx.transmitterCanTransmitHeTbBlockAck))))
            continue;
        if (!context.puncturedSubchannels.empty() &&
                (negotiated == nullptr ||
                 !negotiated->localTxPeerRx.preamblePuncturing))
            continue;
        seen.insert(packet.peer);
        IIeee80211HeDlScheduler::CandidateInfo candidate;
        candidate.staAddress = packet.peer;
        candidate.accessCategory = packet.accessCategory;
        candidate.tid = packet.tid;
        candidate.holPacketBytes = 4 + packet.packetBytes;
        candidate.holEnqueueTime = packet.enqueueTime;
        candidate.holDelay = snapshot.now - packet.enqueueTime;
        candidate.sourceQueueToken = packet.queueToken;
        candidate.hasAdvertisedHeCapabilities = packet.hasAdvertisement;
        candidate.advertisedHeCapabilities = packet.advertisement;
        candidate.hasNegotiatedHeCapabilities = packet.hasNegotiatedCapabilities;
        candidate.negotiatedHeCapabilities = packet.negotiatedCapabilities;
        candidate.operatingModeRxNss = packet.operatingModeRxNss;
        candidate.hasFreshCsi = packet.hasFreshCsi;
        candidate.pathLossDb = packet.pathLossDb;
        candidate.hasFreshPathLoss = packet.hasFreshPathLoss;
        int eligiblePackets = 0;
        for (const auto& queued : snapshot.packets) {
            if (queued.peer != packet.peer || queued.tid != packet.tid ||
                    !(queued.queueToken == packet.queueToken) ||
                    !queued.qosData || !queued.activeBlockAck ||
                    (queued.sequenceNumberValid && !queued.retryEligible) ||
                    eligiblePackets >= std::min(availableSlots,
                            context.maxAmpduMpduCount))
                continue;
            int64_t subframeLength = 4 + queued.packetBytes;
            candidate.backlogBytes += subframeLength;
            if (eligiblePackets++ > 0)
                candidate.backlogBytes += (4 - subframeLength % 4) % 4;
            candidate.eligiblePacketIdentities.push_back(queued.packetIdentity);
        }
        context.candidates.push_back(candidate);
    }
    std::stable_sort(context.candidates.begin(), context.candidates.end(),
            [] (const auto& a, const auto& b) {
                return a.holEnqueueTime < b.holEnqueueTime;
            });
    if (!context.candidates.empty()) {
        context.candidates.front().anchor = true;
        context.anchorSta = context.candidates.front().staAddress;
    }
    context.coding = snapshot.localLdpc &&
            std::all_of(context.candidates.begin(), context.candidates.end(),
                    [] (const auto& candidate) {
                        return candidate.hasNegotiatedHeCapabilities &&
                                candidate.negotiatedHeCapabilities.localTxPeerRx.valid &&
                                candidate.negotiatedHeCapabilities.mutual.ldpc;
                    }) ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
    return context;
}

HcfQueueToken HeDlMuExchangeCoordinator::selectOldestEligibleQueue(
        const HeDlMuPreparationSnapshot& snapshot)
{
    const HeDlMuCandidateSnapshot *oldest = nullptr;
    for (const auto& packet : snapshot.packets)
        if (!packet.queuePeer.isUnspecified() && packet.queueIndex == 0 &&
                packet.twtEligible && !packet.addbaRequestInProgress &&
                (oldest == nullptr || packet.enqueueTime < oldest->enqueueTime))
            oldest = &packet;
    return oldest == nullptr ? HcfQueueToken() : oldest->queueToken;
}

IIeee80211HeDlScheduler::ScheduleContext HeDlMuExchangeCoordinator::buildCandidateContext(
        const IIeee80211HeDlScheduler::ScheduleContext& snapshotContext)
{
    auto result = snapshotContext;
    result.candidates.erase(std::remove_if(result.candidates.begin(), result.candidates.end(),
            [] (const auto& candidate) {
                return candidate.staAddress.isUnspecified() ||
                        !candidate.sourceQueueToken.isValid() ||
                        !candidate.hasNegotiatedHeCapabilities ||
                        !candidate.negotiatedHeCapabilities.localTxPeerRx.valid;
            }), result.candidates.end());
    std::stable_sort(result.candidates.begin(), result.candidates.end(),
            [] (const auto& left, const auto& right) {
                if (left.holEnqueueTime != right.holEnqueueTime)
                    return left.holEnqueueTime < right.holEnqueueTime;
                return left.staAddress < right.staAddress;
            });
    for (auto& candidate : result.candidates)
        candidate.anchor = false;
    result.anchorSta = MacAddress();
    if (!result.candidates.empty()) {
        result.candidates.front().anchor = true;
        result.anchorSta = result.candidates.front().staAddress;
    }
    return result;
}

HeDlMuExchangeCoordinator::PreparationResult HeDlMuExchangeCoordinator::preparePlan(
        const IIeee80211HeDlScheduler::ScheduleContext& snapshotContext,
        IIeee80211HeDlScheduler& scheduler)
{
    PreparationResult result;
    auto context = buildCandidateContext(snapshotContext);
    if (context.candidates.size() < 2)
        return result;
    auto allocations = scheduler.schedule(context);
    result.plan = HeDlMuPlan::create(context, allocations, result.diagnostic);
    return result;
}

HeDlMuExchangeCoordinator::FallbackCandidate
HeDlMuExchangeCoordinator::selectFallbackCandidate(
        const HeDlMuPreparationSnapshot& snapshot)
{
    const HeDlMuCandidateSnapshot *oldest = nullptr;
    for (const auto& packet : snapshot.packets)
        if (!packet.queuePeer.isUnspecified() && packet.queueIndex == 0 &&
                packet.twtEligible && !packet.addbaRequestInProgress &&
                (oldest == nullptr || packet.enqueueTime < oldest->enqueueTime ||
                 (packet.enqueueTime == oldest->enqueueTime &&
                  packet.packetIdentity.getValue() < oldest->packetIdentity.getValue())))
            oldest = &packet;
    return oldest == nullptr ? FallbackCandidate() :
            FallbackCandidate {oldest->queueToken, oldest->packetIdentity};
}

std::optional<HeDlMuExchangeCoordinator::PreparedStart>
HeDlMuExchangeCoordinator::prepareSingleUserStart(AccessCategory ac,
        const HeDlMuPreparationSnapshot& snapshot) const
{
    if (snapshot.accessCategory != ac)
        throw cRuntimeError("HE DL MU single-user snapshot access category mismatch");
    if (snapshot.hasSingleUserFrameToTransmit)
        return PreparedStart {StartKind::SINGLE_USER_FALLBACK, ac};
    const auto fallbackCandidate = selectFallbackCandidate(snapshot);
    if (!fallbackCandidate.queueToken.isValid())
        return std::nullopt;
    PreparedStart prepared {StartKind::SINGLE_USER_FALLBACK, ac};
    prepared.stageQueueToken = fallbackCandidate.queueToken;
    prepared.stagePacketIdentity = fallbackCandidate.packetIdentity;
    return prepared;
}

std::optional<HeDlMuExchangeCoordinator::PreparedStart> HeDlMuExchangeCoordinator::prepareStart(AccessCategory ac,
        const HeDlMuPreparationSnapshot& snapshot,
        IIeee80211HeDlScheduler& scheduler,
        const StartupParameters& parameters)
{
    if (actions == nullptr)
        throw cRuntimeError("HE DL MU coordinator is not configured");
    if (snapshot.accessCategory != ac)
        throw cRuntimeError("HE DL MU snapshot access category mismatch");

    auto context = buildScheduleContext(snapshot);
    const auto fallbackCandidate = selectFallbackCandidate(snapshot);

    if (snapshot.heAccessPoint && snapshot.common.enableDlMuMimo &&
            snapshot.localDlMuMimoBeamformer) {
        HcfHeSoundingSnapshot soundingSnapshot;
        soundingSnapshot.accessCategory = ac;
        soundingSnapshot.channelCenterFrequency = context.channelCenterFrequency;
        soundingSnapshot.channelBandwidth = context.channelBandwidth;
        for (const auto& candidate : context.candidates) {
            HcfHeSoundingCandidateSnapshot soundingCandidate;
            soundingCandidate.address = candidate.staAddress;
            soundingCandidate.associationId = actions->getHeDlMuAssociationId(
                    candidate.staAddress);
            soundingCandidate.eligible = candidate.hasNegotiatedHeCapabilities &&
                    candidate.negotiatedHeCapabilities.localTxPeerRx.valid &&
                    candidate.hasAdvertisedHeCapabilities &&
                    candidate.advertisedHeCapabilities.dlMuMimoBeamformee;
            soundingCandidate.maximumSpatialStreams = soundingCandidate.eligible ?
                    std::min(getMaxNss(candidate.negotiatedHeCapabilities.
                            localTxPeerRx.mcsNss), 4) : 1;
            soundingCandidate.hasFreshCsi = candidate.hasFreshCsi;
            soundingSnapshot.candidates.push_back(soundingCandidate);
        }
        auto soundingAction = soundingProvider->prepareSounding(soundingSnapshot);
        if (soundingAction)
            return PreparedStart {StartKind::HE_SOUNDING, ac, soundingAction};
    }

    if (snapshot.hasRecoveryFrame)
        return PreparedStart {StartKind::RECOVERY_SINGLE_USER, ac};
    if (!snapshot.pendingQueueEmpty && !snapshot.pendingHeadMuEligible)
        return snapshot.hasSingleUserFrameToTransmit ?
                std::optional<PreparedStart>(PreparedStart {StartKind::SINGLE_USER_FALLBACK, ac}) :
                std::nullopt;

    if (snapshot.pendingQueueEmpty) {
        const HeDlMuCandidateSnapshot *bootstrap = nullptr;
        for (const auto& packet : snapshot.packets) {
            if (packet.queuePeer.isUnspecified() || packet.queueIndex != 0 ||
                    !packet.unicast || !packet.qosData || !packet.twtEligible ||
                    !packet.addbaRequired || packet.activeBlockAck ||
                    packet.addbaRequestInProgress)
                continue;
            if (bootstrap == nullptr || packet.enqueueTime < bootstrap->enqueueTime)
                bootstrap = &packet;
        }
        if (bootstrap != nullptr) {
            PreparedStart prepared {StartKind::ADDBA_SINGLE_USER, ac};
            prepared.stageQueueToken = bootstrap->queueToken;
            prepared.stagePacketIdentity = bootstrap->packetIdentity;
            return prepared;
        }
    }

    auto preparation = preparePlan(context, scheduler);
    if (!preparation.plan) {
        if (context.candidates.size() < 2 &&
                !(snapshot.pendingQueueEmpty && fallbackCandidate.queueToken.isValid() &&
                  fallbackCandidate.packetIdentity.isValid()))
            return std::nullopt;
        PreparedStart prepared {StartKind::SINGLE_USER_FALLBACK, ac};
        if (snapshot.pendingQueueEmpty) {
            prepared.stageQueueToken = fallbackCandidate.queueToken;
            prepared.stagePacketIdentity = fallbackCandidate.packetIdentity;
        }
        prepared.startSingleUser = context.candidates.size() >= 2;
        return prepared;
    }

    HeDlMuTxOpFs::validateConfiguration(parameters.maxAmpduMpduCount,
            parameters.maxHeMuPsduLength, parameters.maxHeMuPpduDuration);
    auto ackMethod = snapshot.sequentialBar ?
            HeDlMuTxOpFs::AckMethod::EXPLICIT_SEQUENTIAL_BAR :
            HeDlMuTxOpFs::AckMethod::MU_BAR_TRIGGER;
    if (ackMethod == HeDlMuTxOpFs::AckMethod::MU_BAR_TRIGGER) {
        const auto fullBandwidthRu = physicallayer::getHeEqualRuLayout(
                context.channelCenterFrequency, context.channelBandwidth, 1).front();
        const auto fullBandwidthUlMuMimo = std::count_if(
                preparation.plan->getAllocations().begin(),
                preparation.plan->getAllocations().end(), [&] (const auto& allocation) {
                    return allocation.ru.toneSize == fullBandwidthRu.toneSize &&
                            allocation.ru.toneOffset == fullBandwidthRu.toneOffset;
                }) >= 2;
        if (fullBandwidthUlMuMimo)
            for (const auto& allocation : preparation.plan->getAllocations()) {
                auto candidate = std::find_if(context.candidates.begin(), context.candidates.end(),
                        [&] (const auto& entry) { return entry.staAddress == allocation.staAddress; });
                if (candidate == context.candidates.end() ||
                        !candidate->hasNegotiatedHeCapabilities ||
                        !candidate->negotiatedHeCapabilities.localRxPeerTx.valid ||
                        !candidate->negotiatedHeCapabilities.localRxPeerTx.fullBandwidthUlMuMimo) {
                    ackMethod = HeDlMuTxOpFs::AckMethod::EXPLICIT_SEQUENTIAL_BAR;
                    break;
                }
            }
    }

    PreparedStart prepared {StartKind::HE_DL_MULTIUSER, ac};
    prepared.plan = std::move(preparation.plan);
    prepared.ackMethod = ackMethod;
    prepared.parameters = parameters;
    prepared.scheduler = &scheduler;
    if (snapshot.pendingQueueEmpty) {
        prepared.stageQueueToken = fallbackCandidate.queueToken;
        prepared.stagePacketIdentity = fallbackCandidate.packetIdentity;
    }
    return prepared;
}

bool HeDlMuExchangeCoordinator::commitStart(const PreparedStart& preparedStart)
{
    const auto ac = preparedStart.accessCategory;
    if (preparedStart.kind == StartKind::HE_SOUNDING) {
        if (!preparedStart.soundingAction)
            throw cRuntimeError("Prepared HE sounding start lacks an action");
        soundingProvider->commitPreparedSounding(*preparedStart.soundingAction, ac);
        return true;
    }
    if (preparedStart.kind != StartKind::HE_DL_MULTIUSER) {
        if (preparedStart.stageQueueToken.isValid() &&
                preparedStart.stagePacketIdentity.isValid() &&
                !actions->stageHeDlMuPacket(preparedStart.stageQueueToken,
                        preparedStart.stagePacketIdentity, ac))
            return false;
        return preparedStart.startSingleUser &&
                actions->startHeDlMuSingleUserIfEligible(ac);
    }
    if (!preparedStart.plan)
        throw cRuntimeError("Prepared HE DL MU start lacks a plan");
    if (nextTransactionToken == 0 ||
            nextTransactionToken == std::numeric_limits<uint64_t>::max())
        throw cRuntimeError("HE DL MU exchange ID exhausted");
    auto token = nextTransactionToken++;
    if (!reservePlan(*preparedStart.plan, token)) {
        return false;
    }
    ReservationRollbackGuard rollbackGuard(*this, token);
    if (preparedStart.scheduler == nullptr)
        throw cRuntimeError("Prepared HE DL MU start lacks its scheduler commit seam");
    pendingScheduler = preparedStart.scheduler;
    pendingScheduleContext = preparedStart.plan->getScheduleContext();
    pendingAllocations = preparedStart.plan->getAllocations();
    pendingProtectionAccessCategory = ac;
    pendingProtectionSnapshot = actions->captureHeDlMuProtection(ac);
    bool started = false;
    try {
        started = actions->startHeDlMuExchange(ac, *preparedStart.plan, token,
                preparedStart.ackMethod, preparedStart.parameters, this, this);
    }
    catch (...) {
        restorePendingProtection();
        throw;
    }
    if (!started) {
        rollbackReservation(token);
        const bool staged = preparedStart.stageQueueToken.isValid() &&
                preparedStart.stagePacketIdentity.isValid() &&
                actions->stageHeDlMuPacket(preparedStart.stageQueueToken,
                        preparedStart.stagePacketIdentity, ac);
        if (!staged)
            return false;
        return actions->startHeDlMuSingleUserIfEligible(ac);
    }
    if (startPhase == StartPhase::IDLE)
        startPhase = StartPhase::ACTIVE;
    rollbackGuard.release();
    return true;
}

bool HeDlMuExchangeCoordinator::tryStart(AccessCategory ac,
        const HeDlMuPreparationSnapshot& snapshot,
        IIeee80211HeDlScheduler& scheduler,
        const StartupParameters& parameters)
{
    auto preparedStart = prepareStart(ac, snapshot, scheduler, parameters);
    return preparedStart.has_value() && commitStart(*preparedStart);
}

bool HeDlMuExchangeCoordinator::hasForcedSingleUser(AccessCategory ac) const
{
    return ac >= AC_BK && ac < AC_NUMCATEGORIES && forceNextSingleUser[ac];
}

bool HeDlMuExchangeCoordinator::consumeForcedSingleUser(AccessCategory ac)
{
    if (!hasForcedSingleUser(ac))
        return false;
    forceNextSingleUser[ac] = false;
    return true;
}

bool HeDlMuExchangeCoordinator::reservePlan(const HeDlMuPlan& plan, uint64_t token)
{
    if (token == 0 || pendingExchangeId != 0 || activeExchange != nullptr ||
            !reservedPackets.empty()) {
        return false;
    }
    std::map<MacAddress, std::vector<Packet *>> prepared;
    const auto& context = plan.getScheduleContext();
    for (const auto& allocation : plan.getAllocations()) {
        auto candidate = std::find_if(context.candidates.begin(), context.candidates.end(),
                [&] (const auto& entry) { return entry.staAddress == allocation.staAddress; });
        if (candidate == context.candidates.end()) {
            return false;
        }
        auto queue = resolveHeDlMuQueue(candidate->sourceQueueToken);
        if (queue == nullptr || candidate->eligiblePacketIdentities.empty()) {
            return false;
        }
        for (const auto& identity : candidate->eligiblePacketIdentities) {
            Packet *matched = nullptr;
            for (int i = 0; i < queue->getNumPackets(); ++i) {
                auto current = queue->getPacket(i);
                if (HcfPacketIdentity(current->getId()) != identity)
                    continue;
                auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                        current->peekAtFront<Ieee80211MacHeader>());
                if (header == nullptr || header->getType() != ST_DATA_WITH_QOS ||
                        header->getReceiverAddress() != allocation.staAddress ||
                        header->getTid() != candidate->tid) {
                    return false;
                }
                matched = current;
                break;
            }
            if (matched == nullptr) {
                return false;
            }
            prepared[allocation.staAddress].push_back(matched);
        }
        if (prepared[allocation.staAddress].empty()) {
            return false;
        }
    }
    if (prepared.size() < 2) {
        return false;
    }
    pendingExchangeId = token;
    reservedPackets = std::move(prepared);
    return true;
}

void HeDlMuExchangeCoordinator::rollbackReservation(uint64_t token)
{
    if (token != pendingExchangeId)
        return;
    restorePendingProtection();
    pendingExchangeId = 0;
    startPhase = StartPhase::IDLE;
    reservedPackets.clear();
    pendingScheduler = nullptr;
    pendingScheduleContext = {};
    pendingAllocations.clear();
}

void HeDlMuExchangeCoordinator::abortActiveExchange()
{
    activeExchange.reset();
}

void HeDlMuExchangeCoordinator::shutdown()
{
    if (pendingExchangeId != 0)
        rollbackReservation(pendingExchangeId);
    reservedPackets.clear();
    pendingScheduler = nullptr;
    pendingScheduleContext = {};
    pendingAllocations.clear();
    pendingProtectionAccessCategory.reset();
    pendingProtectionSnapshot.reset();
    pendingExchangeId = 0;
    startPhase = StartPhase::IDLE;
    activeExchange.reset();
    actions = nullptr;
    soundingProvider = nullptr;
}

void HeDlMuExchangeCoordinator::finalizeReservation(uint64_t token,
        const std::vector<HeDlMuMember>& finalizedMembers)
{
    if (token == 0 || token != pendingExchangeId || finalizedMembers.empty())
        throw cRuntimeError("Invalid HE DL MU reservation finalization");
    std::map<MacAddress, std::vector<Packet *>> finalizedPackets;
    std::set<HcfPacketIdentity> identities;
    for (const auto& member : finalizedMembers) {
        auto reserved = reservedPackets.find(member.peer);
        if (member.packet == nullptr || !member.packetIdentity.isValid() ||
                member.packetIdentity != HcfPacketIdentity(member.packet->getId()) ||
                !identities.insert(member.packetIdentity).second ||
                reserved == reservedPackets.end() ||
                std::find(reserved->second.begin(), reserved->second.end(),
                        member.packet) == reserved->second.end())
            throw cRuntimeError("HE DL MU finalization contains an invalid or duplicate reserved MPDU identity");
        finalizedPackets[member.peer].push_back(member.packet);
    }
    if (finalizedPackets.size() < 2)
        throw cRuntimeError("HE DL MU finalization contains fewer than two users");
    std::vector<IIeee80211HeDlScheduler::RuAllocation> finalizedAllocations;
    for (const auto& allocation : pendingAllocations)
        if (finalizedPackets.find(allocation.staAddress) != finalizedPackets.end())
            finalizedAllocations.push_back(allocation);
    if (pendingScheduler != nullptr && finalizedAllocations.size() != finalizedPackets.size())
        throw cRuntimeError("HE DL MU finalization does not match the prepared scheduler plan");
    if (pendingScheduler != nullptr)
        pendingScheduler->commitSchedule(pendingScheduleContext, finalizedAllocations);
    reservedPackets = std::move(finalizedPackets);
    pendingScheduler = nullptr;
    pendingScheduleContext = {};
    pendingAllocations.clear();
}

bool HeDlMuExchangeCoordinator::isActiveContainer(const Packet *packet) const
{
    return activeExchange != nullptr && activeExchange->isContainer(packet);
}

bool HeDlMuExchangeCoordinator::routeTransmittedContainer(Packet *packet,
        bool notifyActions)
{
    if (!isActiveContainer(packet))
        return false;
    if (notifyActions)
        for (const auto& member : activeExchange->getMembers())
            heDlMuMemberTransmitted(activeExchange->getId(), member);
    return true;
}

queueing::IPacketQueue *HeDlMuExchangeCoordinator::resolveHeDlMuQueue(HcfQueueToken token) const { return actions->resolveHeDlMuQueue(token); }
Packet *HeDlMuExchangeCoordinator::getReservedHeDlMuPacket(uint64_t token, const MacAddress& peer) const
{
    if (token != pendingExchangeId)
        return nullptr;
    auto it = reservedPackets.find(peer);
    return it == reservedPackets.end() || it->second.empty() ? nullptr : it->second.front();
}
bool HeDlMuExchangeCoordinator::isReservedHeDlMuPacket(uint64_t token,
        const MacAddress& peer, const Packet *packet) const
{
    if (token != pendingExchangeId || packet == nullptr)
        return false;
    auto it = reservedPackets.find(peer);
    return it != reservedPackets.end() &&
            std::find(it->second.begin(), it->second.end(), packet) != it->second.end();
}
IOriginatorBlockAckAgreementHandler *HeDlMuExchangeCoordinator::getHeDlMuBlockAckHandler() const { return actions->getHeDlMuBlockAckHandler(); }
IOriginatorMacDataService *HeDlMuExchangeCoordinator::getHeDlMuOriginatorDataService() const { return actions->getHeDlMuOriginatorDataService(); }
IQosRateSelection *HeDlMuExchangeCoordinator::getHeDlMuRateSelection() const { return actions->getHeDlMuRateSelection(); }
MacAddress HeDlMuExchangeCoordinator::getHeDlMuTransmitterAddress() const { return actions->getHeDlMuTransmitterAddress(); }
int HeDlMuExchangeCoordinator::getHeDlMuFcsMode() const { return actions->getHeDlMuFcsMode(); }
uint8_t HeDlMuExchangeCoordinator::getHeDlMuBssColor() const { return actions->getHeDlMuBssColor(); }
uint16_t HeDlMuExchangeCoordinator::getHeDlMuAssociationId(const MacAddress& peer) const { return actions->getHeDlMuAssociationId(peer); }
std::optional<Ieee80211NegotiatedHeCapabilities> HeDlMuExchangeCoordinator::getHeDlMuNegotiatedCapabilities(const MacAddress& peer) const { return actions->getHeDlMuNegotiatedCapabilities(peer); }

void HeDlMuExchangeCoordinator::heDlMuPlanFinalized(HeDlMuExchangeId id,
        const std::vector<HeDlMuMember>& members)
{
    finalizeReservation(id, members);
}

void HeDlMuExchangeCoordinator::heDlMuPlanCommitted(uint64_t token,
        Packet *container, const std::vector<HeDlMuMember>& committedMembers)
{
    if (token == 0 || token != pendingExchangeId || activeExchange != nullptr)
        throw cRuntimeError("Invalid or overlapping HE DL MU commit");
    std::set<HcfPacketIdentity> identities;
    std::set<MacAddress> committedPeers;
    for (const auto& member : committedMembers) {
        auto reserved = reservedPackets.find(member.peer);
        if (member.packet == nullptr || !member.packetIdentity.isValid() ||
                member.packetIdentity != HcfPacketIdentity(member.packet->getId()) ||
                !identities.insert(member.packetIdentity).second ||
                reserved == reservedPackets.end() ||
                std::find(reserved->second.begin(), reserved->second.end(),
                        member.packet) == reserved->second.end())
            throw cRuntimeError("HE DL MU commit contains an invalid or duplicate MPDU identity");
        committedPeers.insert(member.peer);
    }
    if (committedPeers.size() != reservedPackets.size())
        throw cRuntimeError("HE DL MU commit does not match the reserved user set");
    pendingProtectionAccessCategory.reset();
    pendingProtectionSnapshot.reset();
    startPhase = StartPhase::ACTIVE;
    activeExchange = std::make_unique<HeDlMuExchange>(token, container,
            std::vector<HeDlMuMember>(committedMembers));
    pendingExchangeId = 0;
    reservedPackets.clear();
}

bool HeDlMuExchangeCoordinator::heDlMuMemberTransmitted(uint64_t token,
        const HeDlMuMember& member, bool notifyActions)
{
    if (activeExchange == nullptr || activeExchange->getId() != token ||
            !activeExchange->recordMemberTransmitted(member))
        return false;
    if (notifyActions)
        actions->notifyHeDlMuMemberTransmitted(token, member);
    return true;
}

bool HeDlMuExchangeCoordinator::heDlMuUserOutcome(uint64_t token,
        const MacAddress& peer, HeDlMuUserOutcome outcome, bool notifyActions)
{
    if (activeExchange == nullptr || activeExchange->getId() != token ||
            !activeExchange->recordUserOutcome(peer))
        return false;
    if (notifyActions)
        actions->notifyHeDlMuUserOutcome(token, peer, outcome);
    if (activeExchange->isComplete()) {
        startPhase = StartPhase::IDLE;
        activeExchange.reset();
    }
    return true;
}

void HeDlMuExchangeCoordinator::heDlMuMemberTransmitted(
        HeDlMuExchangeId id, const HeDlMuMember& member)
{
    heDlMuMemberTransmitted(id, member, true);
}

void HeDlMuExchangeCoordinator::heDlMuUserOutcome(HeDlMuExchangeId id,
        const MacAddress& peer, HeDlMuUserOutcome outcome)
{
    heDlMuUserOutcome(id, peer, outcome, true);
}

} // namespace ieee80211
} // namespace inet
