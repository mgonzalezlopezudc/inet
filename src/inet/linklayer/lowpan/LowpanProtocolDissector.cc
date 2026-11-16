// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanProtocolDissector.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/linklayer/lowpan/LowpanMeshHeader_m.h"
#include "inet/linklayer/lowpan/LowpanBroadcastHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanIphcHeader_m.h"
#include "inet/linklayer/lowpan/LowpanUdpNhcHeader_m.h"
#include "inet/linklayer/lowpan/LowpanNhcExtensionHeader_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/packet/dissector/ProtocolDissectorRegistry.h"

namespace inet { namespace lowpan {
Register_Protocol_Dissector(&lowpanProtocol, LowpanProtocolDissector);

void LowpanProtocolDissector::dissect(Packet *packet, const Protocol *protocol, ICallback& callback) const
{
    callback.startProtocolDataUnit(&lowpanProtocol);
    std::function<bool(bool, int)> dissectCompressed = [&](bool iphc, int depth) {
        if (depth > 16 || packet->getDataLength() < B(1))
            return false;
        int dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
        Ptr<const Chunk> header;
        bool next = false;
        bool nested = false;
        int flags = Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT;
        if (iphc) {
            if ((dispatch & 0xe0) != 0x60)
                return false;
            auto parsed = packet->peekAtFront<LowpanIphcHeader>(b(-1), flags);
            next = parsed->getNh();
            header = parsed;
        }
        else if ((dispatch & 0xf8) == 0xf0)
            header = packet->peekAtFront<LowpanUdpNhcHeader>(b(-1), flags);
        else if ((dispatch & 0xf0) == 0xe0) {
            auto parsed = packet->peekAtFront<LowpanNhcExtensionHeader>(b(-1), flags);
            nested = parsed->getEid() == 7;
            next = nested || parsed->getNh();
            header = parsed;
        }
        else
            return false;
        if (header->isIncomplete() || header->isIncorrect() || header->getChunkLength() > packet->getDataLength())
            return false;
        packet->popAtFront(header->getChunkLength());
        callback.visitChunk(header, &lowpanProtocol);
        return !next || dissectCompressed(nested, depth + 1);
    };
    auto dissectIphc = [&]() {
        try {
            if (!dissectCompressed(true, 0))
                callback.markIncorrect();
        }
        catch (const cRuntimeError&) {
            callback.markIncorrect();
        }
        // Compressed payload cannot be decoded without hop identity and original size.
        if (packet->getDataLength() > b(0))
            callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
    };
    auto dissectOuter = [&]() {
        try {
            auto dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
            bool mesh = (dispatch & 0xc0) == 0x80;
            if (mesh) {
                int length = 1 + ((dispatch & 15) == 15 ? 1 : 0) + (dispatch & 0x20 ? 2 : 8) + (dispatch & 0x10 ? 2 : 8);
                if (packet->getDataLength() <= B(length)) return false;
                auto header = packet->popAtFront<LowpanMeshHeader>(B(length), Chunk::PF_ALLOW_INCORRECT);
                callback.visitChunk(header, &lowpanProtocol);
                if (header->isIncorrect()) return false;
                dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
            }
            if (dispatch == 0x50) {
                if (!mesh || packet->getDataLength() <= B(2)) return false;
                callback.visitChunk(packet->popAtFront<LowpanBroadcastHeader>(B(2)), &lowpanProtocol);
                dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
            }
            return (dispatch & 0xc0) != 0x80 && dispatch != 0x50;
        }
        catch (const cRuntimeError&) { return false; }
    };
    if (packet->getDataLength() < B(1) || packet->getDataLength().get() % 8 != 0 || !dissectOuter()) {
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
