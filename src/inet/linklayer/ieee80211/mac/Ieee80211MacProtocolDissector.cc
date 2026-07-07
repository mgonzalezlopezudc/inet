//
// Copyright (C) 2018 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Ieee80211MacProtocolDissector.h"

#include "inet/common/ProtocolGroup.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/dissector/ProtocolDissectorRegistry.h"
#include "inet/linklayer/ieee802/Ieee802EpdHeader_m.h"
#include "inet/linklayer/ieee80211/llc/LlcProtocolTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee8022/Ieee8022LlcHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {

Register_Protocol_Dissector(&Protocol::ieee80211Mac, Ieee80211MacProtocolDissector);

namespace {

const int tolerantParsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;

template<typename T>
bool isMalformed(const Ptr<const T>& chunk)
{
    return chunk->isIncorrect() || chunk->isIncomplete() || chunk->isImproperlyRepresented();
}

void visitRemainingData(Packet *packet, ProtocolDissector::ICallback& callback, const Protocol *protocol)
{
    if (packet->getDataLength() != b(0)) {
        const auto& data = packet->popAtFront(packet->getDataLength(), tolerantParsingFlags);
        callback.visitChunk(data, protocol);
    }
}

} // namespace

const Protocol *Ieee80211MacProtocolDissector::computeLlcProtocol(Packet *packet) const
{
    if (const auto& llcTag = packet->findTag<ieee80211::LlcProtocolTag>())
        return llcTag->getProtocol();
    else if (const auto& channelTag = packet->findTag<physicallayer::Ieee80211ChannelInd>()) {
        // EtherType protocol discrimination is mandatory for deployments in the 5.9 GHz band
        if (channelTag->getChannel()->getBand() == &physicallayer::Ieee80211CompliantBands::band5_9GHz)
            return &Protocol::ieee802epd;
    }
    if (packet->getDataLength() == b(0))
        return nullptr;
    Ptr<const Chunk> header;
    try {
        header = packet->peekAtFront(b(-1), tolerantParsingFlags);
    }
    catch (cRuntimeError&) {
        return nullptr;
    }
    if (dynamicPtrCast<const Ieee8022LlcHeader>(header) != nullptr)
        return &Protocol::ieee8022llc;
    else if (dynamicPtrCast<const Ieee802EpdHeader>(header) != nullptr)
        return &Protocol::ieee802epd;
    else
        return nullptr;
}

void Ieee80211MacProtocolDissector::dissect(Packet *packet, const Protocol *protocol, ICallback& callback) const
{
    callback.startProtocolDataUnit(&Protocol::ieee80211Mac);
    Ptr<const inet::ieee80211::Ieee80211MacHeader> header;
    try {
        header = packet->popAtFront<inet::ieee80211::Ieee80211MacHeader>(b(-1), tolerantParsingFlags);
    }
    catch (cRuntimeError&) {
        callback.markIncorrect();
        visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
        callback.endProtocolDataUnit(&Protocol::ieee80211Mac);
        return;
    }
    if (isMalformed(header))
        callback.markIncorrect();
    callback.visitChunk(header, &Protocol::ieee80211Mac);
    if (packet->getDataLength() < B(4)) {
        callback.markIncorrect();
        visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
        callback.endProtocolDataUnit(&Protocol::ieee80211Mac);
        return;
    }
    Ptr<const inet::ieee80211::Ieee80211MacTrailer> trailer;
    try {
        trailer = packet->popAtBack<inet::ieee80211::Ieee80211MacTrailer>(B(4), tolerantParsingFlags);
    }
    catch (cRuntimeError&) {
        callback.markIncorrect();
        visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
        callback.endProtocolDataUnit(&Protocol::ieee80211Mac);
        return;
    }
    if (isMalformed(trailer))
        callback.markIncorrect();
    // TODO fragmentation & aggregation
    if (auto dataHeader = dynamicPtrCast<const inet::ieee80211::Ieee80211DataHeader>(header)) {
        if (packet->getDataLength() == b(0))
            ;
        else if (dataHeader->getMoreFragments() || dataHeader->getFragmentNumber() != 0)
            callback.dissectPacket(packet, nullptr);
        else if (dataHeader->getAMsduPresent()) {
            auto originalTrailerPopOffset = packet->getBackOffset();
            int paddingLength = 0;
            while (packet->getDataLength() > B(0)) {
                int padding = paddingLength == 4 ? 0 : paddingLength;
                if (padding != 0) {
                    if (packet->getDataLength() < B(padding))
                        callback.markIncorrect();
                    if (packet->getDataLength() <= B(padding)) {
                        visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
                        break;
                    }
                    const auto& paddingChunk = packet->popAtFront(B(padding), tolerantParsingFlags);
                    callback.visitChunk(paddingChunk, &Protocol::ieee80211Mac);
                }
                if (packet->getDataLength() < ieee80211::LENGTH_A_MSDU_SUBFRAME_HEADER) {
                    callback.markIncorrect();
                    visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
                    break;
                }
                Ptr<const ieee80211::Ieee80211MsduSubframeHeader> msduSubframeHeader;
                try {
                    msduSubframeHeader = packet->popAtFront<ieee80211::Ieee80211MsduSubframeHeader>(b(-1), tolerantParsingFlags);
                }
                catch (cRuntimeError&) {
                    callback.markIncorrect();
                    visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
                    break;
                }
                if (isMalformed(msduSubframeHeader))
                    callback.markIncorrect();
                callback.visitChunk(msduSubframeHeader, &Protocol::ieee80211Mac);
                auto msduDataLength = msduSubframeHeader->getLength();
                if (msduDataLength < 0) {
                    callback.markIncorrect();
                    visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
                    break;
                }
                auto msduLength = B(msduDataLength);
                if (msduLength > packet->getDataLength()) {
                    callback.markIncorrect();
                    visitRemainingData(packet, callback, &Protocol::ieee80211Mac);
                    break;
                }
                auto msduEndOffset = packet->getFrontOffset() + msduLength;
                packet->setBackOffset(msduEndOffset);
                try {
                    callback.dissectPacket(packet, computeLlcProtocol(packet));
                }
                catch (...) {
                    packet->setBackOffset(originalTrailerPopOffset);
                    throw;
                }
                paddingLength = (4 - (msduSubframeHeader->getChunkLength() + msduLength).get<B>() % 4) % 4;
                packet->setBackOffset(originalTrailerPopOffset);
                packet->setFrontOffset(msduEndOffset);
            }
        }
        else
            callback.dissectPacket(packet, computeLlcProtocol(packet));
    }
    else if (dynamicPtrCast<const inet::ieee80211::Ieee80211ActionFrame>(header)) {
        if (packet->getDataLength() != b(0)) {
            const auto& body = packet->popAtFront(packet->getDataLength(), tolerantParsingFlags);
            callback.visitChunk(body, &Protocol::ieee80211Mac);
        }
    }
    else if (dynamicPtrCast<const inet::ieee80211::Ieee80211MgmtHeader>(header))
        callback.dissectPacket(packet, &Protocol::ieee80211Mgmt);
    else if (packet->getDataLength() != b(0)) {
        const auto& body = packet->popAtFront(packet->getDataLength(), tolerantParsingFlags);
        callback.visitChunk(body, &Protocol::ieee80211Mac);
    }
    callback.visitChunk(trailer, &Protocol::ieee80211Mac);
    callback.endProtocolDataUnit(&Protocol::ieee80211Mac);
}

} // namespace inet
