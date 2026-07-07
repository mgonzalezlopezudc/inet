//
// Copyright (C) 2018 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyProtocolDissector.h"

#include "inet/common/ProtocolGroup.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/common/packet/dissector/ProtocolDissectorRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {

Register_Protocol_Dissector(&Protocol::ieee80211FhssPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211IrPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211DsssPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211HrDsssPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211OfdmPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211ErpOfdmPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211HtPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211VhtPhy, Ieee80211PhyProtocolDissector);
Register_Protocol_Dissector(&Protocol::ieee80211HePhy, Ieee80211PhyProtocolDissector);

namespace {

const int tolerantParsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;

void visitRemainingData(Packet *packet, ProtocolDissector::ICallback& callback, const Protocol *protocol)
{
    if (packet->getDataLength() != b(0)) {
        const auto& data = packet->popAtFront(packet->getDataLength(), tolerantParsingFlags);
        callback.visitChunk(data, protocol);
    }
}

} // namespace

void Ieee80211PhyProtocolDissector::dissect(Packet *packet, const Protocol *protocol, ICallback& callback) const
{
    auto phyProtocol = protocol;
    if (phyProtocol == nullptr) {
        const auto& packetProtocolTag = packet->findTag<PacketProtocolTag>();
        phyProtocol = packetProtocolTag != nullptr ? packetProtocolTag->getProtocol() : nullptr;
    }
    callback.startProtocolDataUnit(phyProtocol);
    auto originalBackOffset = packet->getBackOffset();
    auto payloadEndOffset = packet->getFrontOffset();
    Ptr<const physicallayer::Ieee80211PhyHeader> header;
    try {
        header = physicallayer::Ieee80211Radio::popIeee80211PhyHeaderAtFront(packet, b(-1), tolerantParsingFlags);
    }
    catch (cRuntimeError&) {
        callback.markIncorrect();
        visitRemainingData(packet, callback, phyProtocol);
        callback.endProtocolDataUnit(phyProtocol);
        return;
    }
    if (header->isIncorrect() || header->isIncomplete() || header->isImproperlyRepresented())
        callback.markIncorrect();
    callback.visitChunk(header, phyProtocol);
    payloadEndOffset += header->getChunkLength() + header->getLengthField();
    bool incorrect = (payloadEndOffset > originalBackOffset || b(header->getLengthField()) < header->getChunkLength());
    if (incorrect) {
        callback.markIncorrect();
        payloadEndOffset = originalBackOffset;
    }
    packet->setBackOffset(payloadEndOffset);
    try {
        callback.dissectPacket(packet, &Protocol::ieee80211Mac);
    }
    catch (...) {
        packet->setBackOffset(originalBackOffset);
        throw;
    }
    packet->setBackOffset(originalBackOffset);
    auto paddingLength = packet->getDataLength();
    if (paddingLength > b(0)) {
        const auto& padding = packet->popAtFront(paddingLength, tolerantParsingFlags);
        callback.visitChunk(padding, phyProtocol);
    }
    callback.endProtocolDataUnit(phyProtocol);
}

} // namespace inet
