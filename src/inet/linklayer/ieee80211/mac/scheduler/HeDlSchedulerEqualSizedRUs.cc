//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(HeDlSchedulerEqualSizedRUs);

void HeDlSchedulerEqualSizedRUs::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        maxMuStations = par("maxMuStations");
    }
}

std::vector<IIeee80211HeDlScheduler::RuAllocation>
HeDlSchedulerEqualSizedRUs::schedule(
        const std::vector<MacAddress>& candidates,
        Hz channelCenterFrequency,
        Hz channelBandwidth)
{
    int numSelected = std::min((int)candidates.size(), maxMuStations);
    std::vector<Ieee80211HeRu> rus = calculateHeRus(channelCenterFrequency, channelBandwidth, numSelected);

    std::vector<RuAllocation> result;
    result.reserve(numSelected);
    for (int i = 0; i < numSelected; ++i) {
        RuAllocation alloc;
        alloc.staAddress = candidates[i];
        alloc.ru = rus[i];
        result.push_back(alloc);
    }
    return result;
}

} // namespace ieee80211
} // namespace inet
