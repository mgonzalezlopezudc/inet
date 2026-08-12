//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcf.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtSoundingCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtGroupIdManager.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtSoundingFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtGroupIdManagementFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

Define_Module(VhtHcf);

void VhtHcf::validatePacketLevelRadio(cModule *radio)
{
    if (dynamic_cast<physicallayer::IIeee80211VhtPacketRadio *>(radio) == nullptr)
        throw cRuntimeError("VHT SU beamforming requires an Ieee80211 packet-level radio");
}

void VhtHcf::receiveSignal(cComponent *source, simsignal_t signalID,
        cObject *obj, cObject *details)
{
    ModeSetListener::receiveSignal(source, signalID, obj, details);
    if (signalID == modesetChangedSignal && vhtRadio != nullptr) {
        updateEffectiveChannelWidth(csiCache, lastEffectiveChannelWidth,
                vhtRadio->getVhtChannelWidth());
        auto selection = vhtRadio->getVhtMuRxSelection();
        if (selection.active && selection.channelWidth != vhtRadio->getVhtChannelWidth())
            vhtRadio->setVhtMuRxSelection({});
    }
}

void VhtHcf::updateEffectiveChannelWidth(VhtCsiCache& cache,
        Hz& previousWidth, Hz currentWidth)
{
    if (previousWidth != Hz(0) && currentWidth != previousWidth)
        cache.clear();
    previousWidth = currentWidth;
}

namespace {

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

void VhtHcf::initialize(int stage)
{
    Hcf::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        enableVhtSuBeamforming = par("enableVhtSuBeamforming");
        enableVhtDlMuMimo = par("enableVhtDlMuMimo");
        auto configuredGroupId = par("vhtDlMuGroupId").intValue();
        if (configuredGroupId < 1 || configuredGroupId > 62)
            throw cRuntimeError("vhtDlMuGroupId must be in the range 1..62");
        vhtDlMuGroupId = configuredGroupId;
        beamformingGainDb = par("beamformingGain");
        csiCache.configure(par("vhtCsiValidityDuration"));
        soundingCoordinator = check_and_cast<IVhtSoundingCoordinator *>(
                getSubmodule("soundingCoordinator"));
        groupIdManager = check_and_cast<IVhtGroupIdManager *>(
                getSubmodule("groupIdManager"));
        dlMuScheduler = check_and_cast<IIeee80211VhtDlMuScheduler *>(
                getSubmodule("dlMuScheduler"));
        if (enableVhtSuBeamforming || enableVhtDlMuMimo) {
            auto radio = getContainingNicModule(this)->getSubmodule("radio");
            validatePacketLevelRadio(radio);
            vhtRadio = check_and_cast<physicallayer::IIeee80211VhtPacketRadio *>(radio);
            lastEffectiveChannelWidth = vhtRadio->getVhtChannelWidth();
        }
        groupIdManager->setLocalMembershipListener(this);
        WATCH(enableVhtSuBeamforming);
        WATCH(enableVhtDlMuMimo);
        WATCH(nextDialogToken);
    }
}

std::vector<MacAddress> VhtHcf::getConstrainedVhtMuPeers() const
{
    std::vector<std::pair<MacAddress, int>> capablePeers;
    auto mib = mac->getMib();
    if (!enableVhtDlMuMimo || mib->getStationType() != Ieee80211Mib::ACCESS_POINT)
        return {};
    auto channelWidth = vhtRadio == nullptr ? Hz(0) : vhtRadio->getVhtChannelWidth();
    for (const auto& station : mib->getPeerAssociationSnapshots()) {
        auto negotiated = mib->getNegotiatedVhtCapabilities(station.getAddress());
        if (!station.hasMemberStatus() || station.getMemberStatus() != Ieee80211Mib::ASSOCIATED || !negotiated ||
                !negotiated->localTxPeerRx.valid || !negotiated->localTxPeerRx.muMimo ||
                !negotiated->localTxPeerRx.supportedChannelWidths.count(channelWidth) ||
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[0] < 0 ||
                negotiated->localTxPeerRx.maxNstsTotal < 2 ||
                negotiated->localTxPeerRx.soundingNsts < 2)
            continue;
        capablePeers.emplace_back(station.getAddress(), negotiated->localTxPeerRx.soundingNsts);
    }
    std::sort(capablePeers.begin(), capablePeers.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    auto groupDimension = std::min<size_t>(4, vhtRadio == nullptr ? 0 :
            vhtRadio->getVhtAntennaCount());
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

VhtDlMuTxOpFs *VhtHcf::createVhtDlMuTxOpFs(const VhtDlMuPlan& plan,
        IAckHandler *ackHandler)
{
    return new VhtDlMuTxOpFs(plan, modeSet, ackHandler, this);
}

void VhtHcf::prioritizeQueuedAddbaRequests()
{
    auto edcaf = edca->getEdcaf(AccessCategory::AC_VO);
    if (edcaf == nullptr)
        return;
    auto queue = edcaf->getPendingQueue();
    std::vector<Packet *> addbaPackets;
    std::vector<Packet *> otherPackets;
    while (!queue->isEmpty()) {
        auto packet = queue->dequeuePacket();
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (dynamicPtrCast<const Ieee80211AddbaRequest>(header) != nullptr)
            addbaPackets.push_back(packet);
        else
            otherPackets.push_back(packet);
    }
    for (auto packet : addbaPackets)
        queue->enqueuePacket(packet);
    for (auto packet : otherPackets)
        queue->enqueuePacket(packet);
}

bool VhtHcf::tryStartVhtDlMu(AccessCategory ac)
{
    if (!enableVhtDlMuMimo || vhtRadio == nullptr ||
            vhtRadio->getVhtAntennaCount() < 2 ||
            mac->getMib()->getStationType() != Ieee80211Mib::ACCESS_POINT)
        return false;
    auto peers = getConstrainedVhtMuPeers();
    if (peers.size() < 2)
        return false;
    auto edcaf = edca->getEdcaf(ac);
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
    context.channelWidth = vhtRadio->getVhtChannelWidth();
    context.transmitDimensions = std::min(8, vhtRadio->getVhtAntennaCount());
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
    context.groupId = vhtDlMuGroupId;
    std::set<MacAddress> addedPeers;
    for (auto packet : candidatePackets) {
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || header->getType() != ST_DATA_WITH_QOS)
            continue;
        auto peerIt = std::find(peers.begin(), peers.end(), header->getReceiverAddress());
        if (peerIt == peers.end())
            continue;
        auto peer = *peerIt;
        if (addedPeers.count(peer) > 0)
            continue;
        addedPeers.insert(peer);
        auto peerIndex = std::distance(peers.begin(), peerIt);
        auto position = (peerIndex + context.groupId) % 4;
        auto negotiated = mac->getMib()->getNegotiatedVhtCapabilities(peer);
        auto generation = mac->getMib()->getVhtAssociationGeneration(peer);
        auto csi = csiCache.findFresh(peer, context.channelWidth, generation);
        auto modeRequest = packet->findTag<physicallayer::Ieee80211ModeReq>();
        auto mode = modeRequest == nullptr ? rateSelection->computeMode(packet, header,
                edcaf->getTxopProcedure()) : modeRequest->getMode();
        const int soundingNsts = !negotiated ? 0 :
                negotiated->localTxPeerRx.soundingNsts;
        if (mode != nullptr && modeSet->getPhyFamily(mode) ==
                physicallayer::Ieee80211PhyFamily::VHT && generation > 0 &&
                soundingNsts >= 2 &&
                (csi == nullptr || !csi->feedbackTypeMu ||
                 csi->nc < static_cast<uint8_t>(soundingNsts)) &&
                soundingCoordinator->mayAttempt(peer)) {
            auto ndpMode = modeSet->getVhtSuNdpMode(mode, soundingNsts);
            auto sequence = new VhtSoundingFs(mac->getMib(), &csiCache, peer,
                    getPeerAssociationId(peer), generation,
                    nextDialogToken++ & 0x3f, soundingNsts, modeSet,
                    ndpMode, beamformingGainDb, true, soundingNsts);
            soundingCoordinator->recordAttempt(peer);
            frameSequenceHandler->startFrameSequence(sequence, buildContext(ac), this);
            emit(IFrameSequenceHandler::frameSequenceStartedSignal,
                    frameSequenceHandler->getContext());
            return true;
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
                        if (modeSet->findVhtMode(mcs, nss, context.channelWidth, ldpc) != nullptr) {
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
        candidate.sourceQueue = queue;
        candidate.packet = packet;
        auto psduBytes = B(4) + B(packet->getByteLength());
        psduBytes += B((4 - psduBytes.get<B>() % 4) % 4);
        candidate.psduLength = psduBytes;
        candidate.beamformingGainDb = csi == nullptr ? 0 : csi->beamformingGainDb;
        candidate.leakagePenaltyDb = 0;
        candidate.soundingNsts = !negotiated ? 0 : negotiated->localTxPeerRx.soundingNsts;
        candidate.receiverMaxNstsTotal = !negotiated ? 0 : negotiated->localTxPeerRx.maxNstsTotal;
        candidate.associated = isAssociatedPeer(peer);
        candidate.negotiatedMuMimo = negotiated &&
                negotiated->localTxPeerRx.valid && negotiated->localTxPeerRx.muMimo;
        candidate.exactlyOneSpatialStream = negotiated &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[0] >= 0 &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[1] < 0;
        candidate.freshCsi = csi != nullptr && csi->feedbackTypeMu &&
                csi->soundingNsts >= candidate.numberOfSpatialStreams &&
                csi->nc >= candidate.numberOfSpatialStreams;
        candidate.activeGroup = groupIdManager->isActive(peer, context.groupId, position,
                generation, context.channelWidth);
        candidate.activeBlockAckAgreement = hasActiveOriginatorBlockAckAgreement(
                originatorBlockAckAgreementHandler.get(), peer, header->getTid());
        if (!candidate.activeBlockAckAgreement && originatorBlockAckAgreementHandler != nullptr &&
                originatorBlockAckAgreementPolicy != nullptr &&
                originatorBlockAckAgreementHandler->processQueuedDataFrame(
                        packet, header, originatorBlockAckAgreementPolicy, this))
            prioritizeQueuedAddbaRequests();
        candidate.unsegmented = header->getFragmentNumber() == 0 &&
                !header->getMoreFragments();
        EV_INFO << "tryStartVhtDlMu Candidate for " << peer << ": mcs=" << candidate.mcs
                << ", assoc=" << candidate.associated << ", muMimo=" << candidate.negotiatedMuMimo
                << ", nss=" << candidate.numberOfSpatialStreams << ", freshCsi=" << candidate.freshCsi
                << ", activeGroup=" << candidate.activeGroup << ", activeBA=" << candidate.activeBlockAckAgreement
                << ", unseg=" << candidate.unsegmented << ", isEligible=" << IIeee80211VhtDlMuScheduler::isEligible(context, candidate) << "\n";
        context.candidates.push_back(candidate);
    }
    auto selected = dlMuScheduler->schedule(context);
    EV_INFO << "tryStartVhtDlMu candidates count=" << context.candidates.size() << ", selected count=" << selected.size() << "\n";
    VhtDlMuPlanDiagnostic diagnostic;
    auto plan = VhtDlMuPlan::create(context, selected, diagnostic);
    if (!plan) {
        EV_INFO << "tryStartVhtDlMu plan creation failed! selected count=" << selected.size() << "\n";
        return false;
    }
    EV_INFO << "tryStartVhtDlMu SUCCESS! Plan created for " << selected.size() << " users!\n";

    auto txop = edcaf->getTxopProcedure();
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
    auto sequence = createVhtDlMuTxOpFs(*plan, edcaf->getAckHandler());
    frameSequenceHandler->startFrameSequence(sequence, buildContext(ac), this);
    emit(IFrameSequenceHandler::frameSequenceStartedSignal,
            frameSequenceHandler->getContext());
    return true;
}

bool VhtHcf::isAssociatedPeer(const MacAddress& peer) const
{
    auto mib = mac->getMib();
    if (mib->getStationType() == Ieee80211Mib::ACCESS_POINT)
        return mib->isPeerAssociated(peer);
    return mib->isAssociated() && mib->getBssid() == peer;
}

uint16_t VhtHcf::getPeerAssociationId(const MacAddress& peer) const
{
    auto mib = mac->getMib();
    return mib->getStationType() == Ieee80211Mib::ACCESS_POINT ?
            mib->getAssociationId(peer) : mib->getLocalAssociationId();
}

bool VhtHcf::isEligibleVhtSu(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peer, int& soundingNsts) const
{
    soundingNsts = 0;
    if (!enableVhtSuBeamforming) { EV_INFO << "isEligibleVhtSu false: enableVhtSuBeamforming false\n"; return false; }
    if (mode == nullptr) { EV_INFO << "isEligibleVhtSu false: mode nullptr\n"; return false; }
    if (!modeSet->containsMode(mode)) { EV_INFO << "isEligibleVhtSu false: modeSet does not contain mode\n"; return false; }
    if (modeSet->getPhyFamily(mode) != physicallayer::Ieee80211PhyFamily::VHT) {
        EV_INFO << "isEligibleVhtSu false: phyFamily " << (int)modeSet->getPhyFamily(mode) << " is not VHT\n";
        return false;
    }
    if (peer.isMulticast()) { EV_INFO << "isEligibleVhtSu false: peer is multicast\n"; return false; }
    if (!isAssociatedPeer(peer)) { EV_INFO << "isEligibleVhtSu false: not associated peer " << peer << "\n"; return false; }
    auto negotiated = mac->getMib()->getNegotiatedVhtCapabilities(peer);
    if (!negotiated) { EV_INFO << "isEligibleVhtSu false: negotiated absent\n"; return false; }
    if (!negotiated->localTxPeerRx.valid) { EV_INFO << "isEligibleVhtSu false: localTxPeerRx not valid\n"; return false; }
    if (!negotiated->localTxPeerRx.suBeamforming) { EV_INFO << "isEligibleVhtSu false: suBeamforming false\n"; return false; }
    if (negotiated->localTxPeerRx.soundingNsts < 2) {
        EV_INFO << "isEligibleVhtSu false: soundingNsts " << negotiated->localTxPeerRx.soundingNsts << " < 2\n";
        return false;
    }
    soundingNsts = negotiated->localTxPeerRx.soundingNsts;
    EV_INFO << "isEligibleVhtSu TRUE: soundingNsts=" << soundingNsts << " for peer " << peer << "\n";
    return true;
}


void VhtHcf::startFrameSequence(AccessCategory ac)
{
    if (!enableVhtSuBeamforming && !enableVhtDlMuMimo) {
        csiCache.clear();
        groupIdManager->invalidateAll();
        Hcf::startFrameSequence(ac);
        return;
    }
    auto previousWidth = lastEffectiveChannelWidth;
    updateEffectiveChannelWidth(csiCache, lastEffectiveChannelWidth,
            vhtRadio->getVhtChannelWidth());
    if (previousWidth != Hz(0) && previousWidth != lastEffectiveChannelWidth) {
        groupIdManager->invalidateAll();
    }
    if (enableVhtDlMuMimo) {
        auto peers = getConstrainedVhtMuPeers();
        uint8_t groupId = vhtDlMuGroupId;
        for (size_t i = 0; i < peers.size(); ++i) {
            uint8_t position = (i + groupId) % 4;
            auto generation = mac->getMib()->getVhtAssociationGeneration(peers[i]);
            auto state = groupIdManager->getState(peers[i], groupId,
                    generation, lastEffectiveChannelWidth);
            if (state == IVhtGroupIdManager::State::ABSENT) {
                auto txop = edca->getEdcaf(ac)->getTxopProcedure();
                if (!txop->isProtectionConfigured())
                    txop->configureProtection(TxopProcedure::InitialProtection::NONE);
                auto sequence = new VhtGroupIdManagementFs(mac->getMib(),
                        groupIdManager, peers[i], groupId, position,
                        generation, lastEffectiveChannelWidth);
                frameSequenceHandler->startFrameSequence(sequence, buildContext(ac), this);
                emit(IFrameSequenceHandler::frameSequenceStartedSignal,
                        frameSequenceHandler->getContext());
                return;
            }
            if (state == IVhtGroupIdManager::State::PENDING)
                break;
        }
        if (tryStartVhtDlMu(ac))
            return;
    }
    auto edcaf = edca->getEdcaf(ac);
    auto packet = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (packet != nullptr) {
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        auto peer = header->getReceiverAddress();
        auto txop = edcaf->getTxopProcedure();
        auto modeRequest = packet->findTag<physicallayer::Ieee80211ModeReq>();
        auto mode = modeRequest == nullptr ? rateSelection->computeMode(packet, header, txop) :
                modeRequest->getMode();
        setFrameMode(packet, header, mode);
        int soundingNsts = 0;
        auto generation = mac->getMib()->getVhtAssociationGeneration(peer);
        auto csi = csiCache.findFresh(peer, mode->getDataMode()->getBandwidth(), generation);
        if (isEligibleVhtSu(mode, peer, soundingNsts) && generation > 0 &&
                (csi == nullptr || csi->soundingNsts < soundingNsts) &&
                soundingCoordinator->mayAttempt(peer)) {
            auto ndpMode = modeSet->getVhtSuNdpMode(mode, soundingNsts);
            auto sequence = new VhtSoundingFs(mac->getMib(), &csiCache, peer,
                    getPeerAssociationId(peer), generation,
                    nextDialogToken++ & 0x3f, soundingNsts, modeSet,
                    ndpMode, beamformingGainDb);
            soundingCoordinator->recordAttempt(peer);
            frameSequenceHandler->startFrameSequence(sequence, buildContext(ac), this);
            emit(IFrameSequenceHandler::frameSequenceStartedSignal,
                    frameSequenceHandler->getContext());
            return;
        }
    }
    Hcf::startFrameSequence(ac);
}

bool VhtHcf::processHeaderlessNdpIndication(Packet *packet)
{
    auto indication = packet->findTag<physicallayer::Ieee80211NdpInd>();
    if (indication == nullptr || indication->getPhyFormat() !=
            physicallayer::IEEE80211_NDP_PHY_VHT)
        return false;
    return soundingCoordinator->processHeaderlessNdp(packet, mac, modeSet, tx,
            this, enableVhtSuBeamforming || enableVhtDlMuMimo);
}

void VhtHcf::recipientProcessReceivedFrame(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header)
{
    auto action = dynamicPtrCast<const Ieee80211ActionFrame>(header);
    auto group = action != nullptr && action->getCategory() == 21 ?
            findVhtActionBody<Ieee80211VhtGroupIdManagement>(packet) : nullptr;
    if (group != nullptr) {
        auto mib = mac->getMib();
        auto peer = action->getTransmitterAddress();
        auto generation = mib->getVhtAssociationGeneration(peer);
        auto negotiated = mib->getNegotiatedVhtCapabilities(peer);
        bool permitted = enableVhtDlMuMimo &&
                mib->getStationType() == Ieee80211Mib::STATION &&
                mib->isAssociated() && mib->getBssid() == peer &&
                negotiated && negotiated->localRxPeerTx.valid &&
                negotiated->localRxPeerTx.muMimo &&
                vhtRadio != nullptr;
        auto channelWidth = vhtRadio == nullptr ? Hz(0) : vhtRadio->getVhtChannelWidth();
        if (!permitted || !groupIdManager->consume(peer, group, generation, channelWidth)) {
            groupIdManager->invalidatePeer(peer);
        }
    }
    if (dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(header) != nullptr &&
            soundingCoordinator->processNdpAnnouncement(packet, header, mac,
                    enableVhtSuBeamforming || enableVhtDlMuMimo,
                    mac->getMib()->vhtOperation.operatingChannelWidth))
        return;
    Hcf::recipientProcessReceivedFrame(packet, header);
}

void VhtHcf::setFrameMode(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header,
        const physicallayer::IIeee80211Mode *mode) const
{
    Hcf::setFrameMode(packet, header, mode);
    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
    if (dataHeader == nullptr)
        return;
    // A queued/retried packet may retain the tag attached by an earlier mode
    // selection. Clear the modeled benefit before revalidating CSI so expiry,
    // reassociation, or a width/capability change always falls back cleanly.
    if (auto request = packet->findTagForUpdate<physicallayer::Ieee80211VhtTransmissionTag>()) {
        request->setBeamformed(false);
        request->setBeamformingGainDb(0);
        request->setGroupId(63);
        request->setPartialAid(0);
    }
    int soundingNsts = 0;
    auto peer = dataHeader->getReceiverAddress();
    auto generation = mac->getMib()->getVhtAssociationGeneration(peer);
    auto entry = isEligibleVhtSu(mode, peer, soundingNsts) && generation > 0 ?
            csiCache.findFresh(peer, mode->getDataMode()->getBandwidth(), generation) : nullptr;
    if (entry != nullptr) {
        auto request = packet->addTagIfAbsent<physicallayer::Ieee80211VhtTransmissionTag>();
        request->setBeamformed(true);
        request->setBeamformingGainDb(entry->beamformingGainDb);
        request->setGroupId(0);
        request->setPartialAid(getPeerAssociationId(peer));
    }
}

void VhtHcf::transmitFrame(Packet *packet, simtime_t ifs)
{
    auto muTxop = frameSequenceHandler == nullptr ? nullptr :
            dynamic_cast<const VhtDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence());
    if (muTxop != nullptr && muTxop->isContainerPacket(packet)) {
        exchangeCoordinator.beginTransmission(packet);
        auto header = makeShared<Ieee80211DataHeader>();
        header->setType(ST_DATA_WITH_QOS);
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        header->setTransmitterAddress(mac->getAddress());
        header->setAddress3(mac->getMib()->getBssid());
        tx->transmitFrame(packet, header, ifs, this);
        return;
    }
    if (isVhtNdp(packet)) {
        exchangeCoordinator.beginTransmission(packet);
        auto header = makeShared<Ieee80211DataHeader>();
        header->setType(ST_QOS_NULL);
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        header->setTransmitterAddress(mac->getAddress());
        header->setAddress3(mac->getMib()->getBssid());
        tx->transmitFrame(packet, header, ifs, this);
        return;
    }
    if (dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(packet->peekAtFront()) != nullptr) {
        exchangeCoordinator.beginTransmission(packet);
        auto header = packet->peekAtFront<Ieee80211VhtNdpAnnouncementFrame>();
        packet->addTagIfAbsent<physicallayer::Ieee80211ModeReq>()->setMode(
                modeSet->getSlowestMandatoryMode(MHz(20)));
        tx->transmitFrame(packet, header, ifs, this);
        return;
    }
    auto header = packet->peekAtFront<Ieee80211MacHeader>();
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(header) != nullptr ||
            dynamicPtrCast<const Ieee80211BlockAckReq>(header) != nullptr ||
            dynamicPtrCast<const Ieee80211BlockAck>(header) != nullptr)
        packet->addTagIfAbsent<physicallayer::Ieee80211ModeReq>()->setMode(
                modeSet->getSlowestMandatoryMode(MHz(20)));
    Hcf::transmitFrame(packet, ifs);
}

void VhtHcf::originatorProcessTransmittedFrame(Packet *packet)
{
    auto muTxop = frameSequenceHandler == nullptr ? nullptr :
            dynamic_cast<const VhtDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence());
    if (muTxop != nullptr && muTxop->isContainerPacket(packet)) {
        auto edcaf = edca->getChannelOwner();
        ASSERT(edcaf != nullptr);
        for (const auto& user : muTxop->getActiveUsers()) {
            for (auto packet : user.packets) {
                auto header = packet->peekAtFront<Ieee80211DataHeader>();
                originatorProcessTransmittedDataFrame(packet, header,
                        edcaf->getAccessCategory());
                edcaf->getAckHandler()->transitionToWaitingForBlockAck(header);
            }
        }
        return;
    }
    if (isVhtNdp(packet) ||
            dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(packet->peekAtFront()) != nullptr)
        return;
    Hcf::originatorProcessTransmittedFrame(packet);
}

void VhtHcf::originatorProcessReceivedFrame(Packet *packet,
        Packet *lastTransmittedPacket)
{
    if (isVhtNdp(lastTransmittedPacket) && containsVhtFeedback(packet))
        return;
    Hcf::originatorProcessReceivedFrame(packet, lastTransmittedPacket);
}

void VhtHcf::transmissionComplete(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header)
{
    if (header != nullptr && header->getType() == ST_NOACKACTION &&
            containsVhtFeedback(packet))
        return;
    Hcf::transmissionComplete(packet, header);
}

void VhtHcf::invalidatePeerDerivedState(const MacAddress& peer)
{
    Hcf::invalidatePeerDerivedState(peer);
    csiCache.invalidatePeer(peer);
    soundingCoordinator->invalidatePeer(peer);
    groupIdManager->invalidatePeer(peer);
}

void VhtHcf::localVhtGroupMembershipChanged(
        const std::optional<IVhtGroupIdManager::Membership>& membership)
{
    if (vhtRadio == nullptr || !membership.has_value()) {
        if (vhtRadio != nullptr)
            vhtRadio->setVhtMuRxSelection({});
        return;
    }
    vhtRadio->setVhtMuRxSelection({true, membership->groupId,
            membership->userPosition, membership->channelWidth});
}

} // namespace ieee80211
} // namespace inet
