// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanPageHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanPageHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanPageHeader, LowpanPageHeaderSerializer);
void LowpanPageHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanPageHeader>(chunk);
    if (header->getPage() > 15 || header->getChunkLength() != B(1)) throw cRuntimeError("Invalid LoWPAN page header");
    stream.writeByte(0xf0 | header->getPage());
}
const Ptr<Chunk> LowpanPageHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanPageHeader>();
    auto dispatch = stream.readByte();
    header->setPage(dispatch & 15);
    if ((dispatch & 0xf0) != 0xf0) header->markIncorrect();
    return header;
}
} }
