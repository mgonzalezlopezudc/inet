// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154ACCESSPROVIDER_H
#define __INET_IIEEE802154ACCESSPROVIDER_H
#include <optional>
#include "inet/common/INETDefs.h"
#include "inet/clock/contract/ClockTime.h"
#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"

namespace inet {
/** One immutable CAP epoch on the device clock; a complete exchange may end at a boundary. */
struct Ieee802154AccessWindow
{
    uint64_t epoch;
    clocktime_t start;
    clocktime_t end;
    clocktime_t backoffOrigin;
    std::optional<clocktime_t> bleEnd;
};

struct Ieee802154AccessRequest
{
    uint8_t minBe = 3;
    uint8_t maxBe = 5;
    uint8_t maxBackoffs = 4;
    simtime_t unitBackoffPeriod;
    std::optional<Ieee802154AccessWindow> window;
    simtime_t exchangeDuration = SIMTIME_ZERO; ///< Physical time from actual TX start through the exchange.
};

/** Single operation, increasing IDs; accepted borrowed inputs are copied. */
class INET_API IIeee802154AccessProvider
{
  public:
    virtual ~IIeee802154AccessProvider() = default;
    virtual Ieee802154MacServiceAdmission start(uint64_t id, const Ieee802154AccessRequest& request) = 0;
    /** Supply a later CAP to the same accepted operation; a stale ID cannot change it. */
    virtual Ieee802154MacServiceAdmission updateWindow(uint64_t id, const Ieee802154AccessWindow& window) = 0;
    virtual void handleCcaResult(uint64_t id, bool idle) = 0;
    virtual Ieee802154MacServiceCancelResult cancel(uint64_t id, Ieee802154MacServiceAbortReason reason) = 0;
};
}
#endif
