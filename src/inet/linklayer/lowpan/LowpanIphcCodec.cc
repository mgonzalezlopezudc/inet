// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace inet { namespace lowpan {
namespace {
using AddressBytes = std::array<uint8_t, 16>;

AddressBytes addressBytes(const Ipv6Address& address)
{
    AddressBytes result{};
    for (int i = 0; i < 16; i++)
        result[i] = address.words()[i / 4] >> (24 - (i % 4) * 8);
    return result;
}

Ipv6Address ipv6Address(const AddressBytes& bytes)
{
    uint32_t words[4]{};
    for (int i = 0; i < 16; i++)
        words[i / 4] |= uint32_t(bytes[i]) << (24 - (i % 4) * 8);
    return Ipv6Address(words[0], words[1], words[2], words[3]);
}

bool zeroRange(const AddressBytes& bytes, int begin, int end)
{
    for (int i = begin; i < end; i++)
        if (bytes[i] != 0)
            return false;
    return true;
}

std::optional<AddressBytes> derivedAddress(const Ieee802154Address& address, const Ipv6Address *encapsulating = nullptr)
{
    if (encapsulating) {
        auto bytes = addressBytes(*encapsulating);
        std::fill(bytes.begin(), bytes.begin() + 8, 0);
        bytes[0] = 0xfe;
        bytes[1] = 0x80;
        return bytes;
    }
    if (address.isUnspecified() || address.isBroadcast() ||
        (address.getMode() == Ieee802154Address::SHORT && address.getInt() == 0xfffe))
        return std::nullopt;
    // RFC 6282 section 3.2.2: native extended U/L inversion or short IID mapping.
    uint64_t iid = address.getMode() == Ieee802154Address::EXTENDED ?
        address.getInt() ^ UINT64_C(0x0200000000000000) : UINT64_C(0x000000fffe000000) | address.getInt();
    AddressBytes bytes{};
    bytes[0] = 0xfe;
    bytes[1] = 0x80;
    for (int i = 0; i < 8; i++)
        bytes[8 + i] = iid >> (56 - 8 * i);
    return bytes;
}

int encodeUnicast(const AddressBytes& address, const Ieee802154Address& link, std::vector<uint8_t>& data,
        const Ipv6Address *encapsulating)
{
    if (address[0] == 0xfe && address[1] == 0x80 && zeroRange(address, 2, 8)) {
        auto derived = derivedAddress(link, encapsulating);
        if (derived && *derived == address)
            return 3;
        if (zeroRange(address, 8, 11) && address[11] == 0xff && address[12] == 0xfe && address[13] == 0) {
            data.insert(data.end(), address.begin() + 14, address.end());
            return 2;
        }
        data.insert(data.end(), address.begin() + 8, address.end());
        return 1;
    }
    data.insert(data.end(), address.begin(), address.end());
    return 0;
}

int encodeMulticast(const AddressBytes& address, std::vector<uint8_t>& data)
{
    if (address[1] == 2 && zeroRange(address, 2, 15)) {
        data.push_back(address[15]);
        return 3;
    }
    if (zeroRange(address, 2, 13)) {
        data.push_back(address[1]);
        data.insert(data.end(), address.begin() + 13, address.end());
        return 2;
    }
    if (zeroRange(address, 2, 11)) {
        data.push_back(address[1]);
        data.insert(data.end(), address.begin() + 11, address.end());
        return 1;
    }
    data.insert(data.end(), address.begin(), address.end());
    return 0;
}

std::optional<AddressBytes> statefulUnicast(int mode, const LowpanContextTable::Context& context,
        const Ieee802154Address& link, const std::vector<uint8_t>& inlined, const Ipv6Address *encapsulating)
{
    AddressBytes bytes{};
    if (mode == 3) {
        auto derived = derivedAddress(link, encapsulating);
        if (!derived && context.prefixLength != 128)
            return std::nullopt;
        if (derived)
            std::copy(derived->begin() + 8, derived->end(), bytes.begin() + 8);
    }
    else if (mode == 1 && inlined.size() == 8)
        std::copy(inlined.begin(), inlined.end(), bytes.begin() + 8);
    else if (mode == 2 && inlined.size() == 2) {
        bytes[11] = 0xff;
        bytes[12] = 0xfe;
        bytes[14] = inlined[0];
        bytes[15] = inlined[1];
    }
    else
        return std::nullopt;
    auto prefix = addressBytes(context.prefix);
    for (int bit = 0; bit < context.prefixLength; bit++) {
        uint8_t mask = 0x80 >> (bit % 8);
        bytes[bit / 8] = (bytes[bit / 8] & ~mask) | (prefix[bit / 8] & mask);
    }
    return bytes[0] == 0xff ? std::nullopt : std::optional<AddressBytes>(bytes);
}

struct EncodedAddress {
    int mode = 0;
    bool stateful = false;
    int contextId = 0;
    std::vector<uint8_t> data;
};

EncodedAddress chooseUnicast(const AddressBytes& address, const Ieee802154Address& link,
        const LowpanContextTable *contexts, simtime_t now, const Ipv6Address *encapsulating)
{
    EncodedAddress best;
    best.mode = encodeUnicast(address, link, best.data, encapsulating);
    if (contexts == nullptr)
        return best;
    for (int id = 0; id < 16; id++) {
        auto context = contexts->findForCompression(id, now);
        if (!context || !context->allowCompression)
            continue;
        for (int mode : {3, 2, 1}) {
            int start = mode == 3 ? 16 : mode == 2 ? 14 : 8;
            std::vector<uint8_t> candidate(address.begin() + start, address.end());
            auto reconstructed = statefulUnicast(mode, *context, link, candidate, encapsulating);
            if (reconstructed && *reconstructed == address && candidate.size() < best.data.size())
                best = EncodedAddress{mode, true, id, std::move(candidate)};
        }
    }
    return best;
}

EncodedAddress chooseMulticast(const AddressBytes& address, const LowpanContextTable *contexts, simtime_t now)
{
    EncodedAddress best;
    best.mode = encodeMulticast(address, best.data);
    if (contexts == nullptr || (address[1] & 0x30) != 0x30)
        return best;
    for (int id = 0; id < 16; id++) {
        auto context = contexts->findForCompression(id, now);
        if (!context || !context->allowCompression || context->prefixLength > 64 || address[3] != context->prefixLength)
            continue;
        auto prefix = addressBytes(context->prefix);
        if (!std::equal(prefix.begin(), prefix.begin() + 8, address.begin() + 4))
            continue;
        std::vector<uint8_t> candidate{address[1], address[2]};
        candidate.insert(candidate.end(), address.begin() + 12, address.end());
        if (candidate.size() < best.data.size())
            best = EncodedAddress{0, true, id, std::move(candidate)};
    }
    return best;
}
} // namespace

int LowpanIphcCodec::getInlineLength(const LowpanIphcHeader& header)
{
    static const int trafficLengths[] = {4, 3, 1, 0};
    static const int unicastLengths[] = {16, 8, 2, 0};
    static const int multicastLengths[] = {16, 6, 4, 1};
    if (header.getTf() > 3 || header.getHlim() > 3 || header.getSam() > 3 || header.getDam() > 3 ||
        (!header.getM() && header.getDac() && header.getDam() == 0) ||
        (header.getM() && header.getDac() && header.getDam() != 0))
        return -1;
    int sourceLength = header.getSac() && header.getSam() == 0 ? 0 : unicastLengths[header.getSam()];
    int destinationLength = !header.getM() ? unicastLengths[header.getDam()] :
        header.getDac() ? 6 : multicastLengths[header.getDam()];
    return trafficLengths[header.getTf()] + !header.getNh() + (header.getHlim() == 0) + sourceLength + destinationLength;
}

Ptr<const LowpanIphcHeader> LowpanIphcCodec::encode(const Ipv6Header& ipv6,
        const Ieee802154Address& source, const Ieee802154Address& destination, bool udpNhc,
        const LowpanContextTable *contexts, simtime_t now, const Ipv6Header *encapsulating)
{
    if (ipv6.getVersion() != 6 || ipv6.getFlowLabel() > 0xfffff || ipv6.getSrcAddress().isMulticast())
        return nullptr;
    auto header = makeShared<LowpanIphcHeader>();
    std::vector<uint8_t> data;
    auto traffic = ipv6.getTrafficClass();
    auto flow = ipv6.getFlowLabel();
    if (flow != 0) {
        if ((traffic >> 2) != 0) {
            header->setTf(0);
            data.push_back((traffic << 6) | (traffic >> 2));
            data.push_back(flow >> 16);
        }
        else {
            header->setTf(1);
            data.push_back((traffic << 6) | (flow >> 16));
        }
        data.push_back(flow >> 8);
        data.push_back(flow);
    }
    else if (traffic != 0) {
        header->setTf(2);
        data.push_back((traffic << 6) | (traffic >> 2));
    }
    else
        header->setTf(3);
    header->setNh(udpNhc);
    if (!udpNhc)
        data.push_back(ipv6.getProtocolId());
    auto hopLimit = ipv6.getHopLimit();
    if (hopLimit == 1 || hopLimit == 64 || hopLimit == 255)
        header->setHlim(hopLimit == 1 ? 1 : hopLimit == 64 ? 2 : 3);
    else {
        header->setHlim(0);
        data.push_back(hopLimit);
    }
    int sourceContext = 0;
    if (ipv6.getSrcAddress().isUnspecified()) {
        header->setSac(true);
        header->setSam(0);
    }
    else {
        auto encoded = chooseUnicast(addressBytes(ipv6.getSrcAddress()), source, contexts, now,
            encapsulating ? &encapsulating->getSrcAddress() : nullptr);
        header->setSam(encoded.mode);
        header->setSac(encoded.stateful);
        sourceContext = encoded.contextId;
        data.insert(data.end(), encoded.data.begin(), encoded.data.end());
    }
    header->setM(ipv6.getDestAddress().isMulticast());
    auto encodedDestination = header->getM() ? chooseMulticast(addressBytes(ipv6.getDestAddress()), contexts, now) :
        chooseUnicast(addressBytes(ipv6.getDestAddress()), destination, contexts, now,
            encapsulating ? &encapsulating->getDestAddress() : nullptr);
    header->setDam(encodedDestination.mode);
    header->setDac(encodedDestination.stateful);
    data.insert(data.end(), encodedDestination.data.begin(), encodedDestination.data.end());
    int contextId = (sourceContext << 4) | encodedDestination.contextId;
    header->setContextId(contextId);
    header->setCid(contextId != 0);
    header->setInlineDataArraySize(data.size());
    for (size_t i = 0; i < data.size(); i++)
        header->setInlineData(i, data[i]);
    header->setChunkLength(B(2 + header->getCid() + data.size()));
    header->markImmutable();
    return header;
}

Ptr<Ipv6Header> LowpanIphcCodec::decode(const LowpanIphcHeader& header, int originalSize,
        const Ieee802154Address& source, const Ieee802154Address& destination, bool udpNhc,
        const LowpanContextTable *contexts, simtime_t now, const Ipv6Header *encapsulating)
{
    int expected = getInlineLength(header);
    if (header.isIncorrect() || header.isIncomplete() || expected < 0 ||
        header.getInlineDataArraySize() != (size_t)expected || header.getChunkLength() != B(2 + header.getCid() + expected) ||
        originalSize < 40 || originalSize > 65575 || (header.getNh() && !udpNhc))
        return nullptr;
    // CID selectors remain unused by stateless forms and the unspecified source.
    auto ipv6 = makeShared<Ipv6Header>();
    size_t cursor = 0;
    auto read = [&]() { return header.getInlineData(cursor++); };
    uint8_t traffic = 0;
    uint32_t flow = 0;
    if (header.getTf() == 0) {
        auto rotated = read();
        traffic = (rotated << 2) | (rotated >> 6);
        flow = uint32_t(read() & 15) << 16;
        flow |= uint32_t(read()) << 8;
        flow |= read();
    }
    else if (header.getTf() == 1) {
        auto high = read();
        traffic = high >> 6;
        flow = uint32_t(high & 15) << 16;
        flow |= uint32_t(read()) << 8;
        flow |= read();
    }
    else if (header.getTf() == 2) {
        auto rotated = read();
        traffic = (rotated << 2) | (rotated >> 6);
    }
    ipv6->setTrafficClass(traffic);
    ipv6->setFlowLabel(flow);
    ipv6->setProtocolId(header.getNh() ? IP_PROT_UDP : static_cast<IpProtocolId>(read()));
    static const uint8_t hopLimits[] = {0, 1, 64, 255};
    ipv6->setHopLimit(header.getHlim() == 0 ? read() : hopLimits[header.getHlim()]);
    auto decodeUnicast = [&](int mode, const Ieee802154Address& link, const Ipv6Address *outer) -> std::optional<AddressBytes> {
        if (mode == 3)
            return derivedAddress(link, outer);
        AddressBytes bytes{};
        int start = 0;
        if (mode != 0) {
            bytes[0] = 0xfe;
            bytes[1] = 0x80;
            start = mode == 1 ? 8 : 14;
            if (mode == 2) {
                bytes[11] = 0xff;
                bytes[12] = 0xfe;
            }
        }
        for (int i = start; i < 16; i++)
            bytes[i] = read();
        if (bytes[0] == 0xff)
            return std::nullopt;
        return bytes;
    };
    auto decodeStateful = [&](int mode, int id, const Ieee802154Address& link, const Ipv6Address *outer) -> std::optional<AddressBytes> {
        auto context = contexts ? contexts->find(id, now) : nullptr;
        if (!context)
            return std::nullopt;
        std::vector<uint8_t> bytes;
        int length = mode == 1 ? 8 : mode == 2 ? 2 : 0;
        for (int i = 0; i < length; i++)
            bytes.push_back(read());
        return statefulUnicast(mode, *context, link, bytes, outer);
    };
    int sourceContext = header.getCid() ? header.getContextId() >> 4 : 0;
    int destinationContext = header.getCid() ? header.getContextId() & 15 : 0;
    if (header.getSac() && header.getSam() == 0)
        ipv6->setSrcAddress(Ipv6Address::UNSPECIFIED_ADDRESS);
    else {
        auto outer = encapsulating ? &encapsulating->getSrcAddress() : nullptr;
        auto address = header.getSac() ? decodeStateful(header.getSam(), sourceContext, source, outer) :
            decodeUnicast(header.getSam(), source, outer);
        if (!address)
            return nullptr;
        ipv6->setSrcAddress(ipv6Address(*address));
    }
    if (header.getM()) {
        AddressBytes bytes{};
        if (header.getDac()) {
            auto context = contexts ? contexts->find(destinationContext, now) : nullptr;
            if (!context || context->prefixLength > 64)
                return nullptr;
            bytes[0] = 0xff;
            bytes[1] = read();
            bytes[2] = read();
            bytes[3] = context->prefixLength;
            auto prefix = addressBytes(context->prefix);
            std::copy(prefix.begin(), prefix.begin() + 8, bytes.begin() + 4);
            for (int i = 12; i < 16; i++)
                bytes[i] = read();
        }
        else if (header.getDam() == 0) {
            for (auto& byte : bytes)
                byte = read();
            if (bytes[0] != 0xff)
                return nullptr;
        }
        else {
            bytes[0] = 0xff;
            bytes[1] = header.getDam() == 3 ? 2 : read();
            int start = header.getDam() == 1 ? 11 : header.getDam() == 2 ? 13 : 15;
            for (int i = start; i < 16; i++)
                bytes[i] = read();
        }
        ipv6->setDestAddress(ipv6Address(bytes));
    }
    else {
        auto outer = encapsulating ? &encapsulating->getDestAddress() : nullptr;
        auto address = header.getDac() ? decodeStateful(header.getDam(), destinationContext, destination, outer) :
            decodeUnicast(header.getDam(), destination, outer);
        if (!address)
            return nullptr;
        ipv6->setDestAddress(ipv6Address(*address));
    }
    ASSERT(cursor == header.getInlineDataArraySize());
    ipv6->setPayloadLength(B(originalSize - 40));
    return ipv6;
}

} } // namespace inet::lowpan
