// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef INET_IEE_E802154_MACGTSREQUEST_H
#define INET_IEE_E802154_MACGTSREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154GtsValues.h"
#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"
#include "inet/linklayer/ieee802154/Ieee802154Address.h"
#include "inet/linklayer/ieee802154/Ieee802154MacDataRequest.h"

namespace inet {

struct Ieee802154MacGtsRequest { Ieee802154GtsCharacteristics characteristics; Ieee802154MacSecurityParameters security; };
struct Ieee802154MacGtsConfirm { Ieee802154GtsCharacteristics characteristics; Ieee802154MacServiceResult result; };
struct Ieee802154MacGtsIndication { Ieee802154Address deviceAddress; Ieee802154GtsCharacteristics characteristics; };

} // namespace inet
#endif
