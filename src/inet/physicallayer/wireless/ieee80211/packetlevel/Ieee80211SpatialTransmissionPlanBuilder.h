//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211SPATIALTRANSMISSIONPLANBUILDER_H
#define __INET_IEEE80211SPATIALTRANSMISSIONPLANBUILDER_H

#include <memory>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduDescription.h"

namespace inet {
namespace physicallayer {

/**
 * Builds the bounded, receiver-independent spatial plan for an HT mixed
 * PPDU.  The builder deliberately owns no transmitter or receiver state;
 * local capability decisions remain at the caller boundary.
 */
class INET_API Ieee80211SpatialTransmissionPlanBuilder final
{
  public:
    static std::shared_ptr<const SpatialTransmissionPlan> build(
        const Ieee80211HtPpduDescription& description,
        simtime_t totalPpduDuration,
        int numberOfTransmitAntennas,
        const std::vector<int>& orderedTransmitAntennaIndices = {});
};

} // namespace physicallayer
} // namespace inet

#endif
