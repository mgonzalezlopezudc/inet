// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154EXCHANGETIMING_H
#define __INET_IEEE802154EXCHANGETIMING_H

#include "inet/physicallayer/wireless/ieee802154/contract/Ieee802154PhyServicePrimitives.h"

namespace inet {

/** Physical durations for the selected legacy O-QPSK exchange. */
class INET_API Ieee802154ExchangeTiming
{
  public:
    static simtime_t ppduDuration(units::values::b psduLength, const physicallayer::Ieee802154PhyTimingParameters& phy);
    static simtime_t interframeSpacing(units::values::b psduLength, simtime_t symbolPeriod, uint32_t sifsSymbols, uint32_t lifsSymbols);
    static simtime_t ackDuration(const physicallayer::Ieee802154PhyTimingParameters& phy);
    static simtime_t ackWait(const physicallayer::Ieee802154PhyTimingParameters& phy);
    static simtime_t ccaAndReadiness(simtime_t ccaDuration, const physicallayer::Ieee802154PhyTimingParameters& phy);
    static simtime_t exchangeDuration(units::values::b psduLength, bool acknowledged, simtime_t propagationBound,
            uint32_t sifsSymbols, uint32_t lifsSymbols, const physicallayer::Ieee802154PhyTimingParameters& phy);
    /** A complete exchange ending exactly at the boundary fits. */
    static bool fits(simtime_t start, simtime_t end, simtime_t duration);
};

} // namespace inet
#endif
