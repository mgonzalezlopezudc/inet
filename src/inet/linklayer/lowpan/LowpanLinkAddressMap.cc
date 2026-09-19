// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanLinkAddressMap.h"

namespace inet { namespace lowpan {

void LowpanLinkAddressMap::addPeer(const Ieee802154Address& nativeAddress, const MacAddress& alias)
{
    if (nativeAddress.isUnspecified() || nativeAddress.isBroadcast() ||
        (nativeAddress.getMode() == Ieee802154Address::SHORT && nativeAddress.getInt() == 0xfffe) ||
        alias.isUnspecified() || alias.isMulticast())
        throw cRuntimeError("LoWPAN peer requires a native unicast identity and legacy unicast alias");
    if (aliases.count(nativeAddress) || identities.count(alias))
        throw cRuntimeError("Duplicate LoWPAN native identity or compatibility alias");
    aliases.emplace(nativeAddress, alias);
    identities.emplace(alias, nativeAddress);
}

void LowpanLinkAddressMap::bind(int interfaceModuleId, const Ipv6Address& nextHop, const Ieee802154Address& nativeAddress)
{
    if (interfaceModuleId < 0 || nextHop.isUnspecified() || nextHop.isMulticast() || !aliases.count(nativeAddress))
        throw cRuntimeError("Invalid LoWPAN static next-hop binding");
    if (!neighbors.emplace(std::make_pair(interfaceModuleId, nextHop), nativeAddress).second)
        throw cRuntimeError("Duplicate LoWPAN static next-hop binding");
}

const MacAddress *LowpanLinkAddressMap::findAlias(const Ieee802154Address& nativeAddress) const
{
    auto it = aliases.find(nativeAddress);
    return it == aliases.end() ? nullptr : &it->second;
}

const Ieee802154Address *LowpanLinkAddressMap::findIdentity(const MacAddress& alias) const
{
    auto it = identities.find(alias);
    return it == identities.end() ? nullptr : &it->second;
}

const Ieee802154Address *LowpanLinkAddressMap::findNeighbor(int interfaceModuleId, const Ipv6Address& nextHop) const
{
    auto it = neighbors.find(std::make_pair(interfaceModuleId, nextHop));
    return it == neighbors.end() ? nullptr : &it->second;
}

} } // namespace inet::lowpan
