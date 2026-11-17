// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

class LowpanRfragAckHeader extends inet::FieldsChunk
{
    chunkLength = B(6);
    bool ecn = false;
    uint8_t datagramTag = 0;
    uint32_t bitmap = 0; // Seq0 is the most significant bit; FULL includes unused one-bits.
}
