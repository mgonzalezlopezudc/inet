// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 4944 section 11.1: LOWPAN_BC0 dispatch and originator's sequence.
class LowpanBroadcastHeader extends inet::FieldsChunk
{
    chunkLength = B(2);
    uint8_t sequenceNumber = 0;
}
