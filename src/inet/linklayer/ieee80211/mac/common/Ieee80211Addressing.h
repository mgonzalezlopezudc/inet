//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211ADDRESSING_H
#define __INET_IEEE80211ADDRESSING_H

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

struct Ieee80211AddressRoles
{
    MacAddress receiverAddress = MacAddress::UNSPECIFIED_ADDRESS;
    MacAddress transmitterAddress = MacAddress::UNSPECIFIED_ADDRESS;
    MacAddress destinationAddress = MacAddress::UNSPECIFIED_ADDRESS;
    MacAddress sourceAddress = MacAddress::UNSPECIFIED_ADDRESS;
    MacAddress bssid = MacAddress::UNSPECIFIED_ADDRESS;
    bool hasTransmitterAddress = false;
    bool hasDataAddressRoles = false;
    bool supported = true;
};

/**
 * Interprets the represented IEEE 802.11 address fields.
 *
 * Address 1 and Address 2 are always RA and TA when present. The DA, SA, and
 * BSSID roles are derived only for the nonmesh data combinations represented
 * by Ieee80211DataHeader. Mesh and GLK-specific address interpretation is not
 * represented by this helper and is reported as unsupported.
 */
inline Ieee80211AddressRoles interpretIeee80211AddressRoles(const Ptr<const Ieee80211MacHeader>& header)
{
    Ieee80211AddressRoles roles;
    if (header == nullptr) {
        roles.supported = false;
        return roles;
    }

    roles.receiverAddress = header->getReceiverAddress();
    if (auto twoAddressHeader = dynamicPtrCast<const Ieee80211TwoAddressHeader>(header)) {
        roles.transmitterAddress = twoAddressHeader->getTransmitterAddress();
        roles.hasTransmitterAddress = true;
    }

    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
    if (dataHeader == nullptr)
        return roles;

    roles.hasDataAddressRoles = true;
    const bool toDs = dataHeader->getToDS();
    const bool fromDs = dataHeader->getFromDS();
    const auto address1 = dataHeader->getReceiverAddress();
    const auto address2 = dataHeader->getTransmitterAddress();
    const auto address3 = dataHeader->getAddress3();

    if (!toDs && !fromDs) {
        roles.destinationAddress = address1;
        roles.sourceAddress = address2;
        roles.bssid = address3;
    }
    else if (!toDs && fromDs) {
        roles.destinationAddress = address1;
        roles.sourceAddress = address3;
        roles.bssid = address2;
    }
    else if (toDs && !fromDs) {
        roles.destinationAddress = address3;
        roles.sourceAddress = address2;
        roles.bssid = address1;
    }
    else {
        roles.destinationAddress = address3;
        roles.sourceAddress = dataHeader->getAddress4();
    }
    return roles;
}

} // namespace ieee80211
} // namespace inet

#endif
