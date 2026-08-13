// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingService.h"

#include <algorithm>

#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/common/FcsMode_m.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"

namespace inet {
namespace ieee80211 {

namespace {

template <typename T>
Ptr<const T> findPacketChunk(const Packet *packet)
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

} // namespace

std::optional<HeSoundingService::StartAction> HeSoundingService::prepareSounding(
        const IIeee80211HeDlScheduler::ScheduleContext& snapshot,
        const std::function<uint16_t(const MacAddress&)>& getAssociationId)
{
    std::vector<HeSoundingFs::TargetSta> freshTargets;
    std::vector<HeSoundingFs::TargetSta> staleTargets;
    for (const auto& candidate : snapshot.candidates) {
        if (!candidate.hasNegotiatedHeCapabilities ||
                !candidate.negotiatedHeCapabilities.localTxPeerRx.valid ||
                !candidate.hasAdvertisedHeCapabilities)
            continue;
        if (!isDlMuMimoEligible(snapshot.localHeCapabilities,
                candidate.advertisedHeCapabilities,
                candidate.negotiatedHeCapabilities, snapshot.channelBandwidth,
                snapshot.numApAntennas))
            continue;
        HeSoundingFs::TargetSta target;
        target.address = candidate.staAddress;
        target.aid = getAssociationId(candidate.staAddress);
        target.maxNss = std::min(getMaxNss(
                candidate.negotiatedHeCapabilities.localTxPeerRx.mcsNss), 4);
        (candidate.hasFreshCsi ? freshTargets : staleTargets).push_back(target);
    }
    if (staleTargets.empty())
        return std::nullopt;

    // IEEE Std 802.11-2024, 26.7.3: sounding establishes the channel-state
    // information used for beamformed HE transmissions. Stale peers are
    // ordered first so the model's eight-peer exchange cap cannot starve them.
    staleTargets.insert(staleTargets.end(), freshTargets.begin(), freshTargets.end());
    if (staleTargets.size() > 8)
        staleTargets.resize(8);
    StartAction action;
    action.targets = std::move(staleTargets);
    action.channelCenterFrequency = snapshot.channelCenterFrequency;
    action.channelBandwidth = snapshot.channelBandwidth;
    return action;
}

std::optional<HeSoundingService::StartAction> HeSoundingService::prepareSounding(
        const HcfHeSoundingSnapshot& snapshot)
{
    if (!snapshot.isComplete())
        return std::nullopt;
    std::vector<HeSoundingFs::TargetSta> staleTargets;
    std::vector<HeSoundingFs::TargetSta> freshTargets;
    for (const auto& candidate : snapshot.candidates) {
        if (!candidate.eligible || candidate.address.isUnspecified() ||
                candidate.associationId == 0)
            continue;
        HeSoundingFs::TargetSta target {candidate.address,
                candidate.associationId, candidate.maximumSpatialStreams};
        (candidate.hasFreshCsi ? freshTargets : staleTargets).push_back(target);
    }
    if (staleTargets.empty())
        return std::nullopt;
    staleTargets.insert(staleTargets.end(), freshTargets.begin(), freshTargets.end());
    if (staleTargets.size() > 8)
        staleTargets.resize(8);
    StartAction action;
    action.targets = std::move(staleTargets);
    action.channelCenterFrequency = snapshot.channelCenterFrequency;
    action.channelBandwidth = snapshot.channelBandwidth;
    return action;
}

void HeSoundingService::commitPreparedSounding(StartAction action,
        AccessCategory accessCategory)
{
    if (actions == nullptr)
        throw cRuntimeError("HE sounding service has no typed start action");
    action.dialogToken = nextSoundingDialogToken++;
    action.triggerId = allocateIeee80211HeTriggerId();
    actions->startHeSoundingExchange(action, accessCategory);
}

bool HeSoundingService::processNdpIndication(bool isHeSuNdp)
{
    if (!isHeSuNdp)
        return false;
    // IEEE Std 802.11-2024, 26.7.3: the NDP has no PSDU and therefore cannot
    // be correlated through a MAC header or the ordinary ACK/BA path.
    if (ndpAnnouncementReceived)
        ndpReceived = true;
    return true;
}

bool HeSoundingService::processReceivedFrame(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header,
        const ReceiveSnapshot& snapshot, const ReceiveActions& actions)
{
    if (header == nullptr) {
        auto rxVectorInd = packet->findTag<physicallayer::Ieee80211HeRxVectorInd>();
        const bool soundingNdp = rxVectorInd != nullptr &&
                rxVectorInd->getRxVector() != nullptr &&
                rxVectorInd->getRxVector()->getCommon().getPpduFormat() ==
                        physicallayer::HE_SINGLE_USER;
        if (soundingNdp) {
            if (!ndpAnnouncementReceived)
                EV_WARN << "Ignoring HE sounding NDP without a preceding NDPA\n";
            processNdpIndication(true);
            delete packet;
            return true;
        }
    }

    if (dynamicPtrCast<const Ieee80211MgmtHeader>(header) && header->getType() == ST_ACTION) {
        if (auto ndpa = findPacketChunk<Ieee80211HeNdpAnnouncement>(packet)) {
            soundingTargets.clear();
            resetStaState();
            soundingDialogToken = ndpa->getDialogToken();
            bool targeted = false;
            if (snapshot.localAssociationId > 0) {
                for (unsigned int i = 0; i < ndpa->getStationsArraySize(); ++i) {
                    const auto& staInfo = ndpa->getStations(i);
                    targeted |= staInfo.aid == snapshot.localAssociationId;
                    soundingTargets.push_back({{}, staInfo.aid, staInfo.nc});
                }
            }
            ndpAnnouncementReceived = targeted;
            delete packet;
            return true;
        }

        if (auto feedback = findPacketChunk<Ieee80211HeCompressedBeamformingFeedback>(packet)) {
            auto twoAddressHeader = dynamicPtrCast<const Ieee80211TwoAddressHeader>(header);
            if (feedback->getValid() && twoAddressHeader != nullptr && actions.publishCsiUpdate)
                actions.publishCsiUpdate({twoAddressHeader->getTransmitterAddress(),
                        snapshot.linkPhy.getChannelBandwidth()});
            delete packet;
            return true;
        }
    }

    auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(header);
    if (trigger == nullptr || trigger->getTriggerType() != 1)
        return false;

    // IEEE Std 802.11-2024, 9.3.1.22 Table 9-47 and 26.7.3: BFRP follows
    // the correlated NDPA/NDP exchange and solicits HE-TB feedback.
    auto correlation = packet->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>();
    if (correlation == nullptr || correlation->getTriggerId() == 0) {
        EV_WARN << "Ignoring BFRP Trigger without model-only correlation context\n";
        delete packet;
        return true;
    }
    if (!ndpAnnouncementReceived || !ndpReceived)
        return false;

    const Ieee80211HeTriggerUserInfo *selected = nullptr;
    for (unsigned int i = 0; i < trigger->getUsersArraySize(); ++i)
        if (trigger->getUsers(i).aid == snapshot.localAssociationId) {
            selected = &trigger->getUsers(i);
            break;
        }
    if (selected != nullptr) {
        int maxNss = 1;
        auto peer = std::find_if(snapshot.peers.begin(), snapshot.peers.end(),
                [&] (const PeerSnapshot& value) {
                    return value.address == trigger->getTransmitterAddress();
                });
        if (peer != snapshot.peers.end() && peer->negotiatedCapabilities &&
                peer->negotiatedCapabilities->localRxPeerTx.valid)
            maxNss = std::min(getMaxNss(
                    peer->negotiatedCapabilities->localRxPeerTx.mcsNss), 4);

        auto feedback = makeShared<Ieee80211HeCompressedBeamformingFeedback>();
        feedback->setDialogToken(soundingDialogToken);
        feedback->setAid(snapshot.localAssociationId);
        feedback->setFeedbackBandwidth(20e6);
        feedback->setNc(maxNss);
        feedback->setNr(snapshot.soundingDimensions);
        feedback->setValid(true);
        feedback->setChunkLength(B(6)); // IEEE 802.11-2024, 9.6.28.2, Table 9-99.

        auto responseHeader = makeShared<Ieee80211ActionFrame>();
        responseHeader->setType(ST_ACTION);
        responseHeader->setCategory(30);
        responseHeader->setReceiverAddress(trigger->getTransmitterAddress());
        responseHeader->setTransmitterAddress(snapshot.localAddress);
        responseHeader->setAddress3(trigger->getTransmitterAddress());
        auto response = new Packet("HE-Feedback", responseHeader);
        response->insertAtBack(feedback);
        response->insertAtBack(makeShared<Ieee80211MacTrailer>());

        auto protection = attachHeTbTxVectorFromTrigger(response, *trigger, *selected,
                snapshot.localAssociationId, snapshot.linkPhy.getChannelCenterFrequency(),
                snapshot.linkPhy.getMaximumTransmitPower(),
                B((response->getDataLength().get<b>() + 7) / 8), snapshot.bssColor,
                correlation->getTriggerId(), false, 0, 0, 0,
                snapshot.solicitingTxopDuration, snapshot.sifs);
        auto writableHeader = response->removeAtFront<Ieee80211ActionFrame>();
        writableHeader->setDurationField(protection.macDurationField);
        response->insertAtFront(writableHeader);
        auto trailer = response->removeAtBack<Ieee80211MacTrailer>(B(4));
        auto fcsMode = static_cast<FcsMode>(snapshot.fcsMode);
        trailer->setFcsMode(fcsMode);
        if (fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(response, fcsMode));
        response->insertAtBack(trailer);
        if (!actions.transmitResponse)
            throw cRuntimeError("HE sounding response has no typed transmit action");
        actions.transmitResponse(response,
                response->peekAtFront<Ieee80211ActionFrame>(), snapshot.sifs);
        delete response;
    }
    resetStaState();
    delete packet;
    return true;
}

void HeSoundingService::resetStaState()
{
    ndpAnnouncementReceived = false;
    ndpReceived = false;
}

} // namespace ieee80211
} // namespace inet
