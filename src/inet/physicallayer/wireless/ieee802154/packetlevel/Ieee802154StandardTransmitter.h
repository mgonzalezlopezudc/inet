// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154STANDARDTRANSMITTER_H
#define __INET_IEEE802154STANDARDTRANSMITTER_H

#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee802154StandardTransmitter : public FlatTransmitterBase
{
  public:
    virtual const ITransmission *createTransmission(const IRadio *radio, const Packet *packet, simtime_t startTime) const override;
};

} // namespace physicallayer
} // namespace inet
#endif
