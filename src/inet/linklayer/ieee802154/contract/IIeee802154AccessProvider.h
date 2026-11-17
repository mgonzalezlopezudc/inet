// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154ACCESSPROVIDER_H
#define __INET_IIEEE802154ACCESSPROVIDER_H
#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"

namespace inet {
struct Ieee802154AccessRequest
{
    uint8_t minBe = 3;
    uint8_t maxBe = 5;
    uint8_t maxBackoffs = 4;
    simtime_t unitBackoffPeriod;
};

/** Single operation, increasing IDs; accepted borrowed inputs are copied. */
class INET_API IIeee802154AccessProvider
{
  public:
    virtual ~IIeee802154AccessProvider() = default;
    virtual Ieee802154MacServiceAdmission start(uint64_t id, const Ieee802154AccessRequest& request) = 0;
    virtual void handleCcaResult(uint64_t id, bool idle) = 0;
    virtual Ieee802154MacServiceCancelResult cancel(uint64_t id, Ieee802154MacServiceAbortReason reason) = 0;
};
}
#endif
