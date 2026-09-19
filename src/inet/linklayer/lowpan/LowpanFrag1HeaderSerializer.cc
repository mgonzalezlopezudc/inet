// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanFrag1HeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanFrag1Header, LowpanFrag1HeaderSerializer);

void LowpanFrag1HeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanFrag1Header>(chunk);
    if (header->getDatagramSize() < 40 || header->getDatagramSize() > 2047 || header->getChunkLength() != B(4))
        throw cRuntimeError("Invalid RFC 4944 LowpanFrag1Header fields");
    stream.writeByte(0xc0 | (header->getDatagramSize() >> 8));
    stream.writeByte(header->getDatagramSize() & 0xff);
    stream.writeUint16Be(header->getDatagramTag());
}

const Ptr<Chunk> LowpanFrag1HeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanFrag1Header>();
    auto dispatch = stream.readByte();
    header->setDatagramSize(((dispatch & 7) << 8) | stream.readByte());
    header->setDatagramTag(stream.readUint16Be());
    if ((dispatch & 0xf8) != 0xc0 || header->getDatagramSize() < 40)
        header->markIncorrect();
    return header;
}
} } // namespace inet::lowpan
