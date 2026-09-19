// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANIPV6NEIGHBOURDISCOVERY_H
#define __INET_LOWPANIPV6NEIGHBOURDISCOVERY_H

#include "inet/networklayer/icmpv6/Ipv6NeighbourDiscovery.h"

namespace inet { namespace lowpan {

// Static native bindings: ordinary RFC 4861 remains active on other interfaces.
class INET_API LowpanIpv6NeighbourDiscovery : public Ipv6NeighbourDiscovery
{
  protected:
    virtual void start() override;
    virtual void initiateDad(const Ipv6Address& tentativeAddr, NetworkInterface *ie) override;
    virtual void startRouterDiscovery(NetworkInterface *ie) override;
    virtual void createRaTimer(NetworkInterface *ie) override;
    virtual void processNDMessage(Packet *packet, const Icmpv6Header *message) override;

  public:
    virtual void sendRedirect(Packet *redirectedPacket, const Ipv6Address& targetAddr,
            const Ipv6Address& destAddr, NetworkInterface *ie) override;
};

} } // namespace inet::lowpan
#endif
