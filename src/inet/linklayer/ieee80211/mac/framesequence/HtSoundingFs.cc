//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HtSoundingFs.h"

#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

template <typename T>
static Ptr<const T> findHtSoundingChunk(const Packet *packet)
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

HtSoundingFs::HtSoundingFs(Ieee80211Mib *mib, HtCsiCache *csiCache,
        const MacAddress& peer, uint64_t associationGeneration,
        uint8_t requestToken, uint8_t soundingNsts,
        Ieee80211HtFeedbackKind feedbackKind,
        physicallayer::Ieee80211ModeSet *modeSet,
        const physicallayer::IIeee80211Mode *ndpMode) :
    mib(mib), csiCache(csiCache), peer(peer),
    associationGeneration(associationGeneration), requestToken(requestToken),
    soundingNsts(soundingNsts), feedbackKind(feedbackKind),
    modeSet(modeSet), ndpMode(ndpMode)
{
    if (mib == nullptr || csiCache == nullptr || modeSet == nullptr || ndpMode == nullptr ||
            peer.isMulticast() || associationGeneration == 0 || requestToken > 6 ||
            soundingNsts < 1 || soundingNsts > 4)
        throw cRuntimeError("Invalid HT sounding sequence configuration");
}

void HtSoundingFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    step = firstStep;
}

IFrameSequenceStep *HtSoundingFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: return new TransmitStep(buildAnnouncement(), SIMTIME_ZERO, true);
        case 1: return new TransmitStep(buildNdp(), modeSet->getSifsTime(), true);
        case 2:
            // IEEE Std 802.11-2024, Table 10-31: immediate feedback follows SIFS.
            return new ReceiveStep(modeSet->getSifsTime() +
                    modeSet->getSlowestMandatoryMode(MHz(20))->getDuration(B(96)) +
                    modeSet->getSlotTime());
        case 3: return nullptr;
        default: throw cRuntimeError("Invalid HT sounding step");
    }
}

bool HtSoundingFs::completeStep(FrameSequenceContext *context)
{
    if (step == 2) {
        auto receive = check_and_cast<IReceiveStep *>(context->getLastStep());
        if (receive->getReceivedFrame() != nullptr && isExpectedFeedback(receive->getReceivedFrame())) {
            auto report = findHtSoundingChunk<Ieee80211HtMimoFeedback>(receive->getReceivedFrame());
            // The bounded report itself is deterministic evidence from the
            // peer's HT-LTF measurement. Its first bytes encode the same
            // SNIR-derived state used by the beamformee; no RF matrix is invented here.
            double quantizedSnirDb = report->getReport(0) / 4.0 - 20.0;
            double snir = std::pow(10.0, quantizedSnirDb / 10.0);
            auto measurement = HtCsiCache::deriveMeasurement(snir, snir,
                    report->getReportArraySize() > 1 ? report->getReport(1) % 77 : 0,
                    soundingNsts, soundingNsts == 3 ? 4 : soundingNsts, feedbackKind);
            measurement.reportBytes.resize(report->getReportArraySize());
            for (size_t i = 0; i < report->getReportArraySize(); i++)
                measurement.reportBytes[i] = report->getReport(i);
            csiCache->update(peer, ndpMode->getDataMode()->getBandwidth(),
                    associationGeneration, soundingNsts, requestToken, measurement);
        }
    }
    step++;
    return true;
}

Packet *HtSoundingFs::buildAnnouncement() const
{
    auto header = makeShared<Ieee80211DataHeader>();
    header->setType(ST_QOS_NULL);
    header->setReceiverAddress(peer);
    header->setTransmitterAddress(mib->address);
    header->setAddress3(peer);
    header->setAckPolicy(NO_ACK);
    header->setOrder(true);
    header->setHtMcsControlPresent(true);
    header->setHtMcsRequest(true);
    header->setHtMcsRequestSequenceIdentifier(requestToken);
    header->setHtCsiSteering(static_cast<uint8_t>(feedbackKind));
    header->setHtNdpAnnouncement(true);
    header->setChunkLength(B(30));
    auto packet = new Packet("HT-NDP-Announcement", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

Packet *HtSoundingFs::buildNdp() const
{
    auto packet = new Packet("HT-NDP");
    packet->addTag<physicallayer::Ieee80211ModeReq>()->setMode(ndpMode);
    auto request = packet->addTag<physicallayer::Ieee80211HtTransmissionReq>();
    request->setSounding(true);
    request->setNdp(true);
    request->setNumberOfSpaceTimeStreams(soundingNsts);
    request->setNumberOfHtLtfSymbols(soundingNsts == 3 ? 4 : soundingNsts);
    return packet;
}

bool HtSoundingFs::isExpectedFeedback(Packet *packet) const
{
    return matchesFeedback(packet, mib->address, peer, requestToken, soundingNsts,
            ndpMode->getDataMode()->getBandwidth(), feedbackKind);
}

bool HtSoundingFs::matchesFeedback(Packet *packet, const MacAddress& localAddress,
        const MacAddress& peer, uint8_t requestToken, uint8_t soundingNsts,
        Hz channelWidth, Ieee80211HtFeedbackKind feedbackKind)
{
    if (packet == nullptr)
        return false;
    auto header = dynamicPtrCast<const Ieee80211ActionFrame>(packet->peekAtFront());
    auto feedback = findHtSoundingChunk<Ieee80211HtMimoFeedback>(packet);
    bool matchingFeedbackType =
            (feedbackKind == Ieee80211HtFeedbackKind::CSI &&
             dynamicPtrCast<const Ieee80211HtCsiFeedback>(feedback) != nullptr) ||
            (feedbackKind == Ieee80211HtFeedbackKind::NONCOMPRESSED_BEAMFORMING &&
             dynamicPtrCast<const Ieee80211HtNoncompressedBeamformingFeedback>(feedback) != nullptr) ||
            (feedbackKind == Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING &&
             dynamicPtrCast<const Ieee80211HtCompressedBeamformingFeedback>(feedback) != nullptr);
    return header != nullptr && feedback != nullptr && header->getType() == ST_NOACKACTION &&
            header->getCategory() == 7 && header->getReceiverAddress() == localAddress &&
            header->getTransmitterAddress() == peer &&
            matchingFeedbackType &&
            feedback->getNr() == soundingNsts &&
            feedback->getChannelWidth() == channelWidth.get();
}

} // namespace ieee80211
} // namespace inet
