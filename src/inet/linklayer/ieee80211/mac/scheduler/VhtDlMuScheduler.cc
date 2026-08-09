//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/VhtDlMuScheduler.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

Define_Module(VhtDlMuScheduler);

std::vector<IIeee80211VhtDlMuScheduler::Candidate>
VhtDlMuScheduler::schedule(const Context& context) const
{
    std::vector<Candidate> eligible;
    for (const auto& candidate : context.candidates)
        if (IIeee80211VhtDlMuScheduler::isEligible(context, candidate))
            eligible.push_back(candidate);
    std::stable_sort(eligible.begin(), eligible.end(), [] (const auto& left, const auto& right) {
        if (left.enqueueTime != right.enqueueTime)
            return left.enqueueTime < right.enqueueTime;
        if (left.associationId != right.associationId)
            return left.associationId < right.associationId;
        return left.peer < right.peer;
    });
    if (eligible.size() < 2)
        return {};
    for (const auto& anchor : eligible) {
        std::vector<Candidate> selected;
        for (int position = 0; position < context.transmitDimensions; ++position) {
            auto it = std::find_if(eligible.begin(), eligible.end(), [&] (const auto& candidate) {
                return candidate.tid == anchor.tid && candidate.userPosition == position;
            });
            if (it == eligible.end())
                break;
            selected.push_back(*it);
        }
        if (selected.size() >= 2)
            return selected;
    }
    return {};
}

} // namespace ieee80211
} // namespace inet
