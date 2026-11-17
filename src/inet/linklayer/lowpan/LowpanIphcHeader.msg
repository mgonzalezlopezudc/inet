// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
namespace inet::lowpan;

// RFC 6282 section 3.1. Inline bytes follow the IPv6 field order.
class LowpanIphcHeader extends inet::FieldsChunk
{
    chunkLength = B(2);
    uint8_t tf = 3;
    bool nh = false;
    uint8_t hlim = 2;
    bool cid = false;
    bool sac = false;
    uint8_t sam = 0;
    bool m = false;
    bool dac = false;
    uint8_t dam = 0;
    uint8_t contextId = 0;
    uint8_t inlineData[];
}
