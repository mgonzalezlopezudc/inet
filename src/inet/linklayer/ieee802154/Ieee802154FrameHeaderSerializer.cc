// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/ieee802154/Ieee802154FrameHeaderSerializer.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameHeader_m.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameFormat.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
namespace inet {
Register_Serializer(Ieee802154FrameHeader, Ieee802154FrameHeaderSerializer);

void Ieee802154FrameHeaderSerializer::serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = staticPtrCast<const Ieee802154FrameHeader>(chunk);
    int control = header->getFrameControl();
    bool ack = control == 0x0002;
    bool broadcast = control == 0xd841;
    if ((Ieee802154FrameFormat::getHeaderLength(control) < 0) ||
        header->getChunkLength() != B(Ieee802154FrameFormat::getHeaderLength(control)))
        throw cRuntimeError("Unsupported native IEEE 802.15.4 frame control or header length");
    if (!ack && (header->getPanId() == 0xffff || header->getSourceAddress().getMode() != Ieee802154Address::EXTENDED ||
        header->getSourceAddress().getInt() == UINT64_MAX ||
        (broadcast ? !header->getDestinationAddress().isBroadcast() :
         header->getDestinationAddress().getMode() != Ieee802154Address::EXTENDED || header->getDestinationAddress().getInt() == UINT64_MAX)))
        throw cRuntimeError("Invalid native IEEE 802.15.4 address/PAN");
    stream.writeUint16Le(control);
    stream.writeByte(header->getSequenceNumber());
    if (!ack) {
        stream.writeUint16Le(header->getPanId());
        auto destination = header->getDestinationAddress().getInt();
        for (int i = 0; i < (broadcast ? 2 : 8); i++)
            stream.writeByte(destination >> (8 * i));
        auto source = header->getSourceAddress().getInt();
        for (int i = 0; i < 8; i++)
            stream.writeByte(source >> (8 * i));
    }
}

const Ptr<Chunk> Ieee802154FrameHeaderSerializer::deserializeFields(MemoryInputStream& stream, const std::type_info&) const
{
    auto header = makeShared<Ieee802154FrameHeader>();
    int control = stream.readUint16Le();
    header->setFrameControl(control);
    if (Ieee802154FrameFormat::getHeaderLength(control) < 0) {
        header->setChunkLength(B(2));
        header->markIncorrect();
        return header;
    }
    header->setSequenceNumber(stream.readByte());
    bool ack = control == 0x0002;
    bool broadcast = control == 0xd841;
    header->setChunkLength(B(Ieee802154FrameFormat::getHeaderLength(control)));
    if (!ack) {
        header->setPanId(stream.readUint16Le());
        uint64_t destination = 0, source = 0;
        for (int i = 0; i < (broadcast ? 2 : 8); i++)
            destination |= uint64_t(stream.readByte()) << (8 * i);
        for (int i = 0; i < 8; i++)
            source |= uint64_t(stream.readByte()) << (8 * i);
        header->setDestinationAddress(Ieee802154Address(broadcast ? Ieee802154Address::SHORT : Ieee802154Address::EXTENDED, destination));
        header->setSourceAddress(Ieee802154Address(Ieee802154Address::EXTENDED, source));
        if (header->getPanId() == 0xffff || source == UINT64_MAX ||
            (broadcast ? destination != 0xffff : destination == UINT64_MAX))
            header->markIncorrect();
    }
    return header;
}
}
