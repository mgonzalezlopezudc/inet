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
    std::vector<MacAddress> peers;
    auto mib = mac->getMib();
    if (!enableVhtDlMuMimo || mib->bssStationData.stationType != Ieee80211Mib::ACCESS_POINT)
        return peers;
    for (const auto& station : mib->bssAccessPointData.stations) {
        auto negotiated = mib->findNegotiatedVhtCapabilities(station.first);
        if (station.second != Ieee80211Mib::ASSOCIATED || negotiated == nullptr ||
                !negotiated->localTxPeerRx.valid || !negotiated->localTxPeerRx.muMimo ||
                !negotiated->localTxPeerRx.supportedChannelWidths.count(MHz(20)) ||
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[0] < 0 ||
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[1] >= 0)
            continue;
        peers.push_back(station.first);
    }
    std::sort(peers.begin(), peers.end());
    if (peers.size() != 2)
        peers.clear();
    return peers;
}

VhtDlMuTxOpFs *VhtHcf::createVhtDlMuTxOpFs(const VhtDlMuPlan& plan,
        IAckHandler *ackHandler)
{
    return new VhtDlMuTxOpFs(plan, modeSet, ackHandler, this);
}

bool VhtHcf::tryStartVhtDlMu(AccessCategory ac)
{
    if (!enableVhtDlMuMimo || vhtRadio == nullptr ||
            vhtRadio->getVhtChannelWidth() != MHz(20) ||
            vhtRadio->getVhtAntennaCount() != 2 ||
            mac->getMib()->bssStationData.stationType != Ieee80211Mib::ACCESS_POINT)
        return false;
    auto peers = getConstrainedVhtMuPeers();
    if (peers.size() != 2)
        return false;
    auto edcaf = edca->getEdcaf(ac);
    auto queue = edcaf->getPendingQueue();
    IIeee80211VhtDlMuScheduler::Context context;
    context.enabled = true;
    context.accessPoint = true;
    context.packetLevelRadio = true;
    context.channelWidth = MHz(20);
    context.transmitDimensions = 2;
    context.groupId = 1;
    for (int i = 0; i < queue->getNumPackets(); ++i) {
        auto packet = queue->getPacket(i);
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || header->getType() != ST_DATA_WITH_QOS)
            continue;
        auto peerIt = std::find(peers.begin(), peers.end(), header->getReceiverAddress());
        if (peerIt == peers.end())
            continue;
        auto peer = *peerIt;
        auto position = std::distance(peers.begin(), peerIt);
        auto negotiated = mac->getMib()->findNegotiatedVhtCapabilities(peer);
        auto generation = mac->getMib()->getVhtAssociationGeneration(peer);
        auto csi = csiCache.findFresh(peer, MHz(20), generation);
        IIeee80211VhtDlMuScheduler::Candidate candidate;
        candidate.peer = peer;
        candidate.associationId = getPeerAssociationId(peer);
        candidate.tid = header->getTid();
        candidate.associationGeneration = generation;
        candidate.userPosition = position;
        candidate.mcs = -1;
        candidate.ldpc = false;
        if (negotiated != nullptr) {
            auto maxMcs = negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[0];
            for (int mcs = maxMcs; mcs >= 0; --mcs)
                if (modeSet->findVhtMode(mcs, 1, MHz(20), false) != nullptr) {
                    candidate.mcs = mcs;
                    break;
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
        candidate.associated = isAssociatedPeer(peer);
        candidate.negotiatedMuMimo = negotiated != nullptr &&
                negotiated->localTxPeerRx.valid && negotiated->localTxPeerRx.muMimo;
        candidate.exactlyOneSpatialStream = negotiated != nullptr &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[0] >= 0 &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[1] < 0;
        candidate.freshCsi = csi != nullptr;
        candidate.activeGroup = groupIdManager->isActive(peer, 1, position,
                generation, MHz(20));
        candidate.activeBlockAckAgreement = hasActiveOriginatorBlockAckAgreement(
                originatorBlockAckAgreementHandler, peer, header->getTid());
        candidate.unsegmented = header->getFragmentNumber() == 0 &&
                !header->getMoreFragments();
        context.candidates.push_back(candidate);
    }
    auto selected = dlMuScheduler->schedule(context);
    VhtDlMuPlanDiagnostic diagnostic;
    auto plan = VhtDlMuPlan::create(context, selected, diagnostic);
    if (!plan)
        return false;
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
    if (mib->bssStationData.stationType == Ieee80211Mib::ACCESS_POINT) {
        auto it = mib->bssAccessPointData.stations.find(peer);
        return it != mib->bssAccessPointData.stations.end() &&
                it->second == Ieee80211Mib::ASSOCIATED;
    }
    return mib->bssStationData.isAssociated && mib->bssData.bssid == peer;
}

uint16_t VhtHcf::getPeerAssociationId(const MacAddress& peer) const
{
    auto mib = mac->getMib();
    return mib->bssStationData.stationType == Ieee80211Mib::ACCESS_POINT ?
            mib->getAssociationId(peer) : mib->bssStationData.associationId;
}

bool VhtHcf::isEligibleVhtSu(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peer, int& soundingNsts) const
{
    soundingNsts = 0;
    if (!enableVhtSuBeamforming || mode == nullptr || !modeSet->containsMode(mode) ||
            modeSet->getPhyFamily(mode) != physicallayer::Ieee80211PhyFamily::VHT ||
            peer.isMulticast() || mode->getDataMode()->getBandwidth() != MHz(20) ||
            !isAssociatedPeer(peer))
        return false;
    auto negotiated = mac->getMib()->findNegotiatedVhtCapabilities(peer);
    if (negotiated == nullptr || !negotiated->localTxPeerRx.valid ||
            !negotiated->localTxPeerRx.suBeamforming ||
            negotiated->localTxPeerRx.soundingNsts < 2)
        return false;
    soundingNsts = negotiated->localTxPeerRx.soundingNsts;
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
    if (enableVhtDlMuMimo && lastEffectiveChannelWidth == MHz(20)) {
        auto peers = getConstrainedVhtMuPeers();
        for (size_t position = 0; position < peers.size(); ++position) {
            auto generation = mac->getMib()->getVhtAssociationGeneration(peers[position]);
            auto state = groupIdManager->getState(peers[position], 1,
                    generation, MHz(20));
            if (state == IVhtGroupIdManager::State::ABSENT) {
                auto txop = edca->getEdcaf(ac)->getTxopProcedure();
                if (!txop->isProtectionConfigured())
                    txop->configureProtection(TxopProcedure::InitialProtection::NONE);
                auto sequence = new VhtGroupIdManagementFs(mac->getMib(),
                        groupIdManager, peers[position], 1, position,
                        generation, MHz(20));
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
        if (isEligibleVhtSu(mode, peer, soundingNsts) && generation > 0 &&
                csiCache.findFresh(peer, MHz(20), generation) == nullptr &&
                soundingCoordinator->mayAttempt(peer)) {
            auto ndpMode = modeSet->getVhtSuNdpMode(mode, std::min(soundingNsts, 2));
            auto sequence = new VhtSoundingFs(mac->getMib(), &csiCache, peer,
                    getPeerAssociationId(peer), generation,
                    nextDialogToken++ & 0x3f, std::min(soundingNsts, 2), modeSet,
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
            this, enableVhtSuBeamforming);
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
        auto negotiated = mib->findNegotiatedVhtCapabilities(peer);
        bool permitted = enableVhtDlMuMimo &&
                mib->bssStationData.stationType == Ieee80211Mib::STATION &&
                mib->bssStationData.isAssociated && mib->bssData.bssid == peer &&
                negotiated != nullptr && negotiated->localRxPeerTx.valid &&
                negotiated->localRxPeerTx.muMimo &&
                vhtRadio != nullptr && vhtRadio->getVhtChannelWidth() == MHz(20);
        if (!permitted || !groupIdManager->consume(peer, group, generation, MHz(20))) {
            groupIdManager->invalidatePeer(peer);
        }
    }
    if (dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(header) != nullptr &&
            soundingCoordinator->processNdpAnnouncement(packet, header, mac,
                    enableVhtSuBeamforming, mac->getMib()->vhtOperation.operatingChannelWidth))
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
            csiCache.findFresh(peer, MHz(20), generation) : nullptr;
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
        auto header = makeShared<Ieee80211DataHeader>();
        header->setType(ST_DATA_WITH_QOS);
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        header->setTransmitterAddress(mac->getAddress());
        header->setAddress3(mac->getMib()->bssData.bssid);
        tx->transmitFrame(packet, header, ifs, this);
        return;
    }
    if (isVhtNdp(packet)) {
        auto header = makeShared<Ieee80211DataHeader>();
        header->setType(ST_QOS_NULL);
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        header->setTransmitterAddress(mac->getAddress());
        header->setAddress3(mac->getMib()->bssData.bssid);
        tx->transmitFrame(packet, header, ifs, this);
        return;
    }
    if (dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(packet->peekAtFront()) != nullptr) {
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
            auto header = user.packet->peekAtFront<Ieee80211DataHeader>();
            originatorProcessTransmittedDataFrame(user.packet, header,
                    edcaf->getAccessCategory());
            edcaf->getAckHandler()->transitionToWaitingForBlockAck(header);
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
