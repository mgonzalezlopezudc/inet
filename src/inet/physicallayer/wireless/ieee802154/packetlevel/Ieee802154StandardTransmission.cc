// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154StandardTransmission.h"
#include "inet/common/packet/Packet.h"

namespace inet {
namespace physicallayer {

std::array<uint8_t,6> Ieee802154StandardTransmission::getPhyHeader() const
{
    return {0,0,0,0,0xa7,uint8_t(getDataLength().get<b>()/8)};
}

std::vector<uint8_t> Ieee802154StandardTransmission::getPpduBytes() const
{
    auto header = getPhyHeader();
    std::vector<uint8_t> bytes(header.begin(),header.end());
    auto payload = getPacket()->peekDataAsBytes();
    bytes.insert(bytes.end(),payload->getBytes().begin(),payload->getBytes().end());
    return bytes;
}

} // namespace physicallayer
} // namespace inet
