// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;

namespace inet::lowpan;

// RFC 4944 section 5.1: uncompressed IPv6 dispatch.
class LowpanHeader extends inet::FieldsChunk
{
    chunkLength = B(1);
    uint8_t dispatch = 0x41;
}
