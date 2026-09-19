// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanUdpNhcHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanUdpNhcCodec.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"

namespace inet { namespace lowpan {
Register_Serializer(LowpanUdpNhcHeader, LowpanUdpNhcHeaderSerializer);

void LowpanUdpNhcHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanUdpNhcHeader>(chunk);
    int mode = header->getPortMode();
    auto source = header->getSourcePort();
    auto destination = header->getDestinationPort();
    if (mode > 3 || header->getChecksumElided() || header->getChunkLength() != B(LowpanUdpNhcCodec::getWireLength(mode)) ||
        (mode == 1 && (destination & 0xff00) != 0xf000) ||
        (mode == 2 && (source & 0xff00) != 0xf000) ||
        (mode == 3 && ((source & 0xfff0) != 0xf0b0 || (destination & 0xfff0) != 0xf0b0)))
        throw cRuntimeError("Invalid or unsupported UDP NHC header");
    stream.writeByte(0xf0 | mode);
    if (mode == 3)
        stream.writeByte((source << 4) | (destination & 15));
    else {
        if (mode == 2) stream.writeByte(source);
        else stream.writeUint16Be(source);
        if (mode == 1) stream.writeByte(destination);
        else stream.writeUint16Be(destination);
    }
    stream.writeUint16Be(header->getChecksum());
}

const Ptr<Chunk> LowpanUdpNhcHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanUdpNhcHeader>();
    int dispatch = stream.readByte();
    int mode = dispatch & 3;
    header->setPortMode(mode);
    header->setChecksumElided(dispatch & 4);
    if ((dispatch & 0xf8) != 0xf0)
        header->markIncorrect();
    if (mode == 3) {
        auto ports = stream.readByte();
        header->setSourcePort(0xf0b0 | (ports >> 4));
        header->setDestinationPort(0xf0b0 | (ports & 15));
    }
    else {
        header->setSourcePort(mode == 2 ? 0xf000 | stream.readByte() : stream.readUint16Be());
        header->setDestinationPort(mode == 1 ? 0xf000 | stream.readByte() : stream.readUint16Be());
    }
    if (!header->getChecksumElided())
        header->setChecksum(stream.readUint16Be());
    header->setChunkLength(B(LowpanUdpNhcCodec::getWireLength(mode, header->getChecksumElided())));
    return header;
}
} } // namespace inet::lowpan
