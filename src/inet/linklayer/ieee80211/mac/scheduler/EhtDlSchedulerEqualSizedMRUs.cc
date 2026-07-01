//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/EhtDlSchedulerEqualSizedMRUs.h"

namespace inet {
namespace ieee80211 {

Define_Module(EhtDlSchedulerEqualSizedMRUs);

void EhtDlSchedulerEqualSizedMRUs::initialize(int stage)
{
    HeDlSchedulerEqualSizedRUs::initialize(stage);
}

std::vector<IIeee80211HeDlScheduler::RuAllocation> EhtDlSchedulerEqualSizedMRUs::schedule(const ScheduleContext& context)
{
    // For now, reuse the HE logic.
    // In a full implementation, we'd assign MRUs (e.g. 52+26-tone MRU) instead of basic RUs
    return HeDlSchedulerEqualSizedRUs::schedule(context);
}

} // namespace ieee80211
} // namespace inet
