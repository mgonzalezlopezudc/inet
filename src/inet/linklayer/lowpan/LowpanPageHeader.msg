// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 8025: universal one-octet 1111pppp page switch.
class LowpanPageHeader extends inet::FieldsChunk
{
    chunkLength = B(1);
    uint8_t page = 0;
}
