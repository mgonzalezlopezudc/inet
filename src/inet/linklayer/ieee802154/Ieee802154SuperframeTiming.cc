// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154SuperframeTiming.h"

namespace inet {

std::optional<Ieee802154SuperframeGeometry> Ieee802154SuperframeTiming::geometry(unsigned int beaconOrder, unsigned int superframeOrder)
{
    if (beaconOrder > 15 || superframeOrder > 15 || (beaconOrder < 15 && superframeOrder != 15 && superframeOrder > beaconOrder))
        return {};
    if (beaconOrder == 15)
        return Ieee802154SuperframeGeometry{15, 15, {}, {}, {}};
    auto interval = uint32_t(960) << beaconOrder;
    if (superframeOrder == 15)
        return Ieee802154SuperframeGeometry{uint8_t(beaconOrder), 15, interval, {}, {}};
    return Ieee802154SuperframeGeometry{uint8_t(beaconOrder), uint8_t(superframeOrder), interval,
            uint32_t(960) << superframeOrder, uint32_t(60) << superframeOrder};
}

uint64_t Ieee802154SuperframeTiming::roundStartTime(uint32_t symbols, uint32_t backoffPeriod)
{
    if (backoffPeriod == 0)
        throw cRuntimeError("Backoff period must be positive");
    return ((uint64_t(symbols) + backoffPeriod - 1) / backoffPeriod) * backoffPeriod;
}

bool Ieee802154SuperframeTiming::physicalIntervalsOverlap(simtime_t firstStart, simtime_t firstEnd,
        simtime_t secondStart, simtime_t secondEnd)
{
    return firstStart < firstEnd && secondStart < secondEnd && firstStart < secondEnd && secondStart < firstEnd;
}

uint32_t Ieee802154SuperframeTiming::timestamp(clocktime_t localTime)
{
    auto ticks = CLOCKTIME_AS_SIMTIME(localTime).raw();
    auto symbolTicks = SimTime(16, SIMTIME_US).raw();
    auto symbols = ticks / symbolTicks;
    if (ticks % symbolTicks < 0)
        --symbols;
    return uint32_t(uint64_t(symbols) & 0xffffff);
}

} // namespace inet
