//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"

#include <algorithm>
#include <cmath>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211SubtypeTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/common/packet/chunk/SequenceChunk.h"

namespace inet {
namespace ieee80211 {

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

HeSoundingFs::HeSoundingFs(Ieee80211Mib *mib,
                           const std::vector<TargetSta>& targets,
                           physicallayer::Ieee80211ModeSet *modeSet,
                           HeMuMimoCsiManager *csiManager,
                           Hz bandwidth,
                           uint8_t dialogToken,
                           uint32_t triggerId) :
    mib(mib),
    targets(targets),
    modeSet(modeSet),
    csiManager(csiManager),
    bandwidth(bandwidth),
    dialogToken(dialogToken),
    triggerId(triggerId)
{
    ASSERT(mib != nullptr);
    ASSERT(modeSet != nullptr);
    ASSERT(csiManager != nullptr);
    ASSERT(!std::isnan(bandwidth.get()) && bandwidth > Hz(0));
    for (const auto& target : targets) {
        ASSERT(target.aid != 0);
        ASSERT(target.maxNss > 0);
    }
    apAddress = mib->address;
}

void HeSoundingFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    ASSERT(context != nullptr);
    this->firstStep = firstStep;
    step = 0;
    EV_INFO << "Starting HE sounding exchange: targets=" << targets.size()
            << ", dialogToken=" << (int)dialogToken
            << ", triggerId=" << triggerId << "\n";
}

IFrameSequenceStep *HeSoundingFs::prepareStep(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    if (targets.empty())
        return nullptr;

    // IEEE 802.11-2024 26.7.3 HE TB sounding sequence:
    // HE NDP Announcement, HE sounding NDP, BFRP Trigger, then HE TB feedback.
    switch (step) {
        case 0: {
            auto ndpa = buildNdpaFrame(context);
            return new TransmitStep(ndpa, modeSet->getSifsTime(), true);
        }
        case 1: {
            auto ndp = buildNdpFrame(context);
            return new TransmitStep(ndp, modeSet->getSifsTime(), true);
        }
        case 2: {
            auto trigger = buildBfrpTriggerFrame(context);
            return new TransmitStep(trigger, modeSet->getSifsTime(), true);
        }
        case 3: {
            auto txStep = check_and_cast<ITransmitStep *>(context->getLastStep());
            auto trigger = txStep->getFrameToTransmit();
            auto header = trigger->peekAtFront<Ieee80211TriggerFrame>();
            ASSERT(header->getCommonDuration() > SIMTIME_ZERO);
            return new ReceiveCollectionStep(modeSet->getSifsTime() +
                    header->getCommonDuration() + modeSet->getSlotTime());
        }
        case 4:
            return nullptr;
        default:
            throw cRuntimeError("Invalid HE sounding step: %d", step);
    }
}

bool HeSoundingFs::completeStep(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    switch (step) {
        case 0:
        case 1:
        case 2:
            step++;
            return true;
        case 3:
            processFeedbacks(context);
            step++;
            return true;
        default:
            throw cRuntimeError("Invalid HE sounding step in completeStep: %d", step);
    }
}

Packet *HeSoundingFs::buildNdpaFrame(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    auto ndpaBody = makeShared<Ieee80211HeNdpAnnouncement>();
    ndpaBody->setDialogToken(dialogToken);
    ndpaBody->setStationsArraySize(targets.size());
    for (size_t i = 0; i < targets.size(); ++i) {
        Ieee80211HeNdpStaInfo info;
        info.aid = targets[i].aid;
        info.feedbackType = 1; // MU
        info.nc = targets[i].maxNss;
        ndpaBody->setStations(i, info);
    }
    // Body chunk length: Cat(1) + HEAction(1) + DialogToken(1) + 2 bytes per STA
    // — matches §9.6.28.4 Table 9-100 Per-STA Info (AID11 | FeedbackTypeAndNg | Disambiguation).
    ndpaBody->setChunkLength(B(3 + targets.size() * 2));

    // Use Ieee80211ActionFrame (category 30 = HE) so the protocol dissector at
    // Ieee80211MacProtocolDissector.cc:246 recognises it as an Action frame and
    // routes the body chunk directly to Ieee80211HeSoundingMgmtFrameSerializer.
    auto header = makeShared<Ieee80211ActionFrame>();
    header->setType(ST_ACTION);
    header->setCategory(30);   // HE category (IEEE 802.11-2024 Table 9-51)
    header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    header->setTransmitterAddress(apAddress);
    header->setAddress3(apAddress); // BSSID

    auto packet = new Packet("HE-NDPA", header);
    packet->insertAtBack(ndpaBody);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

Packet *HeSoundingFs::buildNdpFrame(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    auto ndpPacket = new Packet("HE-NDP");
    auto txTag = ndpPacket->addTagIfAbsent<physicallayer::Ieee80211HeMuTxTag>();
    txTag->setNdp(true);

    uint16_t ruToneSize = 242;
    if (bandwidth >= Hz(160e6)) ruToneSize = 1992;
    else if (bandwidth >= Hz(80e6)) ruToneSize = 996;
    else if (bandwidth >= Hz(40e6)) ruToneSize = 484;

    int totalNsts = 0;
    for (const auto& target : targets)
        totalNsts += target.maxNss;
    ASSERT(totalNsts > 0);

    int streamStartIndex = 0;
    for (size_t i = 0; i < targets.size(); ++i) {
        physicallayer::Ieee80211HeMuTxAllocationInfo allocation;
        allocation.ruIndex = 0;
        allocation.ruToneSize = ruToneSize;
        allocation.ruToneOffset = 0;
        allocation.staId = targets[i].aid;
        allocation.mcs = 0;
        allocation.numberOfSpatialStreams = targets[i].maxNss;
        allocation.dcm = false;
        allocation.psduLength = B(0);
        allocation.streamStartIndex = streamStartIndex;
        allocation.muMimo = true;
        allocation.totalNsts = totalNsts;
        txTag->appendAllocations(allocation);
        streamStartIndex += targets[i].maxNss;
    }

    auto commonRequest = ndpPacket->addTagIfAbsent<physicallayer::Ieee80211HeMuCommonReq>();
    commonRequest->setGuardInterval(physicallayer::HE_GI_3_2_US);
    commonRequest->setCoding(ruToneSize >= 484 ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC);
    commonRequest->setPacketExtensionDurationUs(0);
    commonRequest->setPuncturedSubchannelMask(0);

    return ndpPacket;
}

Packet *HeSoundingFs::buildBfrpTriggerFrame(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    auto header = makeShared<Ieee80211TriggerFrame>();
    // 9.3.1.22 Table 9-47: Trigger type 1 is BFRP.  9.3.1.22.3 says the
    // BFRP Trigger-dependent User Info field is the feedback segment bitmap.
    header->setTriggerType(1);
    header->setTriggerId(triggerId);
    header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    header->setTransmitterAddress(apAddress);
    header->setChannelBandwidthMhz(std::lround(bandwidth.get() / 1e6));
    header->setUsersArraySize(targets.size());

    int count = targets.size();
    int layoutCount = 1;
    if (count > 4) layoutCount = 8;
    else if (count > 2) layoutCount = 4;
    else if (count > 1) layoutCount = 2;

    auto ruLayout = physicallayer::getHeEqualRuLayout(Hz(0), bandwidth, layoutCount);

    bool requiresLdpc = std::any_of(ruLayout.begin(), ruLayout.end(), [] (const auto& ru) {
        return ru.toneSize >= 484;
    });
    header->setCoding(requiresLdpc ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC);
    header->setGuardInterval(physicallayer::HE_GI_1_6_US);
    header->setLtfType(physicallayer::HE_LTF_2X);

    // A BFRP Trigger is addressed only to users for which the selected
    // equal-sized RU layout has an explicit RU. Keep this assertion next to
    // the indexing below to turn configuration mistakes into local failures.
    ASSERT(targets.size() <= ruLayout.size());

    std::vector<physicallayer::Ieee80211HeUserPhyParameters> responseUsers;
    responseUsers.reserve(targets.size());
    for (size_t i = 0; i < targets.size(); ++i) {
        Ieee80211HeTriggerUserInfo user;
        user.aid = targets[i].aid;
        user.ruIndex = ruLayout[i].index;
        user.ruToneSize = ruLayout[i].toneSize;
        user.ruToneOffset = ruLayout[i].toneOffset;
        user.mcs = 0;
        user.coding = requiresLdpc ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
        user.targetRssiDbm = -60;
        header->setUsers(i, user);

        physicallayer::Ieee80211HeUserPhyParameters responseUser;
        responseUser.ru = ruLayout[i];
        responseUser.mcs = user.mcs;
        responseUser.numberOfSpatialStreams = user.numberOfSpatialStreams;
        responseUser.streamStartIndex = user.streamStartIndex;
        responseUser.staId = user.aid;
        responseUser.coding = requiresLdpc ? physicallayer::HE_CODING_LDPC :
                physicallayer::HE_CODING_BCC;
        // The feedback MPDU is 34 bytes: 24 (MAC hdr) + 6 (§9.6.28.2 body) + 4 (FCS).
        responseUser.psduLength = B(34);
        responseUsers.push_back(responseUser);
    }
    auto heMode = modeSet != nullptr && modeSet->getNumModes() > 0 ?
            dynamic_cast<const physicallayer::Ieee80211HeMode *>(modeSet->getMode(0)) : nullptr;
    if (heMode == nullptr)
        throw cRuntimeError("Cannot select HE BFRP signal extension without an HE operating band");
    header->setNoSignalExtension(false);
    physicallayer::Ieee80211HeTriggerResponseFinalizationRequest request;
    request.users = responseUsers;
    request.centerFrequency = heMode->getCenterFrequencyMode() ==
            physicallayer::Ieee80211HeMode::BAND_2_4GHZ ? Hz(2.4e9) : Hz(5e9);
    request.channelBandwidth = bandwidth;
    request.guardInterval = physicallayer::HE_GI_1_6_US;
    request.ltfType = physicallayer::HE_LTF_2X;
    request.noSignalExtension = header->getNoSignalExtension();
    auto finalization = physicallayer::finalizeHeTriggerResponse(request);
    if (!finalization)
        throw cRuntimeError("Cannot finalize BFRP Trigger response: %s", finalization.error.c_str());
    header->setUlLength(finalization.ulLength);
    header->setNumberOfHeLtfSymbols(finalization.parameters.common.numberOfHeLtfSymbols);
    header->setPreFecPaddingFactor(finalization.parameters.common.preFecPaddingFactor);
    header->setLdpcExtraSymbolSegment(finalization.parameters.common.ldpcExtraSymbol);
    header->setPeDisambiguity(finalization.peDisambiguity);
    header->setPacketExtensionDurationUs(finalization.parameters.common.packetExtensionDurationUs);
    header->setCommonDuration(finalization.commonDuration);
    header->setCommonDurationExact(finalization.commonDurationExact);
    header->setDurationField(modeSet->getSifsTime() + finalization.commonDuration);
    header->setChunkLength(B(24 + 6 * targets.size()));

    auto packet = new Packet("HE-BFRP-Trigger", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    EV_INFO << "Built HE BFRP Trigger " << triggerId
            << " for " << targets.size() << " sounding targets"
            << " using " << layoutCount << " RUs and "
            << (requiresLdpc ? "LDPC" : "BCC") << " coding\n";
    return packet;
}

void HeSoundingFs::processFeedbacks(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    auto collection = check_and_cast<ReceiveCollectionStep *>(context->getLastStep());
    std::vector<MacAddress> allAssociatedStations;
    for (const auto& entry : mib->bssAccessPointData.associationIds) {
        allAssociatedStations.push_back(entry.first);
    }
    auto getAid = [this](const MacAddress& addr) {
        return mib->getAssociationId(addr);
    };

    int acceptedFeedbacks = 0;
    int ignoredFeedbacks = 0;
    for (auto packet : collection->getReceivedFrames()) {
        auto header = packet->peekAtFront<Ieee80211MgmtHeader>();
        auto feedback = findPacketChunk<Ieee80211HeCompressedBeamformingFeedback>(packet);
        if (header == nullptr || feedback == nullptr) {
            ignoredFeedbacks++;
            continue;
        }
        if (feedback->getDialogToken() == dialogToken && feedback->getValid()) {
            MacAddress staAddress = header->getTransmitterAddress();
            csiManager->updateCsi(staAddress, bandwidth, allAssociatedStations, getAid);
            acceptedFeedbacks++;
        }
        else
            ignoredFeedbacks++;
    }
    EV_INFO << "Completed HE sounding feedback collection: accepted=" << acceptedFeedbacks
            << ", ignored=" << ignoredFeedbacks
            << ", triggerId=" << triggerId << "\n";
}

} // namespace ieee80211
} // namespace inet
