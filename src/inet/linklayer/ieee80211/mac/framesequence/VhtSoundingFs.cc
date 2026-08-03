//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/VhtSoundingFs.h"

#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

namespace {

template <typename T>
Ptr<const T> findVhtSoundingChunk(const Packet *packet)
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

VhtSoundingFs::VhtSoundingFs(Ieee80211Mib *mib, VhtCsiCache *csiCache,
        const MacAddress& peer, uint16_t associationId,
        uint64_t associationGeneration, uint8_t dialogToken, int soundingNsts,
        physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *ndpMode,
        double beamformingGainDb) :
    mib(mib), csiCache(csiCache), peer(peer), associationId(associationId),
    associationGeneration(associationGeneration), dialogToken(dialogToken),
    soundingNsts(soundingNsts), modeSet(modeSet), ndpMode(ndpMode),
    beamformingGainDb(beamformingGainDb)
{
    ASSERT(mib != nullptr && csiCache != nullptr && modeSet != nullptr && ndpMode != nullptr);
    ASSERT(!peer.isMulticast() && associationId > 0 && associationGeneration > 0);
    ASSERT(soundingNsts >= 2);
}

void VhtSoundingFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    step = 0;
}

IFrameSequenceStep *VhtSoundingFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            return new TransmitStep(buildNdpAnnouncement(), SIMTIME_ZERO, true);
        case 1:
            return new TransmitStep(buildNdp(), modeSet->getSifsTime(), true);
        case 2: {
            // IEEE Std 802.11-2024, 10.35.5.2: compressed beamforming
            // feedback starts after SIFS. Bound the wait by the selected
            // response PPDU duration plus one slot of propagation/start
            // tolerance instead of a feature-specific millisecond constant.
            expectedFeedbackStart = simTime() + modeSet->getSifsTime();
            feedbackStartTolerance = modeSet->getSlotTime();
            return new ReceiveStep(modeSet->getSifsTime() +
                    modeSet->getSlowestMandatoryMode(MHz(20))->getDuration(B(46)) +
                    feedbackStartTolerance,
                    IReceiveStep::TimeoutHandling::COMPLETE_STEP,
                    [this](Packet *packet, FrameSequenceContext *) { return isExpectedFeedback(packet); },
                    IReceiveStep::UnexpectedResponseHandling::IGNORE_RESPONSE,
                    IFrameSequenceStep::Completion::EXPIRED);
        }
        case 3:
            return nullptr;
        default:
            throw cRuntimeError("Invalid VHT sounding step");
    }
}

bool VhtSoundingFs::completeStep(FrameSequenceContext *context)
{
    if (step == 2) {
        auto receive = check_and_cast<IReceiveStep *>(context->getLastStep());
        // FrameSequenceHandler assigns ACCEPTED after completeStep(); the
        // presence of the validator-accepted response is authoritative here.
        if (receive->getReceivedFrame() != nullptr &&
                isExpectedFeedback(receive->getReceivedFrame()))
            csiCache->update(peer, MHz(20), associationGeneration, beamformingGainDb);
    }
    step++;
    return true;
}

Packet *VhtSoundingFs::buildNdpAnnouncement() const
{
    auto header = makeShared<Ieee80211VhtNdpAnnouncementFrame>();
    header->setReceiverAddress(peer);
    header->setTransmitterAddress(mib->address);
    header->setSoundingDialogTokenNumber(dialogToken);
    header->setStationsArraySize(1);
    Ieee80211VhtNdpStaInfo station;
    station.aid = associationId;
    station.muFeedback = false;
    station.ncIndex = 0;
    header->setStations(0, station);
    header->setChunkLength(B(19));
    auto packet = new Packet("VHT-NDPA", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

Packet *VhtSoundingFs::buildNdp() const
{
    auto packet = new Packet("VHT-NDP");
    packet->addTag<physicallayer::Ieee80211ModeReq>()->setMode(ndpMode);
    auto request = packet->addTag<physicallayer::Ieee80211VhtTransmissionTag>();
    request->setNdp(true);
    request->setGroupId(63);
    request->setPartialAid(associationId);
    return packet;
}

bool VhtSoundingFs::isExpectedFeedback(Packet *packet) const
{
    return matchesFeedback(packet, mib->address, peer, dialogToken,
            expectedFeedbackStart, feedbackStartTolerance);
}

bool VhtSoundingFs::matchesFeedback(Packet *packet,
        const MacAddress& localAddress, const MacAddress& peer,
        uint8_t dialogToken, simtime_t expectedStart,
        simtime_t startTolerance)
{
    if (packet == nullptr)
        return false;
    auto header = dynamicPtrCast<const Ieee80211ActionFrame>(packet->peekAtFront());
    auto feedback = findVhtSoundingChunk<Ieee80211VhtCompressedBeamformingFeedback>(packet);
    auto provenance = packet->findTag<physicallayer::Ieee80211PhyProvenanceInd>();
    bool timely = expectedStart < SIMTIME_ZERO || (provenance != nullptr &&
            provenance->getStartTime() >= expectedStart &&
            provenance->getStartTime() <= expectedStart + startTolerance);
    return timely && header != nullptr && header->getType() == ST_NOACKACTION &&
            header->getCategory() == 21 && header->getReceiverAddress() == localAddress &&
            header->getTransmitterAddress() == peer && feedback != nullptr &&
            feedback->getSoundingDialogTokenNumber() == dialogToken;
}

} // namespace ieee80211
} // namespace inet
