// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIphcHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"

namespace inet { namespace lowpan {
Register_Serializer(LowpanIphcHeader, LowpanIphcHeaderSerializer);

void LowpanIphcHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanIphcHeader>(chunk);
    int length = LowpanIphcCodec::getInlineLength(*header);
    if (length < 0 || header->getInlineDataArraySize() != (size_t)length ||
        header->getChunkLength() != B(2 + header->getCid() + length))
        throw cRuntimeError("Invalid LOWPAN_IPHC header shape");
    stream.writeByte(0x60 | (header->getTf() << 3) | (header->getNh() << 2) | header->getHlim());
    stream.writeByte((header->getCid() << 7) | (header->getSac() << 6) | (header->getSam() << 4) |
        (header->getM() << 3) | (header->getDac() << 2) | header->getDam());
    if (header->getCid())
        stream.writeByte(header->getContextId());
    for (int i = 0; i < length; i++)
        stream.writeByte(header->getInlineData(i));
}

const Ptr<Chunk> LowpanIphcHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanIphcHeader>();
    auto first = stream.readByte();
    auto second = stream.readByte();
    header->setTf((first >> 3) & 3);
    header->setNh(first & 4);
    header->setHlim(first & 3);
    header->setCid(second & 0x80);
    header->setSac(second & 0x40);
    header->setSam((second >> 4) & 3);
    header->setM(second & 8);
    header->setDac(second & 4);
    header->setDam(second & 3);
    if (header->getCid())
        header->setContextId(stream.readByte());
    int length = LowpanIphcCodec::getInlineLength(*header);
    if ((first & 0xe0) != 0x60 || length < 0) {
        header->markIncorrect();
        header->setChunkLength(B(2 + header->getCid()));
        return header;
    }
    header->setInlineDataArraySize(length);
    for (int i = 0; i < length; i++)
        header->setInlineData(i, stream.readByte());
    header->setChunkLength(B(2 + header->getCid() + length));
    return header;
}

} } // namespace inet::lowpan
