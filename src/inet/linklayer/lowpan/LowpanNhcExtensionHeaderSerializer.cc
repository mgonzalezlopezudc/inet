// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanNhcExtensionHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanNhcExtensionHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"

namespace inet { namespace lowpan {
Register_Serializer(LowpanNhcExtensionHeader, LowpanNhcExtensionHeaderSerializer);

void LowpanNhcExtensionHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanNhcExtensionHeader>(chunk);
    int eid = header->getEid();
    int length = header->getDataArraySize();
    bool nested = eid == 7;
    int size = nested ? 1 : 1 + !header->getNh() + (eid != 2) + length;
    if ((eid > 4 && !nested) || (nested && (header->getNh() || length != 0)) ||
        (eid == 2 ? length != 7 : length > 255) || header->getChunkLength() != B(size))
        throw cRuntimeError("Invalid extension LOWPAN_NHC header");
    stream.writeByte(0xe0 | (eid << 1) | header->getNh());
    if (nested)
        return;
    if (!header->getNh())
        stream.writeByte(header->getNextHeader());
    if (eid != 2)
        stream.writeByte(length);
    for (int i = 0; i < length; i++)
        stream.writeByte(header->getData(i));
}

const Ptr<Chunk> LowpanNhcExtensionHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanNhcExtensionHeader>();
    int dispatch = stream.readByte();
    int eid = (dispatch >> 1) & 7;
    header->setEid(eid);
    header->setNh(dispatch & 1);
    if ((dispatch & 0xf0) != 0xe0 || (eid > 4 && eid != 7) || (eid == 7 && header->getNh())) {
        header->markIncorrect();
        header->setChunkLength(B(1));
        return header;
    }
    if (eid == 7) {
        header->setChunkLength(B(1));
        return header;
    }
    if (!header->getNh())
        header->setNextHeader(stream.readByte());
    int length = eid == 2 ? 7 : stream.readByte();
    header->setDataArraySize(length);
    for (int i = 0; i < length; i++)
        header->setData(i, stream.readByte());
    header->setChunkLength(B(1 + !header->getNh() + (eid != 2) + length));
    return header;
}
} }
