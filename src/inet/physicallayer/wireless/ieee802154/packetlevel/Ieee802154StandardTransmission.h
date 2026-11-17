// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154STANDARDTRANSMISSION_H
#define __INET_IEEE802154STANDARDTRANSMISSION_H

#include <array>
#include <vector>
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154Transmission.h"

namespace inet {
namespace physicallayer {

/** Packet-level O-QPSK PPDU; the medium packet is its PSDU, with timed PHY overhead. */
class INET_API Ieee802154StandardTransmission : public Ieee802154Transmission
{
  public:
    using Ieee802154Transmission::Ieee802154Transmission;
    std::array<uint8_t,6> getPhyHeader() const;
    std::vector<uint8_t> getPpduBytes() const;
};

} // namespace physicallayer
} // namespace inet
#endif
