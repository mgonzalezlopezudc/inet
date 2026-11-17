// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACSYNCREQUEST_H
#define __INET_IEEE802154MACSYNCREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"
#include "inet/physicallayer/wireless/ieee802154/contract/Ieee802154PhyServicePrimitives.h"

namespace inet {

/** IEEE 802.15.4-2024 Table 10-3 parameters for the selected PHY. */
struct Ieee802154MacSyncRequest
{
    physicallayer::Ieee802154PhyChannel channel;
    bool trackBeacon = false;
};

/** Local installation completion, not an IEEE SYNC confirmation. */
struct Ieee802154MacSyncConfirm { Ieee802154MacServiceResult result; };

} // namespace inet
#endif
