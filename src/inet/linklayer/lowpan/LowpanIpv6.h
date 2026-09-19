// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANIPV6_H
#define __INET_LOWPANIPV6_H

#include "inet/networklayer/ipv6/Ipv6.h"

namespace inet { namespace lowpan {

// Native next-hop resolution is performed by the adaptation link contract.
class INET_API LowpanIpv6 : public Ipv6
{
  protected:
    Ipv6Address fragmentNextHop;
    virtual void resolveMACAddressAndSendPacket(Packet *packet, int interfaceId, Ipv6Address nextHop, bool fromHL) override;
    virtual void fragmentAndSend(Packet *packet) override;
    virtual void sendDatagramToOutput(Packet *packet, const NetworkInterface *destIE, const MacAddress& macAddr) override;
};

} } // namespace inet::lowpan
#endif
