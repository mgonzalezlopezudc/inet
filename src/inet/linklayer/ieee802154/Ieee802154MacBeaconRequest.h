// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACBEACONREQUEST_H
#define __INET_IEEE802154MACBEACONREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacScanRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacDataRequest.h"

namespace inet {

/** Selected non-beacon, unsecured legacy parameters of Table 8-22. */
struct Ieee802154MacBeaconRequest
{
    physicallayer::Ieee802154PhyChannel channel;
    bool enhanced = false;
    uint8_t superframeOrder = 15;
    Ieee802154MacSecurityParameters security;
    std::optional<Ieee802154Address::AddressType> sourceAddressMode;
    std::optional<Ieee802154Address> destinationAddress;
    bool sequenceSuppressed = false;
    bool panIdSuppressed = false;
    std::vector<Ieee802154InformationElements::Header> headerIes;
    std::vector<Ieee802154InformationElements::Payload> payloadIes;
    std::vector<uint8_t> requestedHeaderIds;
    std::vector<Ieee802154RequestedNestedIe> requestedNestedIds;
};

struct Ieee802154MacBeaconConfirm { Ieee802154MacServiceResult result; };

struct Ieee802154MacBeaconRequestIndication
{
    Ieee802154Address sourceAddress;
    std::optional<uint16_t> destinationPanId;
    bool enhanced = false;
    Ieee802154Address destinationAddress;
    std::optional<uint16_t> sourcePanId;
    std::optional<uint8_t> sequenceNumber;
    std::optional<Ieee802154EnhancedBeaconFilter> filter;
    std::vector<Ieee802154InformationElements::Header> headerIes;
    std::vector<Ieee802154InformationElements::Payload> payloadIes;
};

} // namespace inet
#endif
