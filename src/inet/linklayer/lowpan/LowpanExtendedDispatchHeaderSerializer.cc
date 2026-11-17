// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanExtendedDispatchHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanExtendedDispatchHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanExtendedDispatchHeader, LowpanExtendedDispatchHeaderSerializer);
void LowpanExtendedDispatchHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanExtendedDispatchHeader>(chunk);
    if (header->getChunkLength() != B(2)) throw cRuntimeError("Invalid LoWPAN ESC header");
    stream.writeByte(0x40);
    stream.writeByte(header->getExtensionType());
}
const Ptr<Chunk> LowpanExtendedDispatchHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanExtendedDispatchHeader>();
    if (stream.readByte() != 0x40) header->markIncorrect();
    header->setExtensionType(stream.readByte());
    return header;
}
} }
