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
    std::vector<Candidate> selected;
    for (size_t i = 0; i < eligible.size() && selected.empty(); ++i)
        for (size_t j = i + 1; j < eligible.size(); ++j)
            if (eligible[i].tid == eligible[j].tid &&
                    eligible[i].peer != eligible[j].peer &&
                    eligible[i].userPosition != eligible[j].userPosition) {
                selected = {eligible[i], eligible[j]};
                break;
            }
    if (selected.empty())
        return {};
    // Selection priority is independent of Group ID position, but the
    // immutable PHY layout must retain the positions previously ACKed in the
    // Group ID Management exchange.
    std::sort(selected.begin(), selected.end(), [] (const auto& left, const auto& right) {
        return left.userPosition < right.userPosition;
    });
    if (selected[0].userPosition != 0 || selected[1].userPosition != 1)
        return {};
    return selected;
}

} // namespace ieee80211
} // namespace inet
