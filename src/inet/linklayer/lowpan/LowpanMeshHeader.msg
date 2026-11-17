// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
import inet.linklayer.ieee802154.Ieee802154Address;
namespace inet::lowpan;

// RFC 4944 sections 5.2 and 11: logical identities, independent of each MAC hop.
class LowpanMeshHeader extends inet::FieldsChunk
{
    chunkLength = B(17);
    Ieee802154Address originator;
    Ieee802154Address finalDestination;
    uint8_t hopsLeft = 0;
    bool deepHops = false; // Retain the received wire form while forwarding.
}
