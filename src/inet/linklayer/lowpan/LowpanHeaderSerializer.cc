// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"

namespace inet { namespace lowpan {
Register_Serializer(LowpanHeader, LowpanHeaderSerializer);

void LowpanHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanHeader>(chunk);
    if (header->getDispatch() != 0x41 || header->getChunkLength() != B(1))
        throw cRuntimeError("LowpanHeader represents only one-octet LOWPAN_IPV6 dispatch");
    stream.writeByte(header->getDispatch());
}

const Ptr<Chunk> LowpanHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanHeader>();
    header->setDispatch(stream.readByte());
    if (header->getDispatch() != 0x41)
        header->markIncorrect();
    return header;
}
} } // namespace inet::lowpan
