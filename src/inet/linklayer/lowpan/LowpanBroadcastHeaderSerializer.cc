// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanBroadcastHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanBroadcastHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanBroadcastHeader, LowpanBroadcastHeaderSerializer);

void LowpanBroadcastHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanBroadcastHeader>(chunk);
    if (header->getChunkLength() != B(2)) throw cRuntimeError("Invalid RFC 4944 BC0 header");
    stream.writeByte(0x50);
    stream.writeByte(header->getSequenceNumber());
}

const Ptr<Chunk> LowpanBroadcastHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanBroadcastHeader>();
    if (stream.readByte() != 0x50) header->markIncorrect();
    header->setSequenceNumber(stream.readByte());
    return header;
}
} }
