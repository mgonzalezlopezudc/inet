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
};

struct Ieee802154MacBeaconConfirm { Ieee802154MacServiceResult result; };

struct Ieee802154MacBeaconRequestIndication
{
    Ieee802154Address sourceAddress;
    uint16_t destinationPanId;
};

} // namespace inet
#endif
