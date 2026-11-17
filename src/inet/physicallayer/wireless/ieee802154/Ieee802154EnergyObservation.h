// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154ENERGYOBSERVATION_H
#define __INET_IEEE802154ENERGYOBSERVATION_H

#include <map>
#include <memory>
#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IListening.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReception.h"

namespace inet {
namespace physicallayer {

/** One scalar observation; the PHY owner forwards medium arrival events during its interval. */
class INET_API Ieee802154EnergyObservation
{
  public:
    struct Result {
        units::values::W peakPower;
        units::values::W meanPower;
    };

  private:
    struct Contribution {
        simtime_t start;
        simtime_t end;
        units::values::W power;
    };
    const IRadio *radio = nullptr;
    std::unique_ptr<const IListening> listening;
    std::map<int, Contribution> contributions;

  public:
    void begin(const IRadio *radio, simtime_t start, simtime_t end);
    void include(const IReception *reception);
    Result finish();
    void clear();
};

} // namespace physicallayer
} // namespace inet
#endif
