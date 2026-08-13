//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.h"

#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211VhtDlMuScheduler.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtGroupIdManager.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtGroupIdManagementFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtSoundingFs.h"
#include "inet/linklayer/ieee80211/mac/queue/InProgressFrames.h"
#include "inet/linklayer/ieee80211/mac/queue/OrigEnqueueTimeTag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

namespace {

class RollbackGuard
{
  private:
    std::function<void()> rollback;

  public:
    explicit RollbackGuard(std::function<void()> rollback) : rollback(std::move(rollback)) {}
    RollbackGuard(const RollbackGuard&) = delete;
    RollbackGuard& operator=(const RollbackGuard&) = delete;
    ~RollbackGuard() { if (rollback) rollback(); }
    void release() { rollback = {}; }
};

class DefaultVhtDlMuTxOpFactory : public VhtHcfFeature::ITxOpFactory
{
  public:
    virtual VhtDlMuTxOpFs *create(const VhtDlMuPlan& plan,
            physicallayer::Ieee80211ModeSet *modeSet, IAckHandler *ackHandler,
            IFrameSequenceHandler::ICallback *callback,
            IVhtDlMuExchangeCallback *vhtCallback,
            uint64_t transactionToken) override
    {
        return new VhtDlMuTxOpFs(plan, modeSet, ackHandler, callback,
                vhtCallback, transactionToken);
    }
};

bool containsVhtFeedback(const Packet *packet)
{
    auto data = packet->peekData();
    if (dynamicPtrCast<const Ieee80211VhtCompressedBeamformingFeedback>(data))
        return true;
    if (auto sequence = dynamicPtrCast<const SequenceChunk>(data))
        for (const auto& chunk : sequence->getChunks())
            if (dynamicPtrCast<const Ieee80211VhtCompressedBeamformingFeedback>(chunk))
                return true;
    return false;
}

template <typename T>
Ptr<const T> findVhtActionBody(const Packet *packet)
{
    auto data = packet->peekData();
    if (auto chunk = dynamicPtrCast<const T>(data))
        return chunk;
    if (auto sequence = dynamicPtrCast<const SequenceChunk>(data))
        for (const auto& chunk : sequence->getChunks())
            if (auto result = dynamicPtrCast<const T>(chunk))
                return result;
    return nullptr;
}

bool isVhtNdp(const Packet *packet)
{
    auto request = packet == nullptr ? nullptr :
            packet->findTag<physicallayer::Ieee80211VhtTransmissionTag>();
    return packet != nullptr && packet->getDataLength() == B(0) &&
            request != nullptr && request->getNdp();
}

} // namespace

void VhtHcfFeature::validatePacketLevelRadio(cModule *radio)
{
    if (dynamic_cast<physicallayer::IIeee80211VhtPacketRadio *>(radio) == nullptr)
        throw cRuntimeError("VHT SU beamforming requires an Ieee80211 packet-level radio");
}

void VhtHcfFeature::configure(IActions *actions, bool enableSuBeamforming,
        bool enableDlMuMimo, uint8_t dlMuGroupId, double beamformingGainDb,
        simtime_t csiValidityDuration, IVhtSoundingCoordinator *coordinator,
        IVhtGroupIdManager *groupIdManager,
        IIeee80211VhtDlMuScheduler *dlMuScheduler,
        physicallayer::IIeee80211VhtPacketRadio *radio)
{
    if (actions == nullptr || groupIdManager == nullptr || dlMuScheduler == nullptr)
        throw cRuntimeError("Incomplete VHT HCF feature composition");
    this->actions = actions;
    this->enableSuBeamforming = enableSuBeamforming;
    this->enableDlMuMimo = enableDlMuMimo;
    this->dlMuGroupId = dlMuGroupId;
    this->beamformingGainDb = beamformingGainDb;
    this->groupIdManager = groupIdManager;
    this->dlMuScheduler = dlMuScheduler;
    this->radio = radio;
    soundingService.configure(csiValidityDuration, coordinator);
    if (radio != nullptr) {
        soundingService.updateChannelWidth(radio->getVhtChannelWidth());
        membershipAdapter.setRadio(radio);
    }
    groupIdManager->setLocalMembershipListener(&membershipAdapter);
    defaultTxOpFactory = std::make_unique<DefaultVhtDlMuTxOpFactory>();
    txOpFactory = defaultTxOpFactory.get();
}

void VhtHcfFeature::modeSetChanged()
{
    if (radio == nullptr)
        return;
    soundingService.updateChannelWidth(radio->getVhtChannelWidth());
    auto selection = radio->getVhtMuRxSelection();
    if (selection.active && selection.channelWidth != radio->getVhtChannelWidth())
        radio->setVhtMuRxSelection({});
}

bool VhtHcfFeature::isAssociatedPeer(const MacAddress& peer) const
{
    auto mib = actions->getMac()->getMib();
    return mib->getStationType() == Ieee80211Mib::ACCESS_POINT ?
            mib->isPeerAssociated(peer) : mib->isAssociated() && mib->getBssid() == peer;
}

uint16_t VhtHcfFeature::getPeerAssociationId(const MacAddress& peer) const
{
    auto mib = actions->getMac()->getMib();
    return mib->getStationType() == Ieee80211Mib::ACCESS_POINT ?
            mib->getAssociationId(peer) : mib->getLocalAssociationId();
}

bool VhtHcfFeature::isEligibleSu(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peer, int& soundingNsts) const
{
    soundingNsts = 0;
    auto modeSet = actions->getModeSet();
    if (!enableSuBeamforming || mode == nullptr || !modeSet->containsMode(mode) ||
            modeSet->getPhyFamily(mode) != physicallayer::Ieee80211PhyFamily::VHT ||
            peer.isMulticast() || !isAssociatedPeer(peer))
        return false;
    auto negotiated = actions->getMac()->getMib()->getNegotiatedVhtCapabilities(peer);
    if (!negotiated || !negotiated->localTxPeerRx.valid ||
            !negotiated->localTxPeerRx.suBeamforming ||
            negotiated->localTxPeerRx.soundingNsts < 2)
        return false;
    soundingNsts = negotiated->localTxPeerRx.soundingNsts;
    return true;
}

std::vector<MacAddress> VhtHcfFeature::getConstrainedMuPeers() const
{
    std::vector<std::pair<MacAddress, int>> capablePeers;
    auto mib = actions->getMac()->getMib();
    if (!enableDlMuMimo || radio == nullptr ||
            mib->getStationType() != Ieee80211Mib::ACCESS_POINT)
        return {};
    auto channelWidth = radio->getVhtChannelWidth();
    for (const auto& station : mib->getPeerAssociationSnapshots()) {
        auto negotiated = mib->getNegotiatedVhtCapabilities(station.getAddress());
        if (!station.hasMemberStatus() || station.getMemberStatus() != Ieee80211Mib::ASSOCIATED ||
                !negotiated || !negotiated->localTxPeerRx.valid ||
                !negotiated->localTxPeerRx.muMimo ||
                !negotiated->localTxPeerRx.supportedChannelWidths.count(channelWidth) ||
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[0] < 0 ||
                negotiated->localTxPeerRx.maxNstsTotal < 2 ||
                negotiated->localTxPeerRx.soundingNsts < 2)
            continue;
        capablePeers.emplace_back(station.getAddress(), negotiated->localTxPeerRx.soundingNsts);
    }
    std::sort(capablePeers.begin(), capablePeers.end());
    auto groupDimension = std::min<size_t>(4, radio->getVhtAntennaCount());
    if (groupDimension < 2)
        return {};
    for (size_t userCount = groupDimension; userCount >= 2; --userCount) {
        std::vector<MacAddress> peers;
        for (const auto& peer : capablePeers)
            if (peer.second >= static_cast<int>(userCount)) {
                peers.push_back(peer.first);
                if (peers.size() == userCount)
                    return peers;
            }
    }
    return {};
}

void VhtHcfFeature::installBlockAckPrerequisite(
        IOriginatorBlockAckAgreementHandler::PrerequisiteReservation& reservation,
        queueing::IPacketQueue *queue, InProgressFrames *inProgressFrames,
        const std::function<void(Packet *)>& reclaimOwnership)
{
    installAndStartBlockAckPrerequisite(reservation, queue, inProgressFrames,
            reclaimOwnership, [] {});
}

void VhtHcfFeature::installAndStartBlockAckPrerequisite(
        IOriginatorBlockAckAgreementHandler::PrerequisiteReservation& reservation,
        queueing::IPacketQueue *queue, InProgressFrames *inProgressFrames,
        const std::function<void(Packet *)>& reclaimOwnership,
        const std::function<void()>& startSequence)
{
    auto packet = reservation.getPacket();
    RollbackGuard rollback([&] {
        bool installed = false;
        for (int i = 0; i < inProgressFrames->getLength(); ++i)
            if (inProgressFrames->getFrames(i) == packet) {
                inProgressFrames->removeInProgressFrame(packet);
                installed = true;
                break;
            }
        for (int i = 0; i < queue->getNumPackets(); ++i)
            if (queue->getPacket(i) == packet) {
                queue->removePacket(packet);
                installed = true;
                break;
            }
        if (installed) {
            reclaimOwnership(packet);
            delete packet;
        }
        else if (packet->getOwner() == nullptr)
            delete packet;
        reservation.cancel();
    });
    queue->enqueuePacket(reservation.releasePacket());
    if (!inProgressFrames->reserveExactPendingFrame(packet))
        throw cRuntimeError("Reserved VHT ADDBA prerequisite changed queue ownership");
    startSequence();
    reservation.commit();
    rollback.release();
}

void VhtHcfFeature::configureProtectionAndStart(TxopProcedure *txop,
        const std::function<void()>& startSequence)
{
    auto protectionSnapshot = txop->getProtectionStateSnapshot();
    RollbackGuard rollback([txop, protectionSnapshot] {
        txop->restoreProtectionStateSnapshot(protectionSnapshot);
    });
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
    startSequence();
    rollback.release();
}

std::optional<VhtGrantSnapshot> VhtHcfFeature::prepareBlockAckPrerequisite(
        AccessCategory ac) const
{
    auto handler = actions->getBlockAckHandler();
    auto policy = actions->getBlockAckPolicy();
    if (handler == nullptr || policy == nullptr)
        return std::nullopt;
    auto edcaf = actions->getEdca()->getEdcaf(ac);
    auto queue = edcaf == nullptr ? nullptr : edcaf->getPendingQueue();
    if (queue == nullptr)
        return std::nullopt;
    for (int i = 0; i < queue->getNumPackets(); ++i) {
        auto packet = queue->getPacket(i);
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || header->getType() != ST_DATA_WITH_QOS)
            continue;
        auto probe = handler->probeQueuedDataFramePrerequisite(packet, header, policy);
        if (!probe.required)
            continue;
        VhtGrantSnapshot snapshot;
        snapshot.startKind = VhtGrantSnapshot::StartKind::BLOCK_ACK_PREREQUISITE;
        snapshot.exchangeClass = HcfExchangeClass::VHT_DL_MULTIUSER;
        snapshot.accessCategory = ac;
        snapshot.blockAckProbe = probe;
        snapshot.sourceQueueToken = HcfQueueToken(static_cast<uint64_t>(ac) + 1);
        snapshot.packetIdentity = HcfPacketIdentity(packet->getId());
        return snapshot;
    }
    return std::nullopt;
}

std::optional<VhtGrantSnapshot> VhtHcfFeature::prepareDlMu(AccessCategory ac) const
{
    auto mac = actions->getMac();
    auto modeSet = actions->getModeSet();
    if (!enableDlMuMimo || radio == nullptr || radio->getVhtAntennaCount() < 2 ||
            mac->getMib()->getStationType() != Ieee80211Mib::ACCESS_POINT)
        return std::nullopt;
    auto peers = getConstrainedMuPeers();
    if (peers.size() < 2)
        return std::nullopt;
    auto edcaf = actions->getEdca()->getEdcaf(ac);
    auto queue = edcaf->getPendingQueue();
    std::vector<Packet *> candidatePackets;
    if (auto inProgressFrame = edcaf->getInProgressFrames()->getFrameToTransmit())
        candidatePackets.push_back(inProgressFrame);
    for (int i = 0; i < queue->getNumPackets(); ++i)
        candidatePackets.push_back(queue->getPacket(i));

    IIeee80211VhtDlMuScheduler::Context context;
    context.enabled = true;
    context.accessPoint = true;
    context.packetLevelRadio = true;
    context.channelWidth = radio->getVhtChannelWidth();
    context.transmitDimensions = std::min(8, radio->getVhtAntennaCount());
    context.maxNstsTotal = context.transmitDimensions;
    context.shortGi = mac->getMib()->vhtOperation.shortGi;
    for (const auto& peer : peers) {
        auto negotiated = mac->getMib()->getNegotiatedVhtCapabilities(peer);
        bool peerSupportsShortGi = negotiated &&
                (context.channelWidth == MHz(80) ? negotiated->localTxPeerRx.shortGi80 :
                 context.channelWidth == MHz(160) ? negotiated->localTxPeerRx.shortGi160 : false);
        if (!peerSupportsShortGi)
            context.shortGi = false;
    }
    context.groupId = dlMuGroupId;
    std::set<MacAddress> addedPeers;
    for (auto packet : candidatePackets) {
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || header->getType() != ST_DATA_WITH_QOS)
            continue;
        auto peerIt = std::find(peers.begin(), peers.end(), header->getReceiverAddress());
        if (peerIt == peers.end() || addedPeers.count(*peerIt) > 0)
            continue;
        auto peer = *peerIt;
        addedPeers.insert(peer);
        auto peerIndex = std::distance(peers.begin(), peerIt);
        auto position = (peerIndex + context.groupId) % 4;
        auto negotiated = mac->getMib()->getNegotiatedVhtCapabilities(peer);
        auto generation = mac->getMib()->getVhtAssociationGeneration(peer);
        auto csi = soundingService.getCsiCache().findFresh(peer, context.channelWidth, generation);
        auto modeRequest = packet->findTag<physicallayer::Ieee80211ModeReq>();
        auto mode = modeRequest == nullptr ? actions->getRateSelection()->computeMode(
                packet, header, edcaf->getTxopProcedure()) : modeRequest->getMode();
        const int soundingNsts = !negotiated ? 0 : negotiated->localTxPeerRx.soundingNsts;
        if (mode != nullptr && modeSet->getPhyFamily(mode) ==
                physicallayer::Ieee80211PhyFamily::VHT && generation > 0 &&
                soundingNsts >= 2 &&
                (csi == nullptr || !csi->feedbackTypeMu ||
                 csi->nc < static_cast<uint8_t>(soundingNsts)) &&
                soundingService.getCoordinator().mayAttempt(peer)) {
            auto ndpMode = modeSet->getVhtSuNdpMode(mode, soundingNsts);
            VhtGrantSnapshot snapshot;
            snapshot.startKind = VhtGrantSnapshot::StartKind::MU_SOUNDING;
            snapshot.exchangeClass = HcfExchangeClass::VHT_DL_MULTIUSER;
            snapshot.accessCategory = ac;
            snapshot.peer = peer;
            snapshot.associationId = getPeerAssociationId(peer);
            snapshot.associationGeneration = generation;
            snapshot.soundingNsts = soundingNsts;
            snapshot.dialogToken = soundingService.getNextDialogToken() & 0x3f;
            snapshot.soundingModeIdentity = ndpMode->getName();
            snapshot.soundingMode = ndpMode;
            snapshot.muFeedback = true;
            return snapshot;
        }
        IIeee80211VhtDlMuScheduler::Candidate candidate;
        candidate.peer = peer;
        candidate.associationId = getPeerAssociationId(peer);
        candidate.tid = header->getTid();
        candidate.associationGeneration = generation;
        candidate.userPosition = position;
        candidate.numberOfSpatialStreams = 0;
        candidate.mcs = -1;
        candidate.ldpc = false;
        if (negotiated) {
            auto perUserNssLimit = std::min({4,
                    context.transmitDimensions / static_cast<int>(peers.size()),
                    negotiated->localTxPeerRx.maxNstsTotal / static_cast<int>(peers.size()),
                    negotiated->localTxPeerRx.soundingNsts / static_cast<int>(peers.size())});
            for (int nss = perUserNssLimit; nss >= 1 && candidate.mcs < 0; --nss) {
                auto maxMcs = negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[nss - 1];
                for (int mcs = maxMcs; mcs >= 0 && candidate.mcs < 0; --mcs) {
                    for (bool ldpc : {negotiated->localTxPeerRx.ldpc, false}) {
                        if (!ldpc && negotiated->localTxPeerRx.ldpc)
                            continue;
                        if (modeSet->findVhtMode(mcs, nss, context.channelWidth, ldpc)) {
                            candidate.numberOfSpatialStreams = nss;
                            candidate.mcs = mcs;
                            candidate.ldpc = ldpc;
                            break;
                        }
                    }
                }
            }
        }
        auto enqueueTime = packet->findTag<OrigEnqueueTimeTag>();
        candidate.enqueueTime = enqueueTime == nullptr ? packet->getArrivalTime() :
                enqueueTime->getEnqueueTime();
        candidate.sourceQueueToken = HcfQueueToken(static_cast<uint64_t>(ac) + 1);
        candidate.packetIdentity = HcfPacketIdentity(packet->getId());
        auto psduBytes = B(4) + B(packet->getByteLength());
        psduBytes += B((4 - psduBytes.get<B>() % 4) % 4);
        candidate.psduLength = psduBytes;
        candidate.beamformingGainDb = csi == nullptr ? 0 : csi->beamformingGainDb;
        candidate.leakagePenaltyDb = 0;
        candidate.soundingNsts = !negotiated ? 0 : negotiated->localTxPeerRx.soundingNsts;
        candidate.receiverMaxNstsTotal = !negotiated ? 0 : negotiated->localTxPeerRx.maxNstsTotal;
        candidate.associated = isAssociatedPeer(peer);
        candidate.negotiatedMuMimo = negotiated && negotiated->localTxPeerRx.valid &&
                negotiated->localTxPeerRx.muMimo;
        candidate.exactlyOneSpatialStream = negotiated &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[0] >= 0 &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[1] < 0;
        candidate.freshCsi = csi != nullptr && csi->feedbackTypeMu &&
                csi->soundingNsts >= candidate.numberOfSpatialStreams &&
                csi->nc >= candidate.numberOfSpatialStreams;
        candidate.activeGroup = groupIdManager->isActive(peer, context.groupId,
                position, generation, context.channelWidth);
        candidate.activeBlockAckAgreement = hasActiveOriginatorBlockAckAgreement(
                actions->getBlockAckHandler(), peer, header->getTid());
        candidate.unsegmented = header->getFragmentNumber() == 0 &&
                !header->getMoreFragments();
        context.candidates.push_back(candidate);
    }
    auto selectedIndices = dlMuScheduler->schedule(
            IIeee80211VhtDlMuScheduler::SchedulingContext(context));
    std::vector<IIeee80211VhtDlMuScheduler::Candidate> selected;
    for (auto index : selectedIndices) {
        if (index >= context.candidates.size())
            throw cRuntimeError("VHT DL MU scheduler returned an invalid candidate index");
        selected.push_back(context.candidates[index]);
    }
    VhtDlMuPlanDiagnostic diagnostic;
    auto plan = VhtDlMuPlan::create(context, selected, diagnostic);
    if (!plan)
        return std::nullopt;
    VhtGrantSnapshot snapshot;
    snapshot.startKind = VhtGrantSnapshot::StartKind::DL_MULTIUSER;
    snapshot.exchangeClass = HcfExchangeClass::VHT_DL_MULTIUSER;
    snapshot.accessCategory = ac;
    snapshot.dlMuPlan = *plan;
    return snapshot;
}

std::optional<VhtGrantSnapshot> VhtHcfFeature::prepareSuSounding(
        AccessCategory ac) const
{
    auto edcaf = actions->getEdca()->getEdcaf(ac);
    auto packet = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (packet != nullptr) {
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        auto peer = header->getReceiverAddress();
        auto txop = edcaf->getTxopProcedure();
        auto modeRequest = packet->findTag<physicallayer::Ieee80211ModeReq>();
        auto mode = modeRequest == nullptr ? actions->getRateSelection()->computeMode(
                packet, header, txop) : modeRequest->getMode();
        if (mode == nullptr)
            return std::nullopt;
        int soundingNsts = 0;
        auto generation = actions->getMac()->getMib()->getVhtAssociationGeneration(peer);
        auto csi = soundingService.getCsiCache().findFresh(peer,
                mode->getDataMode()->getBandwidth(), generation);
        if (isEligibleSu(mode, peer, soundingNsts) && generation > 0 &&
                (csi == nullptr || csi->soundingNsts < soundingNsts) &&
                soundingService.getCoordinator().mayAttempt(peer)) {
            auto modeSet = actions->getModeSet();
            auto ndpMode = modeSet->getVhtSuNdpMode(mode, soundingNsts);
            VhtGrantSnapshot snapshot;
            snapshot.startKind = VhtGrantSnapshot::StartKind::SU_SOUNDING;
            snapshot.exchangeClass = HcfExchangeClass::VHT_SU_SOUNDING;
            snapshot.accessCategory = ac;
            snapshot.peer = peer;
            snapshot.associationId = getPeerAssociationId(peer);
            snapshot.associationGeneration = generation;
            snapshot.soundingNsts = soundingNsts;
            snapshot.dialogToken = soundingService.getNextDialogToken() & 0x3f;
            snapshot.soundingModeIdentity = ndpMode->getName();
            snapshot.soundingMode = ndpMode;
            return snapshot;
        }
    }
    return std::nullopt;
}

VhtGrantSnapshot VhtHcfFeature::prepareGrantSnapshot(AccessCategory ac) const
{
    if (enableDlMuMimo) {
        auto peers = getConstrainedMuPeers();
        for (size_t i = 0; i < peers.size(); ++i) {
            uint8_t position = (i + dlMuGroupId) % 4;
            auto generation = actions->getMac()->getMib()->getVhtAssociationGeneration(peers[i]);
            auto state = groupIdManager->getState(peers[i], dlMuGroupId, generation,
                    soundingService.getEffectiveChannelWidth());
            if (state == IVhtGroupIdManager::State::ABSENT) {
                VhtGrantSnapshot snapshot;
                snapshot.startKind = VhtGrantSnapshot::StartKind::GROUP_MANAGEMENT;
                snapshot.exchangeClass = HcfExchangeClass::VHT_GROUP_MANAGEMENT;
                snapshot.accessCategory = ac;
                snapshot.peer = peers[i];
                snapshot.associationGeneration = generation;
                snapshot.groupId = dlMuGroupId;
                snapshot.userPosition = position;
                snapshot.channelWidth = soundingService.getEffectiveChannelWidth();
                return snapshot;
            }
            if (state == IVhtGroupIdManager::State::PENDING)
                break;
        }
        if (auto snapshot = prepareBlockAckPrerequisite(ac))
            return *snapshot;
        if (auto snapshot = prepareDlMu(ac))
            return *snapshot;
    }
    if (auto snapshot = prepareSuSounding(ac))
        return *snapshot;
    VhtGrantSnapshot snapshot;
    snapshot.accessCategory = ac;
    return snapshot;
}

void VhtHcfFeature::commitSounding(const VhtGrantSnapshot& snapshot)
{
    auto ndpMode = snapshot.soundingMode;
    if (ndpMode == nullptr || !soundingService.getCoordinator().mayAttempt(snapshot.peer))
        throw cRuntimeError("Prepared VHT sounding exchange became stale before commit");
    auto token = soundingService.commitDialogToken(snapshot.dialogToken);
    RollbackGuard rollback([this, token] { soundingService.rollbackDialogToken(token); });
    auto sequence = new VhtSoundingFs(actions->getMac()->getMib(),
            &soundingService.getCsiCache(), snapshot.peer, snapshot.associationId,
            snapshot.associationGeneration, token, snapshot.soundingNsts,
            actions->getModeSet(), ndpMode, beamformingGainDb,
            snapshot.muFeedback, snapshot.muFeedback ? snapshot.soundingNsts : 1);
    actions->startFeatureFrameSequence(sequence, snapshot.accessCategory);
    soundingService.getCoordinator().recordAttempt(snapshot.peer);
    rollback.release();
}

void VhtHcfFeature::commitBlockAckPrerequisite(const VhtGrantSnapshot& snapshot)
{
    if (!(snapshot.sourceQueueToken == HcfQueueToken(
            static_cast<uint64_t>(snapshot.accessCategory) + 1)))
        throw cRuntimeError("Prepared VHT ADDBA prerequisite has a stale source queue token");
    auto edcaf = actions->getEdca()->getEdcaf(snapshot.accessCategory);
    auto queue = edcaf->getPendingQueue();
    Packet *packet = nullptr;
    for (int i = 0; i < queue->getNumPackets(); ++i)
        if (HcfPacketIdentity(queue->getPacket(i)->getId()) == snapshot.packetIdentity) {
            packet = queue->getPacket(i);
            break;
        }
    if (packet == nullptr)
        throw cRuntimeError("Prepared VHT ADDBA prerequisite lost its exact packet");
    auto header = dynamicPtrCast<const Ieee80211DataHeader>(
            packet->peekAtFront<Ieee80211MacHeader>());
    auto reservation = actions->getBlockAckHandler()->reserveQueuedDataFramePrerequisite(
            snapshot.blockAckProbe, packet, header, actions->getBlockAckPolicy());
    if (!reservation)
        throw cRuntimeError("Prepared VHT ADDBA prerequisite became stale before commit");
    auto prerequisiteEdcaf = actions->getEdca()->getEdcaf(AccessCategory::AC_VO);
    installAndStartBlockAckPrerequisite(reservation,
            prerequisiteEdcaf->getPendingQueue(),
            prerequisiteEdcaf->getInProgressFrames(),
            [this](Packet *packet) { actions->reclaimPacketOwnership(packet); },
            [this, prerequisiteEdcaf] {
                auto txop = prerequisiteEdcaf->getTxopProcedure();
                configureProtectionAndStart(txop, [this] {
                    actions->startFeatureFrameSequence(new HcfFs(), AccessCategory::AC_VO);
                });
            });
}

void VhtHcfFeature::commitDlMu(const VhtGrantSnapshot& snapshot)
{
    if (!snapshot.dlMuPlan)
        throw cRuntimeError("Prepared VHT DL MU exchange has no validated plan");
    auto edcaf = actions->getEdca()->getEdcaf(snapshot.accessCategory);
    auto txop = edcaf->getTxopProcedure();
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
    auto token = nextTransactionToken++;
    activeTransactionToken = token;
    completedUsers.assign(snapshot.dlMuPlan->getUsers().size(), false);
    activePlanCommitted = false;
    activeContainerPacket = nullptr;
    activeUserPackets.clear();
    RollbackGuard rollback([this] {
        activeTransactionToken = 0;
        completedUsers.clear();
        activePlanCommitted = false;
        activeContainerPacket = nullptr;
        activeUserPackets.clear();
        --nextTransactionToken;
    });
    auto sequence = txOpFactory->create(*snapshot.dlMuPlan, actions->getModeSet(),
            edcaf->getAckHandler(), actions->getFrameSequenceCallback(), this, token);
    actions->startFeatureFrameSequence(sequence, snapshot.accessCategory);
    rollback.release();
}

void VhtHcfFeature::commitGrantSnapshot(const VhtGrantSnapshot& snapshot)
{
    switch (snapshot.startKind) {
        case VhtGrantSnapshot::StartKind::COMMON_SINGLE_USER:
            actions->continueBaseFrameSequence(snapshot.accessCategory);
            break;
        case VhtGrantSnapshot::StartKind::GROUP_MANAGEMENT: {
            auto txop = actions->getEdca()->getEdcaf(snapshot.accessCategory)->getTxopProcedure();
            if (!txop->isProtectionConfigured())
                txop->configureProtection(TxopProcedure::InitialProtection::NONE);
            actions->startFeatureFrameSequence(new VhtGroupIdManagementFs(
                    actions->getMac()->getMib(), groupIdManager, snapshot.peer,
                    snapshot.groupId, snapshot.userPosition,
                    snapshot.associationGeneration, snapshot.channelWidth),
                    snapshot.accessCategory);
            break;
        }
        case VhtGrantSnapshot::StartKind::BLOCK_ACK_PREREQUISITE:
            commitBlockAckPrerequisite(snapshot);
            break;
        case VhtGrantSnapshot::StartKind::MU_SOUNDING:
        case VhtGrantSnapshot::StartKind::SU_SOUNDING:
            commitSounding(snapshot);
            break;
        case VhtGrantSnapshot::StartKind::DL_MULTIUSER:
            commitDlMu(snapshot);
            break;
    }
}

void VhtHcfFeature::startFrameSequence(AccessCategory ac)
{
    commitGrantSnapshot(prepareGrantSnapshot(ac));
}

bool VhtHcfFeature::processHeaderlessNdpIndication(Packet *packet)
{
    auto indication = packet->findTag<physicallayer::Ieee80211NdpInd>();
    if (indication == nullptr || indication->getPhyFormat() !=
            physicallayer::IEEE80211_NDP_PHY_VHT)
        return false;
    return soundingService.getCoordinator().processHeaderlessNdp(packet,
            actions->getMac(), actions->getModeSet(), actions->getTx(),
            actions->getTxCallback(),
            enableSuBeamforming || enableDlMuMimo);
}

void VhtHcfFeature::recipientProcessReceivedFrame(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header)
{
    auto action = dynamicPtrCast<const Ieee80211ActionFrame>(header);
    auto group = action != nullptr && action->getCategory() == 21 ?
            findVhtActionBody<Ieee80211VhtGroupIdManagement>(packet) : nullptr;
    if (group != nullptr) {
        auto mib = actions->getMac()->getMib();
        auto peer = action->getTransmitterAddress();
        auto generation = mib->getVhtAssociationGeneration(peer);
        auto negotiated = mib->getNegotiatedVhtCapabilities(peer);
        bool permitted = enableDlMuMimo &&
                mib->getStationType() == Ieee80211Mib::STATION &&
                mib->isAssociated() && mib->getBssid() == peer && negotiated &&
                negotiated->localRxPeerTx.valid && negotiated->localRxPeerTx.muMimo &&
                radio != nullptr;
        auto channelWidth = radio == nullptr ? Hz(0) : radio->getVhtChannelWidth();
        if (!permitted || !groupIdManager->consume(peer, group, generation, channelWidth))
            groupIdManager->invalidatePeer(peer);
    }
    if (dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(header) != nullptr &&
            soundingService.getCoordinator().processNdpAnnouncement(packet, header,
                    actions->getMac(), enableSuBeamforming || enableDlMuMimo,
                    actions->getMac()->getMib()->vhtOperation.operatingChannelWidth))
        return;
    actions->continueBaseRecipientFrame(packet, header);
}

void VhtHcfFeature::setFrameMode(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header,
        const physicallayer::IIeee80211Mode *mode) const
{
    actions->continueBaseSetFrameMode(packet, header, mode);
    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
    if (dataHeader == nullptr)
        return;
    if (auto request = packet->findTagForUpdate<physicallayer::Ieee80211VhtTransmissionTag>()) {
        request->setBeamformed(false);
        request->setBeamformingGainDb(0);
        request->setGroupId(63);
        request->setPartialAid(0);
    }
    int soundingNsts = 0;
    auto peer = dataHeader->getReceiverAddress();
    auto generation = actions->getMac()->getMib()->getVhtAssociationGeneration(peer);
    auto entry = isEligibleSu(mode, peer, soundingNsts) && generation > 0 ?
            soundingService.getCsiCache().findFresh(peer,
                    mode->getDataMode()->getBandwidth(), generation) : nullptr;
    if (entry != nullptr) {
        auto request = packet->addTagIfAbsent<physicallayer::Ieee80211VhtTransmissionTag>();
        request->setBeamformed(true);
        request->setBeamformingGainDb(entry->beamformingGainDb);
        request->setGroupId(0);
        request->setPartialAid(getPeerAssociationId(peer));
    }
}

void VhtHcfFeature::transmitFrame(Packet *packet, simtime_t ifs)
{
    if (packet == activeContainerPacket) {
        auto header = makeShared<Ieee80211DataHeader>();
        header->setType(ST_DATA_WITH_QOS);
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        header->setTransmitterAddress(actions->getMac()->getAddress());
        header->setAddress3(actions->getMac()->getMib()->getBssid());
        actions->getTx()->transmitFrame(packet, header, ifs,
                actions->getTxCallback());
        return;
    }
    if (isVhtNdp(packet)) {
        auto header = makeShared<Ieee80211DataHeader>();
        header->setType(ST_QOS_NULL);
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        header->setTransmitterAddress(actions->getMac()->getAddress());
        header->setAddress3(actions->getMac()->getMib()->getBssid());
        actions->getTx()->transmitFrame(packet, header, ifs,
                actions->getTxCallback());
        return;
    }
    if (dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(
            packet->peekAtFront()) != nullptr) {
        auto header = packet->peekAtFront<Ieee80211VhtNdpAnnouncementFrame>();
        packet->addTagIfAbsent<physicallayer::Ieee80211ModeReq>()->setMode(
                actions->getModeSet()->getSlowestMandatoryMode(MHz(20)));
        actions->getTx()->transmitFrame(packet, header, ifs,
                actions->getTxCallback());
        return;
    }
    auto header = packet->peekAtFront<Ieee80211MacHeader>();
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(header) != nullptr ||
            dynamicPtrCast<const Ieee80211BlockAckReq>(header) != nullptr ||
            dynamicPtrCast<const Ieee80211BlockAck>(header) != nullptr)
        packet->addTagIfAbsent<physicallayer::Ieee80211ModeReq>()->setMode(
                actions->getModeSet()->getSlowestMandatoryMode(MHz(20)));
    actions->continueBaseTransmitFrame(packet, ifs);
}

void VhtHcfFeature::originatorProcessTransmittedFrame(Packet *packet)
{
    if (packet == activeContainerPacket) {
        auto edcaf = actions->getEdca()->getChannelOwner();
        ASSERT(edcaf != nullptr);
        for (const auto& userPackets : activeUserPackets)
            for (auto packet : userPackets) {
                auto header = packet->peekAtFront<Ieee80211DataHeader>();
                actions->processTransmittedDataFrame(packet, header,
                        edcaf->getAccessCategory());
                edcaf->getAckHandler()->transitionToWaitingForBlockAck(header);
            }
        return;
    }
    if (isVhtNdp(packet) || dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(
            packet->peekAtFront()) != nullptr)
        return;
    actions->continueBaseTransmittedFrame(packet);
}

void VhtHcfFeature::originatorProcessReceivedFrame(Packet *packet,
        Packet *lastTransmittedPacket)
{
    if (isVhtNdp(lastTransmittedPacket) && containsVhtFeedback(packet))
        return;
    actions->continueBaseReceivedFrame(packet, lastTransmittedPacket);
}

void VhtHcfFeature::transmissionComplete(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header)
{
    if (header != nullptr && header->getType() == ST_NOACKACTION &&
            containsVhtFeedback(packet))
        return;
    actions->continueBaseTransmissionComplete(packet, header);
}

void VhtHcfFeature::invalidatePeer(const MacAddress& peer)
{
    soundingService.invalidatePeer(peer);
    groupIdManager->invalidatePeer(peer);
}

Ieee80211Mac *VhtHcfFeature::getVhtDlMuMac() const
{
    return actions->getMac();
}

IOriginatorMacDataService *VhtHcfFeature::getVhtDlMuOriginatorDataService() const
{
    return actions->getOriginatorDataService();
}

queueing::IPacketQueue *VhtHcfFeature::resolveVhtDlMuQueue(
        HcfQueueToken sourceQueueToken) const
{
    auto value = sourceQueueToken.getValue();
    if (value == 0 || value > AC_NUMCATEGORIES)
        return nullptr;
    auto edcaf = actions->getEdca()->getEdcaf(
            static_cast<AccessCategory>(value - 1));
    return edcaf == nullptr ? nullptr : edcaf->getPendingQueue();
}

void VhtHcfFeature::vhtDlMuPlanCommitted(uint64_t transactionToken,
        Packet *containerPacket,
        const std::vector<std::vector<Packet *>>& userPackets)
{
    if (transactionToken != activeTransactionToken)
        throw cRuntimeError("VHT DL MU plan committed for a stale transaction");
    activeContainerPacket = containerPacket;
    activeUserPackets = userPackets;
    activePlanCommitted = true;
}

void VhtHcfFeature::processVhtDlMuFailedFrame(Packet *packet)
{
    actions->processFailedFrame(packet);
}

void VhtHcfFeature::processVhtDlMuUserResult(uint64_t transactionToken,
        unsigned int userIndex, UserResult result)
{
    if (transactionToken != activeTransactionToken ||
            userIndex >= completedUsers.size() || completedUsers[userIndex])
        return;
    if (result == UserResult::BLOCK_ACK_RECEIVED ||
            result == UserResult::BLOCK_ACK_TIMED_OUT)
        completedUsers[userIndex] = true;
    if (activePlanCommitted && !completedUsers.empty() &&
            std::all_of(completedUsers.begin(), completedUsers.end(),
                    [] (bool completed) { return completed; })) {
        activeContainerPacket = nullptr;
        activeUserPackets.clear();
        activeTransactionToken = 0;
        activePlanCommitted = false;
    }
}

} // namespace ieee80211
} // namespace inet
