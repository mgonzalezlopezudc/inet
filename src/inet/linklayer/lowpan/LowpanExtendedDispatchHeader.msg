// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 8066: ESC followed by EET, with no generic payload length.
class LowpanExtendedDispatchHeader extends inet::FieldsChunk
{
    chunkLength = B(2);
    uint8_t extensionType = 0;
}
