// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRfragAckHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanRfragAckHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanRfragAckHeader, LowpanRfragAckHeaderSerializer);
void LowpanRfragAckHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanRfragAckHeader>(chunk);
    if (header->getChunkLength() != B(6)) throw cRuntimeError("Invalid RFC 8931 acknowledgement header");
    stream.writeByte(0xea | header->getEcn());
    stream.writeByte(header->getDatagramTag());
    stream.writeUint32Be(header->getBitmap());
}
const Ptr<Chunk> LowpanRfragAckHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanRfragAckHeader>();
    auto dispatch = stream.readByte();
    header->setEcn(dispatch & 1);
    header->setDatagramTag(stream.readByte());
    header->setBitmap(stream.readUint32Be());
    if ((dispatch & 0xfe) != 0xea) header->markIncorrect();
    return header;
}
} }
