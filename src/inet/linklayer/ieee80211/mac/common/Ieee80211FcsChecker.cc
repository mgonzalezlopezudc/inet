//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/common/Ieee80211FcsChecker.h"

#include "inet/common/checksum/Checksum.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

bool Ieee80211FcsChecker::isFcsOk(Packet *packet,
        AggregateReceptionContext aggregateContext)
{
    if (packet->getDataLength() == b(0))
        return !packet->hasBitError();
    if (dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                packet->peekAtFront()) != nullptr &&
            (packet->findTag<Ieee80211MpduReceiveInd>() != nullptr ||
                    aggregateContext == AggregateReceptionContext::INTACT_AMPDU))
        return !packet->cPacket::hasBitError();
    if (packet->hasBitError() || !packet->peekData()->isCorrect() ||
            !packet->hasAtBack<Ieee80211MacTrailer>(B(4)))
        return false;
    const auto& trailer = packet->peekAtBack<Ieee80211MacTrailer>(B(4));
    switch (trailer->getFcsMode()) {
        case FCS_DECLARED_INCORRECT:
            return false;
        case FCS_DECLARED_CORRECT:
            return true;
        case FCS_COMPUTED: {
            const auto& fcsBytes = packet->peekDataAt<BytesChunk>(B(0),
                    packet->getDataLength() - trailer->getChunkLength());
            auto bufferLength = fcsBytes->getChunkLength().get<B>();
            std::vector<uint8_t> buffer(bufferLength);
            fcsBytes->copyToBuffer(buffer.data(), bufferLength);
            return ethernetFcs(buffer.data(), bufferLength) == trailer->getFcs();
        }
        default:
            throw cRuntimeError("Unknown FCS mode");
    }
}

} // namespace ieee80211
} // namespace inet
