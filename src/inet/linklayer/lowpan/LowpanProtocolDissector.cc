// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanProtocolDissector.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/linklayer/lowpan/LowpanMeshHeader_m.h"
#include "inet/linklayer/lowpan/LowpanBroadcastHeader_m.h"
#include "inet/linklayer/lowpan/LowpanDispatchPrefix.h"
#include "inet/linklayer/lowpan/LowpanPageHeader_m.h"
#include "inet/linklayer/lowpan/LowpanExtendedDispatchHeader_m.h"
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
    using Prefix = LowpanDispatchPrefix;
    Prefix::State state{0, false};
    auto prefix = [&]() {
        auto parsed = Prefix::parse(*packet, 0, state);
        for (int i = 0; i < parsed.pagingLength; i++)
            callback.visitChunk(packet->popAtFront<LowpanPageHeader>(B(1)), &lowpanProtocol);
        state = parsed.state;
        return parsed.kind;
    };
    auto dissectEsc = [&]() {
        callback.visitChunk(packet->popAtFront<LowpanExtendedDispatchHeader>(B(2)), &lowpanProtocol);
        callback.markIncorrect(); // No EET payload specification is implemented; never search its body.
    };
    Prefix::Kind kind = Prefix::MALFORMED;
    auto dissectOuter = [&]() {
        try {
            kind = prefix();
            bool mesh = kind == Prefix::MESH;
            if (mesh) {
                auto dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
                int length = 1 + ((dispatch & 15) == 15 ? 1 : 0) + (dispatch & 0x20 ? 2 : 8) + (dispatch & 0x10 ? 2 : 8);
                if (packet->getDataLength() <= B(length)) return false;
                auto header = packet->popAtFront<LowpanMeshHeader>(B(length), Chunk::PF_ALLOW_INCORRECT);
                callback.visitChunk(header, &lowpanProtocol);
                if (header->isIncorrect()) return false;
                kind = prefix();
            }
            if (kind == Prefix::BROADCAST) {
                if (!mesh || packet->getDataLength() <= B(2)) return false;
                callback.visitChunk(packet->popAtFront<LowpanBroadcastHeader>(B(2)), &lowpanProtocol);
                kind = prefix();
            }
            return kind != Prefix::MESH && kind != Prefix::BROADCAST && kind != Prefix::MALFORMED &&
                kind != Prefix::UNSUPPORTED_PAGE;
        }
        catch (const cRuntimeError&) { return false; }
    };
    if (packet->getDataLength() < B(1) || packet->getDataLength().get() % 8 != 0 || !dissectOuter()) {
        callback.markIncorrect();
        if (packet->getDataLength() > b(0))
            callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
    }
    else {
        if (kind == Prefix::FRAG1 || kind == Prefix::FRAGN) {
            bool first = kind == Prefix::FRAG1;
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
                if (first) {
                    auto inner = prefix();
                    if (inner == Prefix::IPV6) callback.visitChunk(packet->popAtFront<LowpanHeader>(), &lowpanProtocol);
                    else if (inner == Prefix::IPHC) dissectIphc();
                    else if (inner == Prefix::ESC) dissectEsc();
                    else callback.markIncorrect();
                }
            }
            // Partial IPv6 and all FRAGN payloads remain opaque until reassembly.
            if (packet->getDataLength() > b(0))
                callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
        }
        else if (kind == Prefix::IPHC)
            dissectIphc();
        else if (kind == Prefix::IPV6) {
            callback.visitChunk(packet->popAtFront<LowpanHeader>(), &lowpanProtocol);
            if (packet->getDataLength() < B(40)) {
                callback.markIncorrect();
                if (packet->getDataLength() > b(0))
                    callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
            }
            else
                callback.dissectPacket(packet, &Protocol::ipv6);
        }
        else {
            if (kind == Prefix::ESC) dissectEsc();
            if (packet->getDataLength() > b(0))
            callback.visitChunk(packet->popAtFront(), &lowpanProtocol);
        }
    }
    callback.endProtocolDataUnit(&lowpanProtocol);
}
} } // namespace inet::lowpan
