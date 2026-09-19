// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanFragnHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanFragnHeader, LowpanFragnHeaderSerializer);

void LowpanFragnHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanFragnHeader>(chunk);
    if (header->getDatagramSize() < 40 || header->getDatagramSize() > 2047 || header->getChunkLength() != B(5) || (header->getDatagramOffset() == 0 || 8 * header->getDatagramOffset() >= header->getDatagramSize()))
        throw cRuntimeError("Invalid RFC 4944 LowpanFragnHeader fields");
    stream.writeByte(0xe0 | (header->getDatagramSize() >> 8));
    stream.writeByte(header->getDatagramSize() & 0xff);
    stream.writeUint16Be(header->getDatagramTag());
    stream.writeByte(header->getDatagramOffset());
}

const Ptr<Chunk> LowpanFragnHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanFragnHeader>();
    auto dispatch = stream.readByte();
    header->setDatagramSize(((dispatch & 7) << 8) | stream.readByte());
    header->setDatagramTag(stream.readUint16Be());
    header->setDatagramOffset(stream.readByte());
    if ((dispatch & 0xf8) != 0xe0 || header->getDatagramSize() < 40 || (header->getDatagramOffset() == 0 || 8 * header->getDatagramOffset() >= header->getDatagramSize()))
        header->markIncorrect();
    return header;
}
} } // namespace inet::lowpan
