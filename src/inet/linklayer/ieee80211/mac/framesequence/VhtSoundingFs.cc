//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/VhtSoundingFs.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/aggregation/MpduDeaggregation.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

namespace {

static int getVhtFeedbackSubcarrierCount(Hz bandwidth, uint8_t grouping)
{
    return bandwidth == MHz(20) ? (grouping == 1 ? 52 : grouping == 2 ? 30 : 16) :
            bandwidth == MHz(40) ? (grouping == 1 ? 108 : grouping == 2 ? 58 : 30) :
            bandwidth == MHz(80) ? (grouping == 1 ? 234 : grouping == 2 ? 122 : 62) :
            (grouping == 1 ? 468 : grouping == 2 ? 244 : 124);
}

static size_t getVhtFeedbackMatrixBytes(Hz bandwidth, int nr, int nc,
        uint8_t grouping, bool feedbackTypeMu)
{
    int angleBits = feedbackTypeMu ? 6 : 3;
    int angleCount = nc * (2 * nr - nc - 1);
    return (getVhtFeedbackSubcarrierCount(bandwidth, grouping) * angleCount * angleBits + 7) / 8;
}

static size_t getVhtMuExclusiveReportBytes(Hz bandwidth, int nc, uint8_t grouping)
{
    int ns = bandwidth == MHz(20) ? (grouping == 1 ? 30 : grouping == 2 ? 16 : 10) :
            bandwidth == MHz(40) ? (grouping == 1 ? 58 : grouping == 2 ? 30 : 16) :
            bandwidth == MHz(80) ? (grouping == 1 ? 122 : grouping == 2 ? 62 : 32) :
            (grouping == 1 ? 244 : grouping == 2 ? 124 : 64);
    return (ns * nc * 4 + 7) / 8;
}

static constexpr size_t VHT_MAX_FEEDBACK_MPDU_BYTES = 3895 - 24 - 4;

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

template <typename T>
void collectVhtSoundingChunks(const Ptr<const Chunk>& chunk,
        std::vector<Ptr<const T>>& result)
{
    if (auto value = dynamicPtrCast<const T>(chunk))
        result.push_back(value);
    else if (auto sequence = dynamicPtrCast<const SequenceChunk>(chunk))
        for (const auto& nested : sequence->getChunks())
            collectVhtSoundingChunks<T>(nested, result);
}

template <typename T>
void collectVhtSoundingChunks(const Packet *packet,
        std::vector<Ptr<const T>>& result)
{
    if (dynamicPtrCast<const Ieee80211MpduSubframeHeader>(packet->peekAtFront()) != nullptr) {
        MpduDeaggregation deaggregation;
        auto frames = deaggregation.deaggregateFrame(packet->dup());
        for (auto frame : *frames) {
            collectVhtSoundingChunks<T>(frame, result);
            delete frame;
        }
        delete frames;
    }
    else
        collectVhtSoundingChunks<T>(packet->peekData(), result);
}

static Packet *getFirstVhtSoundingFrame(const Packet *packet)
{
    if (dynamicPtrCast<const Ieee80211MpduSubframeHeader>(packet->peekAtFront()) == nullptr)
        return nullptr;
    MpduDeaggregation deaggregation;
    auto frames = deaggregation.deaggregateFrame(packet->dup());
    Packet *first = frames->empty() ? nullptr : frames->front();
    if (first != nullptr) {
        first->copyTags(*packet);
        frames->erase(frames->begin());
    }
    for (auto frame : *frames)
        delete frame;
    delete frames;
    return first;
}

} // namespace

VhtSoundingFs::VhtSoundingFs(Ieee80211Mib *mib, VhtCsiCache *csiCache,
        const MacAddress& peer, uint16_t associationId,
        uint64_t associationGeneration, uint8_t dialogToken, int soundingNsts,
        physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *ndpMode,
        double beamformingGainDb, bool feedbackTypeMu, uint8_t requestedNc) :
    mib(mib), csiCache(csiCache), peer(peer), associationId(associationId),
    associationGeneration(associationGeneration), dialogToken(dialogToken),
    soundingNsts(soundingNsts), feedbackTypeMu(feedbackTypeMu), requestedNc(requestedNc),
    modeSet(modeSet), ndpMode(ndpMode),
    beamformingGainDb(beamformingGainDb)
{
    ASSERT(mib != nullptr && csiCache != nullptr && modeSet != nullptr && ndpMode != nullptr);
    ASSERT(!peer.isMulticast() && associationId > 0 && associationGeneration > 0);
    ASSERT(soundingNsts >= 2 && soundingNsts <= 8);
    ASSERT(requestedNc >= 1 && requestedNc <= soundingNsts);
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
            const auto matrixBytes = getVhtFeedbackMatrixBytes(
                    ndpMode->getDataMode()->getBandwidth(), soundingNsts,
                    feedbackTypeMu ? requestedNc : 1, 4, feedbackTypeMu);
            const auto muBytes = feedbackTypeMu ? getVhtMuExclusiveReportBytes(
                    ndpMode->getDataMode()->getBandwidth(), requestedNc, 4) : 0;
            const auto reportBytes = matrixBytes + muBytes + (feedbackTypeMu ? requestedNc - 1 : 0);
            const auto maxSegmentReportBytes = VHT_MAX_FEEDBACK_MPDU_BYTES -
                    (2 + 3 + 1 + (feedbackTypeMu ? requestedNc - 1 : 0));
            const auto segmentCount = std::max<size_t>(1,
                    (reportBytes + maxSegmentReportBytes - 1) /
                    maxSegmentReportBytes);
            return new ReceiveCollectionStep(modeSet->getSifsTime() +
                    modeSet->getSlowestMandatoryMode(MHz(20))->getDuration(B(34 +
                    reportBytes + segmentCount * 8)) +
                    feedbackStartTolerance);
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
        bool validFeedback = false;
        if (auto collection = dynamic_cast<ReceiveCollectionStep *>(receive)) {
            const auto& frames = collection->getReceivedFrames();
            std::vector<Ptr<const Ieee80211VhtCompressedBeamformingFeedback>> feedbacks;
            for (auto frame : frames) {
                collectVhtSoundingChunks<Ieee80211VhtCompressedBeamformingFeedback>(frame, feedbacks);
            }
            auto firstFrame = frames.empty() ? nullptr : getFirstVhtSoundingFrame(frames.front());
            if (!frames.empty() && !feedbacks.empty() &&
                    (firstFrame == nullptr ? isExpectedFeedback(frames.front()) :
                    isExpectedFeedback(firstFrame))) {
                auto first = feedbacks.front();
                size_t expectedSegments = first->getRemainingFeedbackSegments() + 1;
                validFeedback = first->getFirstFeedbackSegment() && feedbacks.size() == expectedSegments;
                for (size_t i = 0; validFeedback && i < feedbacks.size(); ++i) {
                    auto feedback = feedbacks[i];
                    validFeedback = feedback != nullptr &&
                            feedback->getRemainingFeedbackSegments() == expectedSegments - i - 1 &&
                            feedback->getFirstFeedbackSegment() == (i == 0) &&
                            (feedback->getSoundingDialogTokenNumber() == dialogToken) &&
                            (std::isnan(ndpMode->getDataMode()->getBandwidth().get()) ||
                             feedback->getFeedbackBandwidth() == ndpMode->getDataMode()->getBandwidth().get()) &&
                            feedback->getNr() == soundingNsts &&
                            feedback->getFeedbackTypeMu() == feedbackTypeMu &&
                            feedback->getNc() == (feedbackTypeMu ? requestedNc : 1);
                }
            }
            delete firstFrame;
        }
        else
            validFeedback = receive->getReceivedFrame() != nullptr &&
                    isExpectedFeedback(receive->getReceivedFrame());
        if (validFeedback)
            // Model policy: cache a synthetic scalar gain, not the normative
            // per-subcarrier MU feedback matrix. Width, association generation,
            // and sounded NSTS key every configuration-changing dimension.
            csiCache->update(peer, ndpMode->getDataMode()->getBandwidth(),
                    associationGeneration, beamformingGainDb, soundingNsts,
                    feedbackTypeMu, feedbackTypeMu ? requestedNc : 1, soundingNsts);
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
    station.muFeedback = feedbackTypeMu;
    station.ncIndex = requestedNc - 1;
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
            expectedFeedbackStart, feedbackStartTolerance,
            ndpMode->getDataMode()->getBandwidth(), soundingNsts,
            feedbackTypeMu, requestedNc);
}

bool VhtSoundingFs::matchesFeedback(Packet *packet,
        const MacAddress& localAddress, const MacAddress& peer,
        uint8_t dialogToken, simtime_t expectedStart,
        simtime_t startTolerance, Hz expectedBandwidth, int expectedNsts,
        bool expectedFeedbackTypeMu, int expectedNc)
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
            feedback->getSoundingDialogTokenNumber() == dialogToken &&
            (std::isnan(expectedBandwidth.get()) || feedback->getFeedbackBandwidth() == expectedBandwidth.get()) &&
            (expectedNsts == 0 || feedback->getNr() == expectedNsts) &&
            feedback->getFeedbackTypeMu() == expectedFeedbackTypeMu &&
            (expectedNc == 0 || feedback->getNc() == expectedNc);
}

} // namespace ieee80211
} // namespace inet
