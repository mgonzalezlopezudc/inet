// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanNhcCodec.h"
#include "inet/linklayer/lowpan/LowpanNhcExtensionHeader_m.h"
#include "inet/linklayer/lowpan/LowpanUdpNhcCodec.h"
#include "inet/common/packet/chunk/SequenceChunk.h"

namespace inet { namespace lowpan {
namespace {
const int protocols[] = {0, 43, 44, 60, 135};
constexpr int MAX_DEPTH = 16;

struct Chain {
    Ptr<const Chunk> header;
    int original = 0;
    int encoded = 0;
    int protocol = 59;
};

Ptr<const Chunk> join(const Ptr<const Chunk>& first, const Ptr<const Chunk>& second)
{
    if (!second)
        return first;
    auto result = makeShared<SequenceChunk>();
    result->insertAtBack(first);
    result->insertAtBack(second);
    result->markImmutable();
    return result;
}

// Validate the complete TLV area and identify one final padding option.
int trailingPadding(const std::vector<uint8_t>& body)
{
    int trailing = 0;
    for (size_t offset = 0; offset < body.size();) {
        int type = body[offset];
        int size = type == 0 ? 1 : offset + 1 < body.size() ? 2 + body[offset + 1] : -1;
        if (size < 0 || offset + size > body.size())
            return -1;
        trailing = type == 0 || type == 1 ? size : 0;
        if (type == 1)
            for (int i = 2; i < size; i++)
                if (body[offset + i] != 0)
                    return -1;
        offset += size;
    }
    return trailing <= 7 ? trailing : 0;
}

class Codec
{
  public:
    const Packet& packet;
    const Ieee802154Address& source;
    const Ieee802154Address& destination;
    const LowpanContextTable *contexts;
    simtime_t now;

    std::vector<uint8_t> bytes(int offset, int size) const
    {
        return packet.peekDataAt<BytesChunk>(B(offset), B(size))->getBytes();
    }

    Chain encodeIpv6(int offset, int total, const Ipv6Header *outer, int depth) const
    {
        if (depth > MAX_DEPTH || total < 40)
            return {};
        auto ipv6 = packet.peekDataAt<Ipv6Header>(B(offset), B(40));
        if (ipv6->getPayloadLength() != B(total - 40))
            return {};
        auto child = encodeNext(ipv6->getProtocolId(), offset + 40, total - 40, *ipv6, depth + 1);
        auto iphc = LowpanIphcCodec::encode(*ipv6, source, destination, child.header != nullptr, contexts, now, outer);
        if (!iphc)
            return {};
        return {join(iphc, child.header), 40 + child.original,
            int(iphc->getChunkLength().get<B>()) + child.encoded, IP_PROT_IPv6};
    }

    Chain encodeNext(int protocol, int offset, int remaining, const Ipv6Header& outer, int depth) const
    {
        if (depth > MAX_DEPTH)
            return {};
        if (protocol == IP_PROT_UDP && remaining >= 8) {
            auto udp = LowpanUdpNhcCodec::encode(bytes(offset, 8), remaining);
            return udp ? Chain{udp, 8, int(udp->getChunkLength().get<B>()), protocol} : Chain{};
        }
        if (protocol == IP_PROT_IPv6) {
            auto inner = encodeIpv6(offset, remaining, &outer, depth + 1);
            if (!inner.header)
                return {};
            auto marker = makeShared<LowpanNhcExtensionHeader>();
            marker->setEid(7);
            marker->setChunkLength(B(1));
            marker->markImmutable();
            return {join(marker, inner.header), inner.original, 1 + inner.encoded, protocol};
        }
        int eid = 0;
        while (eid < 5 && protocols[eid] != protocol)
            eid++;
        if (eid == 5 || remaining < 8)
            return {};
        auto prefix = bytes(offset, 2);
        int size = eid == 2 ? 8 : 8 * (int(prefix[1]) + 1);
        if (size > remaining)
            return {};
        auto body = bytes(offset + (eid == 2 ? 1 : 2), size - (eid == 2 ? 1 : 2));
        if (eid == 0 || eid == 3) {
            int padding = trailingPadding(body);
            if (padding < 0)
                return {};
            body.resize(body.size() - padding);
        }
        if (eid != 2 && body.size() > 255)
            return {};
        auto child = encodeNext(prefix[0], offset + size, remaining - size, outer, depth + 1);
        auto header = makeShared<LowpanNhcExtensionHeader>();
        header->setEid(eid);
        header->setNh(child.header != nullptr);
        header->setNextHeader(prefix[0]);
        header->setDataArraySize(body.size());
        for (size_t i = 0; i < body.size(); i++)
            header->setData(i, body[i]);
        int encoded = 1 + !header->getNh() + (eid != 2) + body.size();
        header->setChunkLength(B(encoded));
        header->markImmutable();
        return {join(header, child.header), size + child.original, encoded + child.encoded, protocol};
    }

    Chain decodeIpv6(int offset, int total, const Ipv6Header *outer, int depth) const
    {
        if (depth > MAX_DEPTH)
            return {};
        auto iphc = packet.peekDataAt<LowpanIphcHeader>(B(offset), b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
        if (iphc->isIncomplete() || iphc->isIncorrect())
            return {};
        int size = iphc->getChunkLength().get<B>();
        auto ipv6 = LowpanIphcCodec::decode(*iphc, total < 0 ? 40 : total, source, destination,
            iphc->getNh(), contexts, now, outer);
        if (!ipv6)
            return {};
        Chain child;
        if (iphc->getNh()) {
            child = decodeNext(offset + size, total < 0 ? -1 : total - 40, *ipv6, depth + 1);
            if (!child.header)
                return {};
            ipv6->setProtocolId(static_cast<IpProtocolId>(child.protocol));
        }
        if (total < 0)
            total = 40 + child.original + packet.getDataLength().get<B>() - offset - size - child.encoded;
        if (total < 40 + child.original || total > 65575)
            return {};
        ipv6->setPayloadLength(B(total - 40));
        ipv6->markImmutable();
        return {join(ipv6, child.header), 40 + child.original, size + child.encoded, IP_PROT_IPv6};
    }

    Chain decodeNext(int offset, int remaining, const Ipv6Header& outer, int depth) const
    {
        if (depth > MAX_DEPTH || offset >= packet.getDataLength().get<B>())
            return {};
        int dispatch = bytes(offset, 1)[0];
        if ((dispatch & 0xf8) == 0xf0) {
            auto header = packet.peekDataAt<LowpanUdpNhcHeader>(B(offset), b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
            int size = header->getChunkLength().get<B>();
            int payload = remaining < 0 ? 8 + packet.getDataLength().get<B>() - offset - size : remaining;
            auto udp = LowpanUdpNhcCodec::decode(*header, payload);
            if (!udp)
                return {};
            auto restored = makeShared<BytesChunk>(udp->getBytes());
            restored->markImmutable();
            return {restored, 8, size, IP_PROT_UDP};
        }
        if ((dispatch & 0xf0) != 0xe0)
            return {};
        auto header = packet.peekDataAt<LowpanNhcExtensionHeader>(B(offset), b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
        if (header->isIncomplete() || header->isIncorrect())
            return {};
        int encoded = header->getChunkLength().get<B>();
        int eid = header->getEid();
        if (eid == 7) {
            auto inner = decodeIpv6(offset + encoded, remaining, &outer, depth + 1);
            if (!inner.header)
                return {};
            return {inner.header, inner.original, encoded + inner.encoded, IP_PROT_IPv6};
        }
        std::vector<uint8_t> body;
        for (size_t i = 0; i < header->getDataArraySize(); i++)
            body.push_back(header->getData(i));
        if (eid == 0 || eid == 3) {
            if (trailingPadding(body) < 0)
                return {};
            int padding = (8 - ((body.size() + 2) % 8)) % 8;
            if (padding == 1)
                body.push_back(0);
            else if (padding > 1) {
                body.push_back(1);
                body.push_back(padding - 2);
                body.insert(body.end(), padding - 2, 0);
            }
        }
        int size = body.size() + (eid == 2 ? 1 : 2);
        if (size < 8 || size % 8 != 0 || (remaining >= 0 && remaining < size))
            return {};
        Chain child;
        if (header->getNh()) {
            child = decodeNext(offset + encoded, remaining < 0 ? -1 : remaining - size, outer, depth + 1);
            if (!child.header)
                return {};
        }
        std::vector<uint8_t> restored{uint8_t(child.header ? child.protocol : header->getNextHeader())};
        if (eid != 2)
            restored.push_back(size / 8 - 1);
        restored.insert(restored.end(), body.begin(), body.end());
        auto expanded = makeShared<BytesChunk>(restored);
        expanded->markImmutable();
        return {join(expanded, child.header), size + child.original,
            encoded + child.encoded, protocols[eid]};
    }
};
}

LowpanNhcCodec::Result LowpanNhcCodec::encode(const Packet& packet, const Ieee802154Address& source,
        const Ieee802154Address& destination, const LowpanContextTable *contexts, simtime_t now)
{
    try {
        Codec codec{packet, source, destination, contexts, now};
        auto result = codec.encodeIpv6(0, packet.getDataLength().get<B>(), nullptr, 0);
        return {result.header, result.original, result.encoded};
    }
    catch (const cRuntimeError& error) {
        EV_DEBUG << "NHC encode fallback: " << error.what() << endl;
        return {};
    }
}

LowpanNhcCodec::Result LowpanNhcCodec::decode(const Packet& packet, int originalSize, const Ieee802154Address& source,
        const Ieee802154Address& destination, const LowpanContextTable *contexts, simtime_t now)
{
    try {
        Codec codec{packet, source, destination, contexts, now};
        auto result = codec.decodeIpv6(0, originalSize, nullptr, 0);
        return {result.header, result.original, result.encoded};
    }
    catch (const cRuntimeError& error) {
        EV_DEBUG << "NHC decode rejected: " << error.what() << endl;
        return {};
    }
}
} }
