//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"

namespace inet {
namespace ieee80211 {

Define_Module(HeSoundingCoordinator);

void HeSoundingCoordinator::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        WATCH(ndpAnnouncementReceived);
        WATCH(ndpReceived);
        WATCH(soundingDialogToken);
        WATCH(nextSoundingDialogToken);
        WATCH(nextTriggerId);
        WATCH_VECTOR(soundingTargets);
        WATCH_EXPR("soundingTargetCount", soundingTargets.size());
        WATCH_EXPR("soundingState", ndpAnnouncementReceived ? (ndpReceived ? "NDP received" : "NDPA received") : "idle");
    }
}

namespace {

template <typename T>
Ptr<const T> findPacketChunk(const Packet *packet)
{
    auto data = packet->peekData();
    if (auto chunk = dynamicPtrCast<const T>(data))
        return chunk;
    if (auto sequence = dynamicPtrCast<const SequenceChunk>(data)) {
        for (const auto& chunk : sequence->getChunks())
            if (auto result = dynamicPtrCast<const T>(chunk))
                return result;
    }
    return nullptr;
}

} // namespace

bool HeSoundingCoordinator::tryStartSoundingSequence(AccessCategory ac,
                              const IIeee80211HeDlScheduler::ScheduleContext& scheduleContext,
                              IFrameSequenceHandler *frameSequenceHandler,
                              Ieee80211Mac *mac,
                              physicallayer::Ieee80211ModeSet *modeSet,
                              HeMuMimoCsiManager& csiManager,
                              FrameSequenceContext *context,
                              HeHcf *hcf)
{
    int lackCsiCount = 0;
    std::vector<HeSoundingFs::TargetSta> soundingStas;
    for (const auto& candidate : scheduleContext.candidates) {
        auto negotiated = candidate.negotiatedHeCapabilities;
        if (negotiated == nullptr || !negotiated->valid)
            continue;
        auto mib = mac->getMib();
        auto it = mib->bssAccessPointData.advertisedHeCapabilities.find(candidate.staAddress);
        const Ieee80211HeCapabilities *staCapabilities = it != mib->bssAccessPointData.advertisedHeCapabilities.end() ? &it->second : nullptr;
        if (staCapabilities == nullptr)
            continue;
        if (isDlMuMimoEligible(mac->getMib()->localHeCapabilities, *staCapabilities, *negotiated, scheduleContext.channelBandwidth, scheduleContext.numApAntennas)) {
            if (!csiManager.hasFreshCsi(candidate.staAddress, scheduleContext.channelBandwidth))
                lackCsiCount++;
            HeSoundingFs::TargetSta target;
            target.address = candidate.staAddress;
            target.aid = mac->getMib()->getAssociationId(candidate.staAddress);
            target.maxNss = std::min(getMaxNss(negotiated->intersection.txMcsNss), 4);
            soundingStas.push_back(target);
        }
    }
    if (lackCsiCount >= 2) {
        if (soundingStas.size() > 8)
            soundingStas.resize(8);
        EV_INFO << "At least two MU-capable backlogged STAs lack fresh CSI. Initiating sounding sequence." << std::endl;
        auto soundingSequence = new HeSoundingFs(mac->getMib(), soundingStas, modeSet,
                                                 &csiManager, scheduleContext.channelBandwidth,
                                                 nextSoundingDialogToken++, nextTriggerId++);
        frameSequenceHandler->startFrameSequence(soundingSequence, context, hcf);
        hcf->emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
        return true;
    }
    return false;
}

bool HeSoundingCoordinator::processSoundingFrame(Packet *packet,
                          const Ptr<const Ieee80211MacHeader>& header,
                          Ieee80211Mac *mac,
                          physicallayer::Ieee80211ModeSet *modeSet,
                          HeMuMimoCsiManager& csiManager,
                          ITx *tx,
                          ITx::ICallback *callback)
{
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(header) && header->getType() == ST_ACTION) {
        if (auto ndpa = findPacketChunk<Ieee80211HeNdpAnnouncement>(packet)) {
            soundingTargets.clear();
            ndpAnnouncementReceived = false;
            ndpReceived = false;
            soundingDialogToken = ndpa->getDialogToken();

            auto myAid = mac->getMib()->bssStationData.associationId;
            bool targeted = false;
            if (myAid > 0) {
                for (unsigned int i = 0; i < ndpa->getStationsArraySize(); ++i) {
                    const auto& staInfo = ndpa->getStations(i);
                    if (staInfo.aid == myAid) {
                        targeted = true;
                    }
                    SoundingTarget target;
                    target.aid = staInfo.aid;
                    target.maxNss = staInfo.nc;
                    soundingTargets.push_back(target);
                }
            }
            if (targeted) {
                ndpAnnouncementReceived = true;
            }
            delete packet;
            return true;
        }

        if (auto feedback = findPacketChunk<Ieee80211HeCompressedBeamformingFeedback>(packet)) {
            if (feedback->getValid()) {
                std::vector<MacAddress> allAssociatedStations;
                for (const auto& entry : mac->getMib()->bssAccessPointData.associationIds) {
                    allAssociatedStations.push_back(entry.first);
                }
                auto getAid = [mac](const MacAddress& addr) {
                    return mac->getMib()->getAssociationId(addr);
                };
                auto radio = check_and_cast<physicallayer::IRadio *>(getContainingNicModule(this)->getSubmodule("radio"));
                auto transmitter = check_and_cast<const physicallayer::Ieee80211Transmitter *>(radio->getTransmitter());
                Hz bw = Hz(20e6);
                if (transmitter && transmitter->getMode()) {
                    bw = transmitter->getMode()->getDataMode()->getBandwidth();
                }
                auto twoAddrHeader = dynamicPtrCast<const Ieee80211TwoAddressHeader>(header);
                if (twoAddrHeader != nullptr) {
                    csiManager.updateCsi(twoAddrHeader->getTransmitterAddress(), bw, allAssociatedStations, getAid);
                }
            }
            delete packet;
            return true;
        }
    }

    if (auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(header)) {
        // IEEE 802.11-2024 9.3.1.22 Table 9-47 defines BFRP as Trigger type 1.
        // In the HE TB sounding sequence (26.7.3), the BFRP Trigger follows
        // NDPA and NDP and solicits HE compressed beamforming/CQI feedback in
        // an HE TB PPDU.
        if (trigger->getTriggerType() == 1) {
            if (ndpAnnouncementReceived && ndpReceived) {
                auto myAid = mac->getMib()->bssStationData.associationId;
                const Ieee80211HeTriggerUserInfo *selected = nullptr;
                for (unsigned int i = 0; i < trigger->getUsersArraySize(); ++i) {
                    if (trigger->getUsers(i).aid == myAid) {
                        selected = &trigger->getUsers(i);
                        break;
                    }
                }
                if (selected != nullptr) {
                    int maxNss = 1;
                    auto negotiated = mac->getMib()->findNegotiatedHeCapabilities(trigger->getTransmitterAddress());
                    if (negotiated != nullptr && negotiated->valid) {
                        maxNss = std::min(getMaxNss(negotiated->intersection.txMcsNss), 4);
                    }

                    auto feedback = makeShared<Ieee80211HeCompressedBeamformingFeedback>();
                    feedback->setDialogToken(soundingDialogToken);
                    feedback->setAid(myAid);
                    feedback->setFeedbackBandwidth(20e6);
                    feedback->setNc(maxNss);
                    feedback->setNr(mac->getMib()->localHeCapabilities.soundingDimensions);
                    feedback->setValid(true);
                    // Body chunk length: Cat(1) + HEAction(1) + DialogToken(1) + MIMO Control(3)
                    // — matches §9.6.28.2 minimum body per Table 9-99.
                    feedback->setChunkLength(B(6));

                    // Use Ieee80211ActionFrame (category 30 = HE) so the protocol
                    // dissector routes the body to Ieee80211HeSoundingMgmtFrameSerializer.
                    auto responseHeader = makeShared<Ieee80211ActionFrame>();
                    responseHeader->setType(ST_ACTION);
                    responseHeader->setCategory(30);   // HE category
                    responseHeader->setReceiverAddress(trigger->getTransmitterAddress());
                    responseHeader->setTransmitterAddress(mac->getAddress());
                    responseHeader->setAddress3(trigger->getTransmitterAddress()); // BSSID

                    auto response = new Packet("HE-Feedback", responseHeader);
                    response->insertAtBack(feedback);
                    response->insertAtBack(makeShared<Ieee80211MacTrailer>());

                    auto request = response->addTagIfAbsent<physicallayer::Ieee80211HeMuReq>();
                    request->setPpduFormat(physicallayer::HE_TRIGGER_BASED_UPLINK);
                    request->setTriggerId(trigger->getTriggerId());
                    request->setRuIndex(selected->ruIndex);
                    request->setRuToneSize(selected->ruToneSize);
                    request->setRuToneOffset(selected->ruToneOffset);
                    request->setStaId(myAid);
                    request->setMcs(selected->mcs);
                    request->setCommonDuration(trigger->getCommonDuration());

                    tx->transmitFrame(response, responseHeader, modeSet->getSifsTime(), callback);
                    delete response;
                }
                ndpAnnouncementReceived = false;
                ndpReceived = false;
                delete packet;
                return true;
            }
        }
    }

    return false;
}

} // namespace ieee80211
} // namespace inet
