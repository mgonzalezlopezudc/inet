// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanMeshHeaderSerializer.h"
#include "inet/linklayer/lowpan/LowpanMeshHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet { namespace lowpan {
Register_Serializer(LowpanMeshHeader, LowpanMeshHeaderSerializer);

void LowpanMeshHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const LowpanMeshHeader>(chunk);
    auto source = header->getOriginator();
    auto destination = header->getFinalDestination();
    bool shortSource = source.getMode() == Ieee802154Address::SHORT;
    bool shortDestination = destination.getMode() == Ieee802154Address::SHORT;
    int length = 1 + (header->getDeepHops() ? 1 : 0) + (shortSource ? 2 : 8) + (shortDestination ? 2 : 8);
    if (source.isUnspecified() || destination.isUnspecified() || (!header->getDeepHops() && header->getHopsLeft() >= 15) ||
            header->getChunkLength() != B(length))
        throw cRuntimeError("Invalid RFC 4944 Mesh header");
    stream.writeByte(0x80 | (shortSource ? 0x20 : 0) | (shortDestination ? 0x10 : 0) |
        (header->getDeepHops() ? 15 : header->getHopsLeft()));
    if (header->getDeepHops()) stream.writeByte(header->getHopsLeft());
    for (const auto& address : {source, destination})
        for (int i = address.getMode() == Ieee802154Address::SHORT ? 1 : 7; i >= 0; i--)
            stream.writeByte(address.getInt() >> (8 * i));
}

const Ptr<Chunk> LowpanMeshHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<LowpanMeshHeader>();
    uint8_t dispatch = stream.readByte();
    bool deep = (dispatch & 15) == 15;
    header->setDeepHops(deep);
    header->setHopsLeft(deep ? stream.readByte() : dispatch & 15);
    int length = 1 + (deep ? 1 : 0);
    auto address = [&](bool shortened) {
        uint64_t value = 0;
        int count = shortened ? 2 : 8;
        length += count;
        for (int i = 0; i < count; i++) value = (value << 8) | stream.readByte();
        return Ieee802154Address(shortened ? Ieee802154Address::SHORT : Ieee802154Address::EXTENDED, value);
    };
    header->setOriginator(address(dispatch & 0x20));
    header->setFinalDestination(address(dispatch & 0x10));
    header->setChunkLength(B(length));
    if ((dispatch & 0xc0) != 0x80) header->markIncorrect();
    return header;
}
} }
