//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h"

#include <algorithm>
#include <limits>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

HeDlMuExchangeProvider::ReservationRollbackGuard::ReservationRollbackGuard(
        HeDlMuExchangeProvider& provider, uint64_t transactionToken) :
    provider(&provider), transactionToken(transactionToken)
{
}

HeDlMuExchangeProvider::ReservationRollbackGuard::~ReservationRollbackGuard()
{
    if (provider != nullptr)
        provider->rollbackReservation(transactionToken);
}

void HeDlMuExchangeProvider::configure(IActions *actions,
        HeSoundingService *soundingProvider)
{
    if (actions == nullptr || soundingProvider == nullptr)
        throw cRuntimeError("HE DL MU provider requires typed actions and sounding provider");
    this->actions = actions;
    this->soundingProvider = soundingProvider;
}

void HeDlMuExchangeProvider::restorePendingProtection()
{
    if (!pendingProtectionAccessCategory || !pendingProtectionSnapshot)
        return;
    actions->restoreHeDlMuProtection(*pendingProtectionAccessCategory,
            *pendingProtectionSnapshot);
    pendingProtectionAccessCategory.reset();
    pendingProtectionSnapshot.reset();
}

IIeee80211HeDlScheduler::ScheduleContext HeDlMuExchangeProvider::buildScheduleContext(
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

HcfQueueToken HeDlMuExchangeProvider::selectOldestEligibleQueue(
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

IIeee80211HeDlScheduler::ScheduleContext HeDlMuExchangeProvider::buildCandidateContext(
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

HeDlMuExchangeProvider::PreparationResult HeDlMuExchangeProvider::preparePlan(
        const IIeee80211HeDlScheduler::ScheduleContext& snapshotContext,
        IIeee80211HeDlScheduler& scheduler)
{
    PreparationResult result;
    auto context = buildCandidateContext(snapshotContext);
    if (context.candidates.size() < 2) {
        result.state = PreparationState::SINGLE_USER_FALLBACK;
        return result;
    }
    auto allocations = scheduler.schedule(context);
    result.state = PreparationState::SCHEDULER_SELECTED;
    result.plan = HeDlMuPlan::create(context, allocations, result.diagnostic);
    result.state = result.plan ? PreparationState::PLAN_VALIDATED :
            PreparationState::SINGLE_USER_FALLBACK;
    return result;
}

std::optional<HeDlMuExchangeProvider::PreparedStart>
HeDlMuExchangeProvider::prepareSingleUserStart(AccessCategory ac,
        const HeDlMuPreparationSnapshot& snapshot) const
{
    if (snapshot.accessCategory != ac)
        throw cRuntimeError("HE DL MU single-user snapshot access category mismatch");
    if (snapshot.hasSingleUserFrameToTransmit)
        return PreparedStart {StartKind::SINGLE_USER_FALLBACK, ac};
    const HeDlMuCandidateSnapshot *oldest = nullptr;
    for (const auto& packet : snapshot.packets)
        if (!packet.queuePeer.isUnspecified() && packet.queueIndex == 0 &&
                packet.twtEligible && !packet.addbaRequestInProgress &&
                (oldest == nullptr || packet.enqueueTime < oldest->enqueueTime ||
                 (packet.enqueueTime == oldest->enqueueTime &&
                  packet.packetIdentity.getValue() < oldest->packetIdentity.getValue())))
            oldest = &packet;
    if (oldest == nullptr)
        return std::nullopt;
    PreparedStart prepared {StartKind::SINGLE_USER_FALLBACK, ac};
    prepared.stageQueueToken = oldest->queueToken;
    prepared.stagePacketIdentity = oldest->packetIdentity;
    return prepared;
}

std::optional<HeDlMuExchangeProvider::PreparedStart> HeDlMuExchangeProvider::prepareStart(AccessCategory ac,
        const HeDlMuPreparationSnapshot& snapshot,
        IIeee80211HeDlScheduler& scheduler,
        const StartupParameters& parameters)
{
    if (actions == nullptr)
        throw cRuntimeError("HE DL MU provider is not configured");
    if (snapshot.accessCategory != ac)
        throw cRuntimeError("HE DL MU snapshot access category mismatch");

    auto context = buildScheduleContext(snapshot);
    const HeDlMuCandidateSnapshot *oldest = nullptr;
    for (const auto& packet : snapshot.packets)
        if (!packet.queuePeer.isUnspecified() && packet.queueIndex == 0 &&
                packet.twtEligible && !packet.addbaRequestInProgress &&
                (oldest == nullptr || packet.enqueueTime < oldest->enqueueTime ||
                 (packet.enqueueTime == oldest->enqueueTime &&
                  packet.packetIdentity.getValue() < oldest->packetIdentity.getValue())))
            oldest = &packet;
    const auto fallbackQueueToken = oldest == nullptr ? HcfQueueToken() : oldest->queueToken;
    const auto fallbackPacketIdentity = oldest == nullptr ? HcfPacketIdentity() : oldest->packetIdentity;

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
                !(snapshot.pendingQueueEmpty && fallbackQueueToken.isValid() &&
                  fallbackPacketIdentity.isValid()))
            return std::nullopt;
        PreparedStart prepared {StartKind::SINGLE_USER_FALLBACK, ac};
        if (snapshot.pendingQueueEmpty) {
            prepared.stageQueueToken = fallbackQueueToken;
            prepared.stagePacketIdentity = fallbackPacketIdentity;
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
        prepared.stageQueueToken = fallbackQueueToken;
        prepared.stagePacketIdentity = fallbackPacketIdentity;
    }
    return prepared;
}

bool HeDlMuExchangeProvider::commitStart(const PreparedStart& preparedStart)
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
    fallbackQueueToken = preparedStart.stageQueueToken;
    fallbackPacketIdentity = preparedStart.stagePacketIdentity;
    fallbackAccessCategory = ac;
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
    startPhase = StartPhase::COMMITTING;
    pendingProtectionAccessCategory = ac;
    pendingProtectionSnapshot = actions->captureHeDlMuProtection(ac);
    try {
        actions->configureHeDlMuProtection(ac);
        actions->startHeDlMuExchange(ac, *preparedStart.plan, token,
                preparedStart.ackMethod, preparedStart.parameters);
    }
    catch (...) {
        restorePendingProtection();
        throw;
    }
    if (pendingPlanningFailure) {
        const auto failedAccessCategory = *pendingPlanningFailure;
        pendingPlanningFailure.reset();
        rollbackReservation(token);
        const bool staged = preparedStart.stageQueueToken.isValid() &&
                preparedStart.stagePacketIdentity.isValid() &&
                actions->stageHeDlMuPacket(preparedStart.stageQueueToken,
                        preparedStart.stagePacketIdentity, ac);
        if (!staged)
            return false;
        return actions->startHeDlMuSingleUserIfEligible(failedAccessCategory);
    }
    if (startPhase == StartPhase::COMMITTING)
        startPhase = StartPhase::ACTIVE;
    rollbackGuard.release();
    return true;
}

bool HeDlMuExchangeProvider::tryStart(AccessCategory ac,
        const HeDlMuPreparationSnapshot& snapshot,
        IIeee80211HeDlScheduler& scheduler,
        const StartupParameters& parameters)
{
    auto preparedStart = prepareStart(ac, snapshot, scheduler, parameters);
    return preparedStart.has_value() && commitStart(*preparedStart);
}

bool HeDlMuExchangeProvider::hasForcedSingleUser(AccessCategory ac) const
{
    return ac >= AC_BK && ac < AC_NUMCATEGORIES && forceNextSingleUser[ac];
}

bool HeDlMuExchangeProvider::consumeForcedSingleUser(AccessCategory ac)
{
    if (!hasForcedSingleUser(ac))
        return false;
    forceNextSingleUser[ac] = false;
    return true;
}

bool HeDlMuExchangeProvider::reservePlan(const HeDlMuPlan& plan, uint64_t token)
{
    if (token == 0 || activeTransactionToken != 0 || !reservedPackets.empty()) {
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
    activeTransactionToken = token;
    reservedPackets = std::move(prepared);
    return true;
}

void HeDlMuExchangeProvider::rollbackReservation(uint64_t token)
{
    if (token != activeTransactionToken)
        return;
    restorePendingProtection();
    activeTransactionToken = 0;
    startPhase = StartPhase::IDLE;
    pendingPlanningFailure.reset();
    reservedPackets.clear();
    pendingScheduler = nullptr;
    pendingScheduleContext = {};
    pendingAllocations.clear();
}

void HeDlMuExchangeProvider::finalizeReservation(uint64_t token,
        const std::vector<HeDlMuMember>& finalizedMembers)
{
    if (token == 0 || token != activeTransactionToken || finalizedMembers.empty())
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

bool HeDlMuExchangeProvider::isActiveContainer(const Packet *packet) const
{
    return packet != nullptr && packet == containerPacket && activeTransactionToken != 0;
}

bool HeDlMuExchangeProvider::routeTransmittedContainer(Packet *packet,
        bool notifyActions)
{
    if (!isActiveContainer(packet))
        return false;
    if (notifyActions)
        for (const auto& member : members)
            heDlMuMemberTransmitted(activeTransactionToken, member);
    return true;
}

queueing::IPacketQueue *HeDlMuExchangeProvider::resolveHeDlMuQueue(HcfQueueToken token) const { return actions->resolveHeDlMuQueue(token); }
Packet *HeDlMuExchangeProvider::getReservedHeDlMuPacket(uint64_t token, const MacAddress& peer) const
{
    if (token != activeTransactionToken)
        return nullptr;
    auto it = reservedPackets.find(peer);
    return it == reservedPackets.end() || it->second.empty() ? nullptr : it->second.front();
}
bool HeDlMuExchangeProvider::isReservedHeDlMuPacket(uint64_t token,
        const MacAddress& peer, const Packet *packet) const
{
    if (token != activeTransactionToken || packet == nullptr)
        return false;
    auto it = reservedPackets.find(peer);
    return it != reservedPackets.end() &&
            std::find(it->second.begin(), it->second.end(), packet) != it->second.end();
}
IOriginatorBlockAckAgreementHandler *HeDlMuExchangeProvider::getHeDlMuBlockAckHandler() const { return actions->getHeDlMuBlockAckHandler(); }
IOriginatorMacDataService *HeDlMuExchangeProvider::getHeDlMuOriginatorDataService() const { return actions->getHeDlMuOriginatorDataService(); }
IQosRateSelection *HeDlMuExchangeProvider::getHeDlMuRateSelection() const { return actions->getHeDlMuRateSelection(); }
MacAddress HeDlMuExchangeProvider::getHeDlMuTransmitterAddress() const { return actions->getHeDlMuTransmitterAddress(); }
int HeDlMuExchangeProvider::getHeDlMuFcsMode() const { return actions->getHeDlMuFcsMode(); }
uint8_t HeDlMuExchangeProvider::getHeDlMuBssColor() const { return actions->getHeDlMuBssColor(); }
uint16_t HeDlMuExchangeProvider::getHeDlMuAssociationId(const MacAddress& peer) const { return actions->getHeDlMuAssociationId(peer); }
std::optional<Ieee80211NegotiatedHeCapabilities> HeDlMuExchangeProvider::getHeDlMuNegotiatedCapabilities(const MacAddress& peer) const { return actions->getHeDlMuNegotiatedCapabilities(peer); }

void HeDlMuExchangeProvider::heDlMuPlanCommitted(uint64_t token,
        Packet *container, const std::vector<HeDlMuMember>& committedMembers)
{
    if (token == 0 || container == nullptr || committedMembers.empty() || activeTransactionToken != token)
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
    activeTransactionToken = token;
    startPhase = StartPhase::ACTIVE;
    containerPacket = container;
    members = committedMembers;
    transmittedMembers.clear();
    completedUsers.clear();
    reservedPackets.clear();
}

bool HeDlMuExchangeProvider::heDlMuMemberTransmitted(uint64_t token,
        const HeDlMuMember& member, bool notifyActions)
{
    auto committed = std::find_if(members.begin(), members.end(),
            [&] (const auto& entry) {
                return entry.packet == member.packet && entry.peer == member.peer &&
                        entry.packetIdentity == member.packetIdentity;
            });
    if (token != activeTransactionToken || committed == members.end() ||
            !transmittedMembers.insert(member.packet).second)
        return false;
    if (notifyActions)
        actions->heDlMuMemberTransmitted(token, *committed);
    return true;
}

bool HeDlMuExchangeProvider::heDlMuUserOutcome(uint64_t token,
        const MacAddress& peer, HeDlMuUserOutcome outcome, bool notifyActions)
{
    auto committed = std::find_if(members.begin(), members.end(),
            [&] (const auto& entry) { return entry.peer == peer; });
    if (token != activeTransactionToken || committed == members.end() ||
            !completedUsers.insert(peer).second)
        return false;
    if (notifyActions)
        actions->heDlMuUserOutcome(token, peer, outcome);
    std::set<MacAddress> committedUsers;
    for (const auto& member : members)
        committedUsers.insert(member.peer);
    if (completedUsers == committedUsers) {
        activeTransactionToken = 0;
        startPhase = StartPhase::IDLE;
        containerPacket = nullptr;
        members.clear();
        transmittedMembers.clear();
        completedUsers.clear();
    }
    return true;
}

bool HeDlMuExchangeProvider::heDlMuPlanningFailed(uint64_t token,
        AccessCategory ac, bool notifyActions)
{
    // Planning failure belongs only to the reserved, not-yet-committed
    // transaction.  A zero, late, duplicate, or post-commit callback must not
    // schedule a fallback for an unrelated TXOP.
    if (token == 0 || token != activeTransactionToken ||
            !members.empty() || reservedPackets.empty())
        return false;
    if (ac != fallbackAccessCategory)
        throw cRuntimeError("HE DL MU planning failure access category does not match the reserved start");
    if (startPhase == StartPhase::COMMITTING) {
        if (pendingPlanningFailure)
            throw cRuntimeError("Duplicate synchronous HE DL MU planning failure callback");
        pendingPlanningFailure = ac;
        return true;
    }
    rollbackReservation(token);
    const bool staged = ac == fallbackAccessCategory &&
            fallbackQueueToken.isValid() && fallbackPacketIdentity.isValid() &&
            actions->stageHeDlMuPacket(fallbackQueueToken,
                    fallbackPacketIdentity, ac);
    if (ac >= AC_BK && ac < AC_NUMCATEGORIES)
        forceNextSingleUser[ac] = staged;
    if (notifyActions)
        actions->heDlMuPlanningFailed(token, ac);
    return true;
}

} // namespace ieee80211
} // namespace inet
