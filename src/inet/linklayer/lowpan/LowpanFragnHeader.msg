// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 4944 section 5.3; datagram size and offsets refer to original IPv6 bytes.
class LowpanFragnHeader extends inet::FieldsChunk
{
    chunkLength = B(5);
    uint16_t datagramSize = 0;
    uint16_t datagramTag = 0;
    uint8_t datagramOffset = 0; // Eight-octet units, never zero in FRAGN.
}
