// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 4944 section 5.3; datagram size and offsets refer to original IPv6 bytes.
class LowpanFrag1Header extends inet::FieldsChunk
{
    chunkLength = B(4);
    uint16_t datagramSize = 0;
    uint16_t datagramTag = 0;
}
