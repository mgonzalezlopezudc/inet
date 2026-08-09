//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/VhtDlMuScheduler.h"

#include <algorithm>
#include <set>

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
        std::set<int> positions;
        int totalNsts = 0;
        int receiverTotalLimit = 8;
        int soundingLimit = 8;
        for (const auto& candidate : eligible) {
            if (candidate.tid != anchor.tid || positions.count(candidate.userPosition))
                continue;
            auto prospectiveTotal = totalNsts + candidate.numberOfSpatialStreams;
            auto prospectiveReceiverLimit = std::min(receiverTotalLimit, candidate.receiverMaxNstsTotal);
            auto prospectiveSoundingLimit = std::min(soundingLimit, candidate.soundingNsts);
            if (prospectiveTotal > context.transmitDimensions ||
                    prospectiveTotal > context.maxNstsTotal ||
                    prospectiveTotal > prospectiveReceiverLimit ||
                    prospectiveTotal > prospectiveSoundingLimit)
                continue;
            positions.insert(candidate.userPosition);
            totalNsts += candidate.numberOfSpatialStreams;
            receiverTotalLimit = prospectiveReceiverLimit;
            soundingLimit = prospectiveSoundingLimit;
            selected.push_back(candidate);
            if (selected.size() == 4)
                break;
        }
        if (selected.size() >= 2)
            return selected;
    }
    return {};
}

} // namespace ieee80211
} // namespace inet
