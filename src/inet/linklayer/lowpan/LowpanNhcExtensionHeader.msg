// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 6282 section 4.2. EID 2 has seven fixed bytes, including reserved;
// EID 7 is followed by a separate IPHC header and requires NH=0.
class LowpanNhcExtensionHeader extends inet::FieldsChunk
{
    chunkLength = B(3);
    uint8_t eid = 0;
    bool nh = false;
    uint8_t nextHeader = 59;
    uint8_t data[];
}
