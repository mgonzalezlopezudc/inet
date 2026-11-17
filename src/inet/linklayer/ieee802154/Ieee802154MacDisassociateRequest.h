// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACDISASSOCIATEREQUEST_H
#define __INET_IEEE802154MACDISASSOCIATEREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacDataRequest.h"

namespace inet {

struct Ieee802154MacDisassociateRequest
{
    uint16_t devicePanId;
    Ieee802154Address deviceAddress;
    uint8_t reason; ///< Table 10-103: 1 coordinator removal, 2 device leave.
    bool indirectTx = false;
    Ieee802154MacSecurityParameters security;
};

struct Ieee802154MacDisassociateConfirm
{
    uint16_t peerPanId;
    Ieee802154Address peerAddress;
    Ieee802154MacServiceResult result;
};

struct Ieee802154MacDisassociateIndication
{
    Ieee802154Address deviceAddress;
    uint8_t reason;
};

} // namespace inet
#endif
