// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanNativeProtocolDissector.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameHeader_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/checksum/Checksum.h"
#include "inet/common/packet/dissector/ProtocolDissectorRegistry.h"
namespace inet { namespace lowpan {
Register_Protocol_Dissector(&lowpanNativeProtocol, LowpanNativeProtocolDissector);
void LowpanNativeProtocolDissector::dissect(Packet *packet, const Protocol *, ICallback& callback) const
{
    callback.startProtocolDataUnit(&lowpanNativeProtocol);
    bool valid = packet->getDataLength() >= B(5) && packet->getDataLength() <= B(127) && packet->getDataLength().get() % 8 == 0;
    if (valid) {
        auto bytes = packet->peekDataAsBytes()->getBytes();
        valid = crc16_ccitt(bytes) == 0;
        auto header = packet->peekAtFront<Ieee802154FrameHeader>(b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
        valid = valid && !header->isIncomplete() && !header->isIncorrect() && header->getChunkLength() + B(2) <= packet->getDataLength();
        if (valid) {
            auto fcs = packet->popAtBack<BytesChunk>(B(2));
            packet->popAtFront(header->getChunkLength());
            callback.visitChunk(header, &lowpanNativeProtocol);
            if (header->getFrameControl() != 2)
                callback.dissectPacket(packet, &lowpanProtocol);
            else if (packet->getDataLength() != b(0)) {
                valid = false;
                callback.visitChunk(packet->popAtFront(), &lowpanNativeProtocol);
            }
            callback.visitChunk(fcs, &lowpanNativeProtocol);
        }
    }
    if (!valid) {
        callback.markIncorrect();
        if (packet->getDataLength() > b(0)) callback.visitChunk(packet->popAtFront(), &lowpanNativeProtocol);
    }
    callback.endProtocolDataUnit(&lowpanNativeProtocol);
}
} }
