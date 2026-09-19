// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"

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

std::optional<AddressBytes> derivedAddress(const Ieee802154Address& address)
{
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

int encodeUnicast(const AddressBytes& address, const Ieee802154Address& link, std::vector<uint8_t>& data)
{
    if (address[0] == 0xfe && address[1] == 0x80 && zeroRange(address, 2, 8)) {
        auto derived = derivedAddress(link);
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
        const Ieee802154Address& source, const Ieee802154Address& destination, bool udpNhc)
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
    if (ipv6.getSrcAddress().isUnspecified()) {
        header->setSac(true);
        header->setSam(0);
    }
    else
        header->setSam(encodeUnicast(addressBytes(ipv6.getSrcAddress()), source, data));
    header->setM(ipv6.getDestAddress().isMulticast());
    header->setDam(header->getM() ? encodeMulticast(addressBytes(ipv6.getDestAddress()), data) :
        encodeUnicast(addressBytes(ipv6.getDestAddress()), destination, data));
    header->setInlineDataArraySize(data.size());
    for (size_t i = 0; i < data.size(); i++)
        header->setInlineData(i, data[i]);
    header->setChunkLength(B(2 + data.size()));
    header->markImmutable();
    return header;
}

Ptr<Ipv6Header> LowpanIphcCodec::decode(const LowpanIphcHeader& header, int originalSize,
        const Ieee802154Address& source, const Ieee802154Address& destination, bool udpNhc)
{
    int expected = getInlineLength(header);
    if (header.isIncorrect() || header.isIncomplete() || expected < 0 ||
        header.getInlineDataArraySize() != (size_t)expected || header.getChunkLength() != B(2 + header.getCid() + expected) ||
        originalSize < 40 || originalSize > 65575 || (header.getNh() && !udpNhc) || header.getDac() ||
        (header.getSac() && header.getSam() != 0))
        return nullptr;
    // CID selectors are unused by every supported stateless form, including ::.
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
    auto decodeUnicast = [&](int mode, const Ieee802154Address& link) -> std::optional<AddressBytes> {
        if (mode == 3)
            return derivedAddress(link);
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
    if (header.getSac())
        ipv6->setSrcAddress(Ipv6Address::UNSPECIFIED_ADDRESS);
    else {
        auto address = decodeUnicast(header.getSam(), source);
        if (!address)
            return nullptr;
        ipv6->setSrcAddress(ipv6Address(*address));
    }
    if (header.getM()) {
        AddressBytes bytes{};
        if (header.getDam() == 0) {
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
        auto address = decodeUnicast(header.getDam(), destination);
        if (!address)
            return nullptr;
        ipv6->setDestAddress(ipv6Address(*address));
    }
    ASSERT(cursor == header.getInlineDataArraySize());
    ipv6->setPayloadLength(B(originalSize - 40));
    return ipv6;
}

} } // namespace inet::lowpan
