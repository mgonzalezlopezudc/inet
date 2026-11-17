// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_ILOWPANLINK_H
#define __INET_ILOWPANLINK_H

#include "inet/linklayer/lowpan/contract/ILowpanMac.h"
#include "inet/networklayer/contract/ipv6/Ipv6Address.h"
#include "inet/queueing/contract/IPassivePacketSink.h"

namespace inet { namespace lowpan {

class INET_API ILowpanLink : public virtual queueing::IPassivePacketSink
{
  public:
    virtual Ieee802154Address getLinkAddress() const = 0;
    virtual uint16_t getPanId() const = 0;
    virtual const Ieee802154Address *resolveNeighbor(const Ipv6Address& nextHop) const = 0;
    virtual Ptr<const LowpanTransmissionReq> prepareTransmission(const Ieee802154AddressReq& request) const = 0;
};

} } // namespace inet::lowpan
#endif
