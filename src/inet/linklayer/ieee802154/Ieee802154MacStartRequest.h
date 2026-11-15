// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACSTARTREQUEST_H
#define __INET_IEEE802154MACSTARTREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"
#include "inet/physicallayer/wireless/ieee802154/contract/Ieee802154PhyServicePrimitives.h"

namespace inet {

/** Non-beacon START service, IEEE Std 802.15.4-2024, Table 8-20. */
struct Ieee802154MacStartRequest
{
    uint16_t panId;
    physicallayer::Ieee802154PhyChannel channel;
};

struct Ieee802154MacStartConfirm
{
    Ieee802154MacServiceResult result;
};

} // namespace inet
#endif
