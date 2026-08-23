//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTCAPABILITIES_H
#define __INET_IEEE80211HTCAPABILITIES_H

#include <cstdint>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/** Immutable radio-owned subset of local HT PHY capabilities. */
class INET_API Ieee80211HtCapabilities final
{
  private:
    const bool txStbc;
    const uint8_t rxStbc;
    const uint8_t maximumSupportedMcs;
    const uint8_t maximumSupportedSpatialStreams;
    const bool ht20Supported;
    const bool ht40Supported;

  public:
    Ieee80211HtCapabilities(bool txStbc, int rxStbc, int maximumSupportedMcs,
        int maximumSupportedSpatialStreams, bool ht20Supported, bool ht40Supported);

    bool getTxStbc() const { return txStbc; }
    uint8_t getRxStbc() const { return rxStbc; }
    uint8_t getMaximumSupportedMcs() const { return maximumSupportedMcs; }
    uint8_t getMaximumSupportedSpatialStreams() const { return maximumSupportedSpatialStreams; }
    bool getHt20Supported() const { return ht20Supported; }
    bool getHt40Supported() const { return ht40Supported; }
    bool supportsMcs(int mcs) const { return mcs >= 0 && mcs <= maximumSupportedMcs; }
    bool supportsChannelWidth(Hz bandwidth) const {
        return (bandwidth == MHz(20) && ht20Supported) ||
            (bandwidth == MHz(40) && ht40Supported);
    }
    void validateTransmission(int mcs, int numberOfSpatialStreams,
        Hz bandwidth, bool spaceTimeCoded) const;
};

} // namespace physicallayer
} // namespace inet

#endif
