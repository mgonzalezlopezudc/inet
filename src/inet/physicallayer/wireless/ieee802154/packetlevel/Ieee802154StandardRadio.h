// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154STANDARDRADIO_H
#define __INET_IEEE802154STANDARDRADIO_H
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatRadioBase.h"

namespace inet {
namespace physicallayer {

/** Internal synchronous radio entry: accepted ownership and PPDU start share one call. */
class INET_API Ieee802154StandardRadio : public FlatRadioBase
{
  public:
    void transmit(Packet *packet);
};

} // namespace physicallayer
} // namespace inet
#endif
