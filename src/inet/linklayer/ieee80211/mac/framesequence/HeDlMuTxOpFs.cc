//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

HeDlMuTxOpFs::HeDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                             const std::vector<MacAddress>& candidates,
                             Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler)
    : dlScheduler(dlScheduler),
      candidates(candidates),
      modeSet(modeSet),
      pendingQueue(pendingQueue),
      ackHandler(ackHandler)
{
}

void HeDlMuTxOpFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

Packet *HeDlMuTxOpFs::buildMuContainerPacket(FrameSequenceContext *context)
{
    // Determine channel bandwidth and center frequency from the modeSet's first HE mode.
    Hz channelBandwidth = Hz(20e6);       // default: 20 MHz
    Hz channelCenterFrequency = Hz(5.18e9); // default: 5 GHz band, ch36

    if (modeSet != nullptr && modeSet->getNumModes() > 0) {
        auto firstMode = modeSet->getMode(0);
        if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(firstMode)) {
            channelBandwidth = heMode->getDataMode()->getBandwidth();
            channelCenterFrequency = (heMode->getCenterFrequencyMode() == Ieee80211HeMode::BAND_2_4GHZ)
                    ? Hz(2.412e9) : Hz(5.18e9);
        }
    }

    // Obtain per-STA RU assignments from the scheduler.
    auto allocations = dlScheduler->schedule(candidates, channelCenterFrequency, channelBandwidth);
    if (allocations.empty())
        throw cRuntimeError("HeDlMuTxOpFs: scheduler returned empty RU allocation");

    // Assemble the container packet and populate Ieee80211HeMuTag.
    auto container = new Packet("HE-MU-PPDU");

    // Standard QoS data header — broadcast receiver signals HE MU frame.
    auto containerHdr = makeShared<Ieee80211DataHeader>();
    containerHdr->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    containerHdr->setType(ST_DATA_WITH_QOS);
    containerHdr->setChunkLength(b(288)); // minimal 802.11 QoS data header size
    container->insertAtBack(containerHdr);
    container->insertAtBack(makeShared<Ieee80211MacTrailer>());

    auto muTag = container->addTag<Ieee80211HeMuTag>();

    for (const auto& alloc : allocations) {
        // Find the first queued packet destined for this STA.
        Packet *staPacket = nullptr;
        int n = pendingQueue->getNumPackets();
        for (int i = 0; i < n; ++i) {
            Packet *pkt = pendingQueue->getPacket(i);
            const auto& hdr = pkt->peekAtFront<Ieee80211MacHeader>();
            if (hdr->getReceiverAddress() == alloc.staAddress) {
                staPacket = pkt;
                break;
            }
        }
        if (staPacket == nullptr) {
            EV_WARN << "HeDlMuTxOpFs: no queued packet for STA " << alloc.staAddress
                    << ", skipping RU " << alloc.ru.index << endl;
            continue;
        }

        // Remove from pending queue and notify the ack handler.
        pendingQueue->removePacket(staPacket);
        if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                staPacket->peekAtFront<Ieee80211MacHeader>())) {
            ackHandler->frameGotInProgress(dataOrMgmtHdr);
        }

        // Store a duplicate in the tag (tag owns the copy).
        muTag->addAllocation(alloc.ru.index, staPacket->dup());
        delete staPacket;
    }

    if (muTag->getAllocations().empty())
        throw cRuntimeError("HeDlMuTxOpFs: no packets assembled for MU-OFDMA transmission");

    EV_INFO << "HeDlMuTxOpFs: assembled HE MU PPDU with "
            << muTag->getAllocations().size() << " RU allocations." << endl;
    return container;
}

IFrameSequenceStep *HeDlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            containerPacket = buildMuContainerPacket(context);
            return new TransmitStep(containerPacket, context->getIfs());
        }
        case 1:
            return nullptr; // sequence done — no ACK expected at this phase
        default:
            throw cRuntimeError("HeDlMuTxOpFs: unknown step %d", step);
    }
}

bool HeDlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        default:
            throw cRuntimeError("HeDlMuTxOpFs: unknown step %d", step);
    }
}

} // namespace ieee80211
} // namespace inet
