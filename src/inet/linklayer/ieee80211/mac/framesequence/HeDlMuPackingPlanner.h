//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLMUPACKINGPLANNER_H
#define __INET_HEDLMUPACKINGPLANNER_H

#include <functional>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

class INET_API HeDlMuPackingPlanner
{
  public:
    struct SelectedAllocation {
        IIeee80211HeDlScheduler::RuAllocation allocation;
        queueing::IPacketQueue *sourceQueue = nullptr;
        Packet *packet = nullptr;
        Ptr<const Ieee80211DataHeader> dataHeader;
        std::vector<Packet *> packets;
        bool multiTidAggregation = false;
        B psduLength = B(0);
        uint16_t associationId = 0;
        int streamStartIndex = 0;
        bool muMimo = false;
        int totalNsts = 0;
    };

    struct Parameters {
        std::vector<SelectedAllocation> selectedAllocations;
        queueing::IPacketQueue *pendingQueue = nullptr;
        IIeee80211HeDlScheduler::ScheduleContext scheduleContext;
        int maxAmpduMpduCount = 16;
        int maxHeMuPsduLength = 6500631;
        simtime_t packingDurationLimit = SimTime(5.484, SIMTIME_MS);
        simtime_t ppduDurationLimit = SimTime(5.484, SIMTIME_MS);
        std::function<bool(const MacAddress&, Tid)> hasActiveBlockAckAgreement;
        std::function<int(const MacAddress&, Tid)> getAvailableBlockAckSlots;
        std::function<void(Packet *, const MacAddress&, Tid, int, const char *)> warnIneligible;
    };

    struct Plan {
        bool valid = false;
        std::string failureReason;
        std::vector<SelectedAllocation> allocations;
        physicallayer::Ieee80211HePhyValidationResult ppdu;
        int rejectedFinalValidation = 0;
        int durationTrimIterations = 0;

        explicit operator bool() const { return valid; }
    };

  public:
    static B calculateAmpduPsduLength(const std::vector<Packet *>& packets);
    Plan plan(const Parameters& parameters) const;
};

} // namespace ieee80211
} // namespace inet

#endif
