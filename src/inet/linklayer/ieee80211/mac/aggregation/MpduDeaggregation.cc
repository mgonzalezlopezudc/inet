//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/aggregation/MpduDeaggregation.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

Register_Class(MpduDeaggregation);

std::vector<Packet *> *MpduDeaggregation::deaggregateFrame(Packet *aggregatedFrame)
{
    EV_DEBUG << "Deaggregating A-MPDU " << *aggregatedFrame << " into multiple packets.\n";
    auto frames = new std::vector<Packet *>();
    auto reject = [&]() -> std::vector<Packet *> * {
        throw cRuntimeError("Malformed IEEE 802.11 A-MPDU");
    };
    try {
        bool sawMpdu = false;
        bool sawEofPadding = false;
        while (aggregatedFrame->getDataLength() > b(0)) {
            // The final-subframe or EOF Padding Octets field may end with
            // fewer than four unspecified octets. It is never interpreted
            // as another delimiter.
            if (aggregatedFrame->getDataLength() <= B(3)) {
                if (!sawMpdu)
                    return reject();
                delete aggregatedFrame;
                EV_TRACE << "Created " << frames->size() << " packets from A-MPDU.\n";
                return frames;
            }
            if (aggregatedFrame->getDataLength() < LENGTH_A_MPDU_SUBFRAME_HEADER)
                return reject();
            const auto delimiter = aggregatedFrame->peekAtFront<Ieee80211MpduSubframeHeader>();
            const bool eofOrTag = delimiter->getEof();
            const int mpduLength = delimiter->getLength();
            aggregatedFrame->popAtFront<Ieee80211MpduSubframeHeader>();
            if (aggregatedFrame->getDataLength() < B(mpduLength))
                return reject();

            if (mpduLength > 0) {
                if (sawEofPadding)
                    return reject();
                const b sourceOffset = aggregatedFrame->getFrontOffset();
                const auto mpdu = aggregatedFrame->peekDataAt(b(0), B(mpduLength));
                auto frame = new Packet("aMpduSubframe");
                frame->copyTags(*aggregatedFrame);
                frame->insertAtBack(mpdu);
                frame->getRegionTags().copyTags(aggregatedFrame->getRegionTags(),
                        sourceOffset, b(0), B(mpduLength));
                frames->push_back(frame);
                aggregatedFrame->setFrontOffset(sourceOffset + B(mpduLength));
                sawMpdu = true;
            }
            else {
                // For MPDU Length=0, Table 9-659 defines EOF=1 as a VHT
                // EOF padding subframe and EOF=0 as a legal pre-EOF spacing
                // delimiter. No non-EOF subframe may follow EOF padding.
                if (eofOrTag) {
                    if (!sawMpdu)
                        return reject();
                    sawEofPadding = true;
                }
                else if (sawEofPadding)
                    return reject();
                continue;
            }

            int paddingLength = (4 - (mpduLength % 4)) % 4;
            // At the PSDU boundary, §10.12.6 may have room for fewer than
            // the octets needed to reach the next four-octet boundary.
            int availablePadding = std::min(paddingLength,
                    static_cast<int>(aggregatedFrame->getDataLength().get<B>()));
            aggregatedFrame->setFrontOffset(aggregatedFrame->getFrontOffset() + B(availablePadding));
            if (availablePadding < paddingLength || aggregatedFrame->getDataLength() == b(0)) {
                delete aggregatedFrame;
                EV_TRACE << "Created " << frames->size() << " packets from A-MPDU.\n";
                return frames;
            }
        }
        if (!sawMpdu)
            return reject();
        delete aggregatedFrame;
        return frames;
    }
    catch (const cRuntimeError&) {
        for (auto frame : *frames)
            delete frame;
        delete frames;
        throw;
    }
}

} /* namespace ieee80211 */
} /* namespace inet */
