//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

Define_Module(HeHcf);

void HeHcf::initialize(int stage)
{
    Hcf::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        dlScheduler = check_and_cast<IIeee80211HeDlScheduler *>(getSubmodule("dlScheduler"));
        delete frameSequenceHandler;
        frameSequenceHandler = new HeFrameSequenceHandler();
    }
}

std::vector<MacAddress> HeHcf::collectCandidateStations(queueing::IPacketQueue *queue) const
{
    std::vector<MacAddress> candidates;
    int n = queue->getNumPackets();
    for (int i = 0; i < n; ++i) {
        Packet *pkt = queue->getPacket(i);
        const auto& header = pkt->peekAtFront<Ieee80211MacHeader>();
        MacAddress dest = header->getReceiverAddress();
        if (dest.isMulticast() || dest.isBroadcast())
            continue;
        bool seen = false;
        for (const auto& c : candidates) {
            if (c == dest) { seen = true; break; }
        }
        if (!seen)
            candidates.push_back(dest);
    }
    return candidates;
}

void HeHcf::startFrameSequence(AccessCategory ac)
{
    // Check whether HE mode and multi-user conditions are met.
    bool isHeMode = (modeSet != nullptr && strcmp(modeSet->getName(), "ax") == 0);
    if (isHeMode) {
        auto edcaf = edca->getEdcaf(ac);
        auto pendingQueue = edcaf->getPendingQueue();
        auto candidates = collectCandidateStations(pendingQueue);
        if (candidates.size() >= 2) {
            EV_INFO << "HeHcf: MU-OFDMA opportunity detected for " << candidates.size()
                    << " STAs — starting HeDlMuTxOpFs." << endl;
            frameSequenceHandler->startFrameSequence(
                    new HeDlMuTxOpFs(dlScheduler, candidates, modeSet,
                                     pendingQueue, edcaf->getAckHandler(), this),
                    buildContext(ac), this);
            emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
            return;
        }
    }
    // Fallback: standard single-user frame sequence.
    Hcf::startFrameSequence(ac);
}

} // namespace ieee80211
} // namespace inet
