//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEE80211HEULSCHEDULER_H
#define __INET_IIEE80211HEULSCHEDULER_H

#include <array>
#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

class INET_API IIeee80211HeUlScheduler
{
  public:
    struct CandidateInfo {
        MacAddress staAddress;
        uint16_t associationId = 0;
        std::array<int64_t, 4> backlogBytes = {};
        AccessCategory selectedAccessCategory = AC_BE;
        uint8_t selectedTid = 0;
        simtime_t reportAge = SIMTIME_MAX;
        bool hasFreshReport = false;
        bool retryPending = false;
        bool anchor = false;
        double pathLossDb = NaN;
        bool hasFreshPathLoss = false;
        simtime_t lastService = SIMTIME_ZERO;

        int64_t getSelectedBacklogBytes() const { return backlogBytes[selectedAccessCategory]; }
    };

    struct ScheduleContext {
        std::vector<CandidateInfo> candidates;
        Hz channelCenterFrequency = Hz(NaN);
        Hz channelBandwidth = Hz(NaN);
        simtime_t txopLimit = SIMTIME_ZERO;
        simtime_t requestedDuration = SimTime(1, SIMTIME_MS);
        double apSensitivityDbm = -85;
        double targetRssiMarginDb = 10;
        int estimatedRandomAccessContenders = 0;
        double recentRandomAccessCollisionRate = 0;
        double recentRandomAccessIdleRate = 0;
    };

    struct RuAllocation {
        MacAddress staAddress;
        uint16_t associationId = 0;
        uint8_t tid = 0;
        AccessCategory accessCategory = AC_BE;
        physicallayer::Ieee80211HeRu ru;
        bool randomAccess = false;
        int mcs = 0;
        int targetRssiDbm = -75;
        simtime_t estimatedDuration = SIMTIME_ZERO;
    };

    struct Schedule {
        std::vector<RuAllocation> allocations;
        simtime_t commonDuration = SIMTIME_ZERO;
        physicallayer::Ieee80211HeGuardInterval guardInterval = physicallayer::HE_GI_3_2_US;
        physicallayer::Ieee80211HeCoding coding = physicallayer::HE_CODING_BCC;
        int packetExtensionDurationUs = 0;
        uint8_t puncturedSubchannelMask = 0;
    };

    virtual ~IIeee80211HeUlScheduler() {}
    virtual Schedule schedule(const ScheduleContext& context) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
