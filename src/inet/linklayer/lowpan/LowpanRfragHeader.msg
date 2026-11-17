// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 8931 section 5.1: offsets and sizes are compressed bytes, not RFC 4944 units.
class LowpanRfragHeader extends inet::FieldsChunk
{
    chunkLength = B(6);
    bool ecn = false;
    uint8_t datagramTag = 0;
    bool ackRequested = false;
    uint8_t sequence = 0;
    uint16_t fragmentSize = 0;
    uint16_t fragmentOffset = 0; // Seq0: total compressed size. Zero: reset, for every sequence.
}
