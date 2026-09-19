// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanProtocolDissector.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanIphcHeader_m.h"
#include "inet/linklayer/lowpan/LowpanUdpNhcHeader_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/packet/dissector/ProtocolDissectorRegistry.h"

namespace inet { namespace lowpan {
Register_Protocol_Dissector(&lowpanProtocol, LowpanProtocolDissector);

void LowpanProtocolDissector::dissect(Packet *packet, const Protocol *protocol, ICallback& callback) const
{
    callback.startProtocolDataUnit(&lowpanProtocol);
    auto dissectIphc = [&]() {
        try {
            auto header = packet->peekAtFront<LowpanIphcHeader>(b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
            if (header->isIncomplete() || header->isIncorrect() || header->getChunkLength() > packet->getDataLength())
                callback.markIncorrect();
            else {
                packet->popAtFront(header->getChunkLength());
                callback.visitChunk(header, &lowpanProtocol);
                if (header->getNh()) {
                    if (packet->getDataLength() < B(1))
                        callback.markIncorrect();
                    else if ((packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0] & 0xf8) == 0xf0) {
                        auto nhc = packet->peekAtFront<LowpanUdpNhcHeader>(b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
                        if (nhc->isIncomplete() || nhc->isIncorrect() || nhc->getChunkLength() > packet->getDataLength())
                            callback.markIncorrect();
                        else {
                            packet->popAtFront(nhc->getChunkLength());
                            callback.visitChunk(nhc, &lowpanProtocol);
                        }
                    }
                }
            }
        }
        catch (const cRuntimeError&) {
            callback.markIncorrect();
        }
        // Compressed payload cannot be decoded without hop identity and original size.
        if (packet->getDataLength() > b(0))
            callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
    };
    if (packet->getDataLength() < B(1) || packet->getDataLength().get() % 8 != 0) {
        callback.markIncorrect();
        if (packet->getDataLength() > b(0))
            callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
    }
    else {
        auto dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
        if ((dispatch & 0xf8) == 0xc0 || (dispatch & 0xf8) == 0xe0) {
            bool first = (dispatch & 0xf8) == 0xc0;
            int length = first ? 4 : 5;
            if (packet->getDataLength() <= B(length))
                callback.markIncorrect();
            else {
                Ptr<const Chunk> header;
                if (first)
                    header = packet->popAtFront<LowpanFrag1Header>(B(length), Chunk::PF_ALLOW_INCORRECT);
                else
                    header = packet->popAtFront<LowpanFragnHeader>(B(length), Chunk::PF_ALLOW_INCORRECT);
                if (header->isIncorrect())
                    callback.markIncorrect();
                callback.visitChunk(header, &lowpanProtocol);
                if (first && packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0] == 0x41)
                    callback.visitChunk(packet->popAtFront<LowpanHeader>(), &lowpanProtocol);
                else if (first && (packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0] & 0xe0) == 0x60)
                    dissectIphc();
            }
            // Partial IPv6 and all FRAGN payloads remain opaque until reassembly.
            if (packet->getDataLength() > b(0))
                callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
        }
        else if ((dispatch & 0xe0) == 0x60)
            dissectIphc();
        else if (dispatch == 0x41) {
            callback.visitChunk(packet->popAtFront<LowpanHeader>(), &lowpanProtocol);
            if (packet->getDataLength() < B(40)) {
                callback.markIncorrect();
                if (packet->getDataLength() > b(0))
                    callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
            }
            else
                callback.dissectPacket(packet, &Protocol::ipv6);
        }
        else
            callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
    }
    callback.endProtocolDataUnit(&lowpanProtocol);
}
} } // namespace inet::lowpan
