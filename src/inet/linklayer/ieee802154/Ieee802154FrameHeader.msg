// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.packet.chunk.Chunk;
import inet.linklayer.ieee802154.Ieee802154Address;
namespace inet;

// Selected unsecured 2006 data profile or immediate ACK; PAN compression is fixed.
class Ieee802154FrameHeader extends FieldsChunk
{
    chunkLength = B(21);
    uint16_t frameControl = 0xdc61;
    uint8_t sequenceNumber = 0;
    uint16_t panId = 0xffff;
    Ieee802154Address sourceAddress;
    Ieee802154Address destinationAddress;
}
