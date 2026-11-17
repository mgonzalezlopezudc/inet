// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACPOLLREQUEST_H
#define __INET_IEEE802154MACPOLLREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacDataRequest.h"

namespace inet {

struct Ieee802154MacPollRequest
{
    uint16_t coordinatorPanId = 0xffff;
    Ieee802154Address coordinatorAddress;
    Ieee802154MacSecurityParameters security;
};

struct Ieee802154MacPollConfirm
{
    Ieee802154MacServiceResult result;
};

struct Ieee802154MacPurgeConfirm
{
    uint8_t msduHandle;
    Ieee802154MacServiceResult result;
};

} // namespace inet
#endif
