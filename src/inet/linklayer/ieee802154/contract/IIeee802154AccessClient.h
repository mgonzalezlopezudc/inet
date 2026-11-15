// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154ACCESSCLIENT_H
#define __INET_IIEEE802154ACCESSCLIENT_H
#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"

namespace inet {
/** Call-scoped callbacks; ordinary terminal reentry is allowed after detachment. */
class INET_API IIeee802154AccessClient
{
  public:
    virtual ~IIeee802154AccessClient() = default;
    virtual void handleCcaRequest(uint64_t id) = 0;
    virtual void handleAccessConfirm(uint64_t id, const Ieee802154MacServiceResult& result, uint8_t numBackoffs) = 0;
};
}
#endif
