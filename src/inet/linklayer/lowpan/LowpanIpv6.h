// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANIPV6_H
#define __INET_LOWPANIPV6_H

#include "inet/networklayer/ipv6/Ipv6.h"

namespace inet { namespace lowpan {

// Native next-hop resolution is performed by the adaptation link contract.
class INET_API LowpanIpv6 : public Ipv6
{
  public:
    bool isNativeSourceUsable(NetworkInterface *interface, const Ipv6Address& source) const;
    // The exact associated ICMP module is inert for received InterfaceInd traffic.
    bool hasPolicyHooks() const {
        for (const auto& hook : hooks)
            if (hook.second != icmp.get() || typeid(*hook.second) != typeid(Icmpv6)) return true;
        return false;
    }
  protected:
    Ipv6Address fragmentNextHop;
    bool isNativeNdControl(const Packet *packet) const;
    virtual void resolveMACAddressAndSendPacket(Packet *packet, int interfaceId, Ipv6Address nextHop, bool fromHL) override;
    virtual void fragmentAndSend(Packet *packet) override;
    virtual void sendDatagramToOutput(Packet *packet, const NetworkInterface *destIE, const MacAddress& macAddr) override;
    virtual void localDeliverFinish(Packet *packet) override;
    virtual void handleReceivedIcmp(Packet *packet) override;
};

} } // namespace inet::lowpan
#endif
