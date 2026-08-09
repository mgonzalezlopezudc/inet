//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTDLMUPLAN_H
#define __INET_VHTDLMUPLAN_H

#include <algorithm>
#include <optional>
#include <set>
#include <string>

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211VhtDlMuScheduler.h"

namespace inet {
namespace ieee80211 {

enum class VhtDlMuPlanError {
    NONE,
    INVALID_COMMON_GATES,
    INVALID_USER_COUNT,
    INVALID_USER,
    DUPLICATE_USER,
    INVALID_POSITION,
    MISMATCHED_TID,
};

struct VhtDlMuPlanDiagnostic {
    VhtDlMuPlanError code = VhtDlMuPlanError::NONE;
    int userIndex = -1;
    MacAddress peer;
    std::string detail;

    explicit operator bool() const { return code != VhtDlMuPlanError::NONE; }
};

class INET_API VhtDlMuPlan
{
  private:
    IIeee80211VhtDlMuScheduler::Context context;
    std::vector<IIeee80211VhtDlMuScheduler::Candidate> users;

    VhtDlMuPlan(const IIeee80211VhtDlMuScheduler::Context& context,
            const std::vector<IIeee80211VhtDlMuScheduler::Candidate>& users) :
        context(context), users(users) {}

    static std::optional<VhtDlMuPlan> fail(VhtDlMuPlanDiagnostic& diagnostic,
            VhtDlMuPlanError code, int userIndex, const MacAddress& peer,
            const char *detail)
    {
        diagnostic = {code, userIndex, peer, detail};
        return std::nullopt;
    }

  public:
    static std::optional<VhtDlMuPlan> create(
            const IIeee80211VhtDlMuScheduler::Context& context,
            const std::vector<IIeee80211VhtDlMuScheduler::Candidate>& users,
            VhtDlMuPlanDiagnostic& diagnostic)
    {
        diagnostic = {};
        if (!context.enabled || !context.accessPoint || !context.packetLevelRadio ||
                (context.channelWidth != MHz(20) && context.channelWidth != MHz(40) &&
                 context.channelWidth != MHz(80) && context.channelWidth != MHz(160)) ||
                context.transmitDimensions < 2 || context.transmitDimensions > 8 ||
                context.maxNstsTotal < 2 || context.maxNstsTotal > 8 ||
                context.groupId < 1 || context.groupId > 62)
            return fail(diagnostic, VhtDlMuPlanError::INVALID_COMMON_GATES, -1,
                    MacAddress(), "VHT DL MU common gates require AP packet-level VHT width, 2..8 dimensions, and GID 1..62");
        if (users.size() < 2 || users.size() > 4)
            return fail(diagnostic, VhtDlMuPlanError::INVALID_USER_COUNT, -1,
                    MacAddress(), "VHT DL MU requires 2..4 active users");
        std::set<MacAddress> peers;
        std::set<int> positions;
        const auto commonTid = users.front().tid;
        int totalNsts = 0;
        for (size_t i = 0; i < users.size(); ++i) {
            const auto& user = users[i];
            if (!IIeee80211VhtDlMuScheduler::isEligible(context, user))
                return fail(diagnostic, VhtDlMuPlanError::INVALID_USER, i,
                        user.peer, "user failed an immutable VHT DL MU eligibility gate");
            if (!peers.insert(user.peer).second)
                return fail(diagnostic, VhtDlMuPlanError::DUPLICATE_USER, i,
                        user.peer, "duplicate VHT DL MU peer");
            if (!positions.insert(user.userPosition).second)
                return fail(diagnostic, VhtDlMuPlanError::INVALID_POSITION, i,
                        user.peer, "VHT DL MU user positions must be unique values in 0..3");
            if (user.tid != commonTid)
                return fail(diagnostic, VhtDlMuPlanError::MISMATCHED_TID, i,
                        user.peer, "VHT DL MU users must use the same TID");
            totalNsts += user.numberOfSpatialStreams;
        }
        // IEEE Std 802.11-2024, 21.1.1: total VHT MU NSTS is at most eight.
        if (totalNsts > context.transmitDimensions || totalNsts > context.maxNstsTotal ||
                std::any_of(users.begin(), users.end(), [totalNsts] (const auto& user) {
                    return totalNsts > user.receiverMaxNstsTotal || totalNsts > user.soundingNsts;
                }))
            return fail(diagnostic, VhtDlMuPlanError::INVALID_USER_COUNT, -1,
                    MacAddress(), "VHT DL MU total NSTS exceeds the transmitter limit");
        return VhtDlMuPlan(context, users);
    }

    const IIeee80211VhtDlMuScheduler::Context& getContext() const { return context; }
    const std::vector<IIeee80211VhtDlMuScheduler::Candidate>& getUsers() const { return users; }
};

} // namespace ieee80211
} // namespace inet

#endif
