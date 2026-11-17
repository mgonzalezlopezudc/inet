// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154StandardRadio.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee802154StandardRadio);

void Ieee802154StandardRadio::transmit(Packet *packet)
{
    Enter_Method("transmit");
    take(packet);
    handleUpperPacket(packet);
}

} // namespace physicallayer
} // namespace inet
