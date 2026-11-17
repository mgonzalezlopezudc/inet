// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACASSOCIATERESPONSE_H
#define __INET_IEEE802154MACASSOCIATERESPONSE_H

#include "inet/linklayer/ieee802154/Ieee802154MacDataRequest.h"

namespace inet {

/** Selected MLME-ASSOCIATE.response fields (2024 Table 10-106); allocation is higher-layer policy. */
struct Ieee802154MacAssociateResponse
{
    Ieee802154Address deviceAddress;
    uint16_t shortAddress = 0xfffe;
    uint8_t associationStatus = 0; ///< Selected Table 10-102 values: 0 success, 1 capacity, 2 denied.
    Ieee802154MacSecurityParameters security;
};

/** Native correlated carrier of the response's eventual COMM-STATUS, not an IEEE response-confirm primitive. */
struct Ieee802154MacAssociateResponseStatus
{
    Ieee802154Address deviceAddress;
    Ieee802154MacServiceResult result;
};

} // namespace inet
#endif
