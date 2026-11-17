// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154SUPERFRAMETIMING_H
#define __INET_IEEE802154SUPERFRAMETIMING_H

#include <cstdint>
#include <optional>

#include "inet/clock/contract/ClockTime.h"

namespace inet {

/** Geometry in symbols; an absent active duration identifies beacon-only or nonbeacon mode. */
struct INET_API Ieee802154SuperframeGeometry
{
    uint8_t beaconOrder;
    uint8_t superframeOrder;
    std::optional<uint32_t> beaconInterval;
    std::optional<uint32_t> activeDuration;
    std::optional<uint32_t> slotDuration;
};

class INET_API Ieee802154SuperframeTiming
{
  public:
    /** Invalid order combinations return no geometry. BO=15 normalizes SO to 15. */
    static std::optional<Ieee802154SuperframeGeometry> geometry(unsigned int beaconOrder, unsigned int superframeOrder);
    /** Low 24 bits of the local-clock symbol count, rounded down at symbol boundaries. */
    static uint32_t timestamp(clocktime_t localTime);
};

} // namespace inet
#endif
