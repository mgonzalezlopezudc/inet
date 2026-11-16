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
    std::optional<clocktime_t> bleStart; ///< First complete BLE backoff period after the beacon IFS.
    simtime_t notBefore = SIMTIME_ZERO; ///< Earliest physical instant allowed by this sender's IFS.
    simtime_t physicalCapStart = SIMTIME_ZERO; ///< Actual beacon end, before any sender-specific IFS.
    simtime_t physicalCapEnd = SIMTIME_ZERO; ///< CAP end converted while still future.
    std::optional<simtime_t> latestCompletion; ///< Earlier local radio reservation, if any.
    uint8_t beaconOrder = 15; ///< Actual beacon epoch, retained if the tracker or PIB changes.
};

struct Ieee802154AccessRequest
{
    uint8_t minBe = 3;
    uint8_t maxBe = 5;
    uint8_t maxBackoffs = 4;
    simtime_t unitBackoffPeriod; ///< Nominal duration in local-clock units for slotted access.
    std::optional<Ieee802154AccessWindow> window;
    simtime_t exchangeDuration = SIMTIME_ZERO; ///< Physical time from actual TX start through the exchange.
    simtime_t ifsDuration = SIMTIME_ZERO; ///< Tail of exchangeDuration that does not occupy the radio.
    simtime_t ccaDuration = SIMTIME_ZERO; ///< Physical duration of one PHY CCA.
    simtime_t turnaroundTime = SIMTIME_ZERO; ///< Physical TX readiness after the second CCA.
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
    /** Return an unperformed slotted CCA for re-alignment after local radio unavailability. */
    virtual void deferCca(uint64_t id, simtime_t notBefore) = 0;
    virtual Ieee802154MacServiceCancelResult cancel(uint64_t id, Ieee802154MacServiceAbortReason reason) = 0;
};
}
#endif
