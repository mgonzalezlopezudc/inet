// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 6282 section 4.3; checksum elision is representable but unsupported.
class LowpanUdpNhcHeader extends inet::FieldsChunk
{
    chunkLength = B(7);
    bool checksumElided = false;
    uint8_t portMode = 0;
    uint16_t sourcePort = 0;
    uint16_t destinationPort = 0;
    uint16_t checksum = 0;
}
