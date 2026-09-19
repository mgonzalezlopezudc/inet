// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANLINKADDRESSMAP_H
#define __INET_LOWPANLINKADDRESSMAP_H

#include <map>
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee802154/Ieee802154Address.h"
#include "inet/networklayer/contract/ipv6/Ipv6Address.h"

namespace inet { namespace lowpan {

/** One delivery domain's validated configuration; no address bits are truncated. */
class INET_API LowpanLinkAddressMap
{
  protected:
    std::map<Ieee802154Address, MacAddress> aliases;
    std::map<MacAddress, Ieee802154Address> identities;
    std::map<std::pair<int, Ipv6Address>, Ieee802154Address> neighbors;

  public:
    void addPeer(const Ieee802154Address& nativeAddress, const MacAddress& alias);
    void bind(int interfaceModuleId, const Ipv6Address& nextHop, const Ieee802154Address& nativeAddress);
    const MacAddress *findAlias(const Ieee802154Address& nativeAddress) const;
    const Ieee802154Address *findIdentity(const MacAddress& alias) const;
    const Ieee802154Address *findNeighbor(int interfaceModuleId, const Ipv6Address& nextHop) const;
};

} } // namespace inet::lowpan
#endif
