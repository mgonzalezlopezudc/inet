//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTDLMUPLAN_H
#define __INET_VHTDLMUPLAN_H

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
                context.channelWidth != MHz(20) || context.transmitDimensions != 2 ||
                context.groupId != 1)
            return fail(diagnostic, VhtDlMuPlanError::INVALID_COMMON_GATES, -1,
                    MacAddress(), "VHT DL MU common gates require AP packet-level 20 MHz, two dimensions, and GID 1");
        if (users.size() != 2)
            return fail(diagnostic, VhtDlMuPlanError::INVALID_USER_COUNT, -1,
                    MacAddress(), "constrained VHT DL MU requires exactly two users");
        std::set<MacAddress> peers;
        std::set<int> positions;
        const auto commonTid = users.front().tid;
        for (size_t i = 0; i < users.size(); ++i) {
            const auto& user = users[i];
            if (!IIeee80211VhtDlMuScheduler::isEligible(context, user))
                return fail(diagnostic, VhtDlMuPlanError::INVALID_USER, i,
                        user.peer, "user failed an immutable VHT DL MU eligibility gate");
            if (!peers.insert(user.peer).second)
                return fail(diagnostic, VhtDlMuPlanError::DUPLICATE_USER, i,
                        user.peer, "duplicate VHT DL MU peer");
            if (!positions.insert(user.userPosition).second || user.userPosition != i)
                return fail(diagnostic, VhtDlMuPlanError::INVALID_POSITION, i,
                        user.peer, "VHT DL MU positions must be canonical 0,1");
            if (user.tid != commonTid)
                return fail(diagnostic, VhtDlMuPlanError::MISMATCHED_TID, i,
                        user.peer, "constrained VHT DL MU users must use the same TID");
        }
        return VhtDlMuPlan(context, users);
    }

    const IIeee80211VhtDlMuScheduler::Context& getContext() const { return context; }
    const std::vector<IIeee80211VhtDlMuScheduler::Candidate>& getUsers() const { return users; }
};

} // namespace ieee80211
} // namespace inet

#endif
