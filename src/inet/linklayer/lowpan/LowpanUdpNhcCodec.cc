// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanUdpNhcCodec.h"
namespace inet { namespace lowpan {
int LowpanUdpNhcCodec::getWireLength(int portMode, bool checksumElided)
{
    static const int lengths[] = {7, 6, 6, 4};
    return portMode >= 0 && portMode < 4 ? lengths[portMode] - (checksumElided ? 2 : 0) : -1;
}

Ptr<const LowpanUdpNhcHeader> LowpanUdpNhcCodec::encode(const std::vector<uint8_t>& bytes, int ipv6PayloadLength)
{
    if (bytes.size() != 8 || ipv6PayloadLength < 8 || ipv6PayloadLength > 65535 ||
        (int(bytes[4]) << 8 | bytes[5]) != ipv6PayloadLength)
        return nullptr;
    auto header = makeShared<LowpanUdpNhcHeader>();
    int source = int(bytes[0]) << 8 | bytes[1];
    int destination = int(bytes[2]) << 8 | bytes[3];
    int mode = (source & 0xfff0) == 0xf0b0 && (destination & 0xfff0) == 0xf0b0 ? 3 :
        (destination & 0xff00) == 0xf000 ? 1 : (source & 0xff00) == 0xf000 ? 2 : 0;
    header->setPortMode(mode);
    header->setSourcePort(source);
    header->setDestinationPort(destination);
    header->setChecksum(int(bytes[6]) << 8 | bytes[7]);
    header->setChunkLength(B(getWireLength(mode)));
    header->markImmutable();
    return header;
}

Ptr<const BytesChunk> LowpanUdpNhcCodec::decode(const LowpanUdpNhcHeader& header, int ipv6PayloadLength)
{
    if (header.isIncomplete() || header.isIncorrect() || header.getChecksumElided() ||
        header.getPortMode() > 3 || header.getChunkLength() != B(getWireLength(header.getPortMode())) ||
        ipv6PayloadLength < 8 || ipv6PayloadLength > 65535)
        return nullptr;
    return makeShared<BytesChunk>(std::vector<uint8_t>{
        uint8_t(header.getSourcePort() >> 8), uint8_t(header.getSourcePort()),
        uint8_t(header.getDestinationPort() >> 8), uint8_t(header.getDestinationPort()),
        uint8_t(ipv6PayloadLength >> 8), uint8_t(ipv6PayloadLength),
        uint8_t(header.getChecksum() >> 8), uint8_t(header.getChecksum())});
}
} } // namespace inet::lowpan
