// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRfragHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanRfragHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanRfragHeader, LowpanRfragHeaderSerializer);
void LowpanRfragHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanRfragHeader>(chunk);
    if (header->getChunkLength() != B(6) || header->getSequence() > 31 || header->getFragmentSize() > 1023 ||
            (header->getFragmentOffset() != 0 && header->getFragmentSize() == 0))
        throw cRuntimeError("Invalid RFC 8931 recoverable fragment header");
    stream.writeByte(0xe8 | header->getEcn());
    stream.writeByte(header->getDatagramTag());
    stream.writeUint16Be((header->getAckRequested() ? 0x8000 : 0) | (header->getSequence() << 10) | header->getFragmentSize());
    stream.writeUint16Be(header->getFragmentOffset());
}
const Ptr<Chunk> LowpanRfragHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanRfragHeader>();
    auto dispatch = stream.readByte();
    header->setEcn(dispatch & 1);
    header->setDatagramTag(stream.readByte());
    auto fields = stream.readUint16Be();
    header->setAckRequested(fields & 0x8000);
    header->setSequence((fields >> 10) & 31);
    header->setFragmentSize(fields & 1023);
    header->setFragmentOffset(stream.readUint16Be());
    if ((dispatch & 0xfe) != 0xe8 || (header->getFragmentOffset() != 0 && header->getFragmentSize() == 0))
        header->markIncorrect();
    return header;
}
} }
