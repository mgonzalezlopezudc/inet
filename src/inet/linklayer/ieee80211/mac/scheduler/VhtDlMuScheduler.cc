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

std::vector<unsigned int>
VhtDlMuScheduler::schedule(const SchedulingContext& context) const
{
    std::vector<CandidateSnapshot> eligible;
    for (const auto& candidate : context.candidates)
        if (IIeee80211VhtDlMuScheduler::isEligible(context, candidate))
            eligible.push_back(candidate);
    std::stable_sort(eligible.begin(), eligible.end(), [] (const auto& left, const auto& right) {
        if (left.candidate.enqueueTime != right.candidate.enqueueTime)
            return left.candidate.enqueueTime < right.candidate.enqueueTime;
        if (left.candidate.associationId != right.candidate.associationId)
            return left.candidate.associationId < right.candidate.associationId;
        if (left.candidate.peer != right.candidate.peer)
            return left.candidate.peer < right.candidate.peer;
        return left.candidateIndex < right.candidateIndex;
    });
    if (eligible.size() < 2)
        return {};
    for (const auto& anchor : eligible) {
        std::vector<unsigned int> selected;
        std::set<int> positions;
        int totalNsts = 0;
        int receiverTotalLimit = 8;
        int soundingLimit = 8;
        for (const auto& candidate : eligible) {
            if (candidate.candidate.tid != anchor.candidate.tid ||
                    positions.count(candidate.candidate.userPosition))
                continue;
            auto prospectiveTotal = totalNsts + candidate.candidate.numberOfSpatialStreams;
            auto prospectiveReceiverLimit = std::min(receiverTotalLimit, candidate.candidate.receiverMaxNstsTotal);
            auto prospectiveSoundingLimit = std::min(soundingLimit, candidate.candidate.soundingNsts);
            if (prospectiveTotal > context.common.transmitDimensions ||
                    prospectiveTotal > context.common.maxNstsTotal ||
                    prospectiveTotal > prospectiveReceiverLimit ||
                    prospectiveTotal > prospectiveSoundingLimit)
                continue;
            positions.insert(candidate.candidate.userPosition);
            totalNsts += candidate.candidate.numberOfSpatialStreams;
            receiverTotalLimit = prospectiveReceiverLimit;
            soundingLimit = prospectiveSoundingLimit;
            selected.push_back(candidate.candidateIndex);
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
