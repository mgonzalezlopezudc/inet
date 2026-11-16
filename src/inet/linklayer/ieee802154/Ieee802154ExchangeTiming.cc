// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154ExchangeTiming.h"

namespace inet {

simtime_t Ieee802154ExchangeTiming::ppduDuration(units::values::b psduLength, const physicallayer::Ieee802154PhyTimingParameters& phy)
{
    return phy.shrDuration + phy.phrDuration + SimTime(psduLength.get<units::values::b>() / phy.bitrate.get<units::values::bps>());
}

simtime_t Ieee802154ExchangeTiming::interframeSpacing(units::values::b psduLength, simtime_t symbolPeriod, uint32_t sifsSymbols, uint32_t lifsSymbols)
{
    return symbolPeriod * (psduLength <= units::values::B(18) ? sifsSymbols : lifsSymbols);
}

simtime_t Ieee802154ExchangeTiming::ackDuration(const physicallayer::Ieee802154PhyTimingParameters& phy)
{
    return ppduDuration(units::values::B(5),phy);
}

simtime_t Ieee802154ExchangeTiming::ackWait(const physicallayer::Ieee802154PhyTimingParameters& phy)
{
    return phy.turnaroundTime + ackDuration(phy);
}

simtime_t Ieee802154ExchangeTiming::ccaAndReadiness(simtime_t ccaDuration, const physicallayer::Ieee802154PhyTimingParameters& phy)
{
    return ccaDuration + phy.turnaroundTime;
}

simtime_t Ieee802154ExchangeTiming::exchangeDuration(units::values::b psduLength, bool acknowledged, simtime_t propagationBound,
        uint32_t sifsSymbols, uint32_t lifsSymbols, const physicallayer::Ieee802154PhyTimingParameters& phy)
{
    return ppduDuration(psduLength,phy) + interframeSpacing(psduLength,phy.symbolPeriod,sifsSymbols,lifsSymbols) +
            (acknowledged ? ackWait(phy) + 2 * propagationBound : SIMTIME_ZERO);
}

bool Ieee802154ExchangeTiming::fits(simtime_t start, simtime_t end, simtime_t duration)
{
    return duration >= SIMTIME_ZERO && start <= end && duration <= end - start;
}

} // namespace inet
