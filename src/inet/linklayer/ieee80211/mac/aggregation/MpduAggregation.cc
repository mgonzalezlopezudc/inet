//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/aggregation/MpduAggregation.h"

#include "inet/common/packet/chunk/ByteCountChunk.h"

namespace inet {
namespace ieee80211 {

Register_Class(MpduAggregation);

Packet *MpduAggregation::aggregateFrames(std::vector<Packet *> *frames)
{
    EV_DEBUG << "Aggregating " << frames->size() << " packets into A-MPDU.\n";
    auto aggregatedFrame = new Packet();
    if (frames->empty())
        throw cRuntimeError("Cannot create an IEEE 802.11 A-MPDU from no MPDUs");
    // Preserve direction/request tags on the aggregate. They are still local
    // until the PHY consumes them; the delimiter and payload are the only
    // information that crosses the transmission boundary.
    aggregatedFrame->copyTags(*frames->front());
    std::string aggregatedName;
    for (size_t i = 0; i < frames->size(); i++) {
        auto mpduSubframeHeader = makeShared<Ieee80211MpduSubframeHeader>();
        auto frame = frames->at(i);
        auto mpdu = frame->peekAll();
        if (mpdu->getChunkLength().get<B>() > 0x3fff)
            throw cRuntimeError("IEEE 802.11 A-MPDU MPDU length exceeds the 14-bit delimiter field");
        mpduSubframeHeader->setLength(mpdu->getChunkLength().get<B>());
        // With a nonzero MPDU Length this bit is Tag, not an end marker
        // (IEEE Std 802.11-2024, Table 9-659). Ordinary A-MPDU MPDUs are
        // untagged; EOF=1 is reserved here for zero-length EOF padding
        // subframes (S-MPDUs and ack-enabled A-MPDUs are separate paths).
        mpduSubframeHeader->setEof(false);
        mpduSubframeHeader->setReserved(false);
        aggregatedFrame->insertAtBack(mpduSubframeHeader);
        aggregatedFrame->insertAtBack(mpdu);
        aggregatedFrame->getRegionTags().copyTags(frame->getRegionTags(), frame->getFrontOffset(),
                aggregatedFrame->getBackOffset() - frame->getDataLength(), frame->getDataLength());
        int paddingLength = 4 - (mpduSubframeHeader->getChunkLength() + mpdu->getChunkLength()).get<B>() % 4;
        if (i + 1 != frames->size() && paddingLength != 4) {
            auto padding = makeShared<ByteCountChunk>(B(paddingLength));
            aggregatedFrame->insertAtBack(padding);
        }
        if (i != 0)
            aggregatedName.append("+");
        aggregatedName.append(frame->getName());
        delete frame;
    }
    aggregatedFrame->setName(aggregatedName.c_str());
    EV_TRACE << "Created A-MPDU " << *aggregatedFrame << ".\n";
    return aggregatedFrame;
}

} /* namespace ieee80211 */
} /* namespace inet */
