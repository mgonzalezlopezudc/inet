//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfAggregationService.h"

#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211FecCodingReq.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Packet *HcfAggregationService::buildAmpduPacket(
        const std::vector<Packet *>& frames, FcsMode fcsMode)
{
    Ptr<const Ieee80211FecCodingReq> fecCodingReq;
    for (auto frame : frames) {
        auto frameFecCodingReq = frame->findTag<Ieee80211FecCodingReq>();
        if (frameFecCodingReq != nullptr) {
            if (fecCodingReq != nullptr &&
                    fecCodingReq->getLdpcAllowed() != frameFecCodingReq->getLdpcAllowed())
                throw cRuntimeError("Cannot build an A-MPDU from MPDUs with different FEC coding requests");
            fecCodingReq = frameFecCodingReq;
        }
    }
    auto aggregatedPacket = new Packet();
    std::string aggregatedName;
    for (size_t i = 0; i < frames.size(); i++) {
        auto frame = frames[i];
        auto trailer = frame->removeAtBack<Ieee80211MacTrailer>(B(4));
        trailer->setFcsMode(fcsMode);
        if (fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(frame, fcsMode));
        frame->insertAtBack(trailer);
        auto mpdu = frame->peekAll();
        auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
        delimiter->setEof(false);
        delimiter->setReserved(0);
        delimiter->setLength(mpdu->getChunkLength().get<B>());
        aggregatedPacket->insertAtBack(delimiter);
        aggregatedPacket->insertAtBack(mpdu);
        aggregatedPacket->getRegionTags().copyTags(frame->getRegionTags(), B(0),
                aggregatedPacket->getFrontOffset() - frame->getDataLength(), frame->getDataLength());
        int paddingLength = (4 - (delimiter->getChunkLength() + mpdu->getChunkLength()).get<B>() % 4) % 4;
        if (i + 1 != frames.size() && paddingLength != 0)
            aggregatedPacket->insertAtBack(makeShared<ByteCountChunk>(B(paddingLength)));
        if (i != 0)
            aggregatedName.append("+");
        aggregatedName.append(frame->getName());
    }
    aggregatedPacket->setName(aggregatedName.c_str());
    if (fecCodingReq != nullptr)
        aggregatedPacket->addTag<Ieee80211FecCodingReq>()->setLdpcAllowed(fecCodingReq->getLdpcAllowed());
    return aggregatedPacket;
}

B HcfAggregationService::calculateAmpduLength(
        const std::vector<Packet *>& frames)
{
    B aggregateLength(0);
    for (size_t i = 0; i < frames.size(); i++) {
        // Match buildAmpduPacket(): the complete stored MPDU (including its
        // existing MAC trailer) follows the four-byte delimiter.
        auto subframeLength = B(4) + frames[i]->getTotalLength();
        aggregateLength += subframeLength;
        if (i + 1 != frames.size())
            aggregateLength += B((4 - subframeLength.get<B>() % 4) % 4);
    }
    return aggregateLength;
}

void HcfAggregationService::recordTransmission(Packet *packet,
        const std::vector<Packet *>& subframes, bool implicitBlockAck)
{
    transmissionLedger.record(packet, subframes, implicitBlockAck);
}

bool HcfAggregationService::hasImplicitBlockAck(Packet *packet) const
{
    return transmissionLedger.hasImplicitBlockAck(packet);
}

std::optional<AmpduTransmissionLedger::Entry>
HcfAggregationService::takeTransmission(Packet *packet)
{
    return transmissionLedger.take(packet);
}

bool HcfAggregationService::discardTransmission(Packet *packet)
{
    return transmissionLedger.discard(packet);
}

} // namespace ieee80211
} // namespace inet
