//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/observation/HeUlTriggerDecisionResultFilters.h"

namespace inet {
namespace ieee80211 {

void HeUlTriggerDecisionUserFilter::receiveSignal(cResultFilter *prev,
        simtime_t_cref t, cObject *object, cObject *details)
{
    const auto event = check_and_cast<HeUlTriggerDecisionEvent *>(object);
    for (size_t i = 0; i < event->users.size(); i++)
        fire(this, t, getValue(*event, event->users[i], i), details);
}

#define Define_HeUlTriggerDecisionUserFilter(NAME, CLASS, VALUE) \
    class CLASS : public HeUlTriggerDecisionUserFilter { \
      protected: \
        virtual intval_t getValue(const HeUlTriggerDecisionEvent& event, \
                const HeUlTriggerDecisionEvent::UserInfo& user, \
                size_t userOrdinal) const override { return VALUE; } \
    }; \
    Register_ResultFilter(NAME, CLASS)

Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionTriggerId",
        HeUlTriggerDecisionTriggerIdFilter, event.triggerId);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionTriggerType",
        HeUlTriggerDecisionTriggerTypeFilter, event.triggerType);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionUserOrdinal",
        HeUlTriggerDecisionUserOrdinalFilter, userOrdinal);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionAssociationId",
        HeUlTriggerDecisionAssociationIdFilter, user.associationId);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionBacklogBytes",
        HeUlTriggerDecisionBacklogBytesFilter, user.backlogBytes);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionReportedBytes",
        HeUlTriggerDecisionReportedBytesFilter, user.reportedBytes);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionPlannedBytes",
        HeUlTriggerDecisionPlannedBytesFilter, user.plannedBytes);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionTid",
        HeUlTriggerDecisionTidFilter, user.tid);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionAccessCategory",
        HeUlTriggerDecisionAccessCategoryFilter, user.accessCategory);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionSelected",
        HeUlTriggerDecisionSelectedFilter, user.selected);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionRuIndex",
        HeUlTriggerDecisionRuIndexFilter, user.ruIndex);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionRuToneSize",
        HeUlTriggerDecisionRuToneSizeFilter, user.ruToneSize);
Define_HeUlTriggerDecisionUserFilter("heUlTriggerDecisionRuToneOffset",
        HeUlTriggerDecisionRuToneOffsetFilter, user.ruToneOffset);

#undef Define_HeUlTriggerDecisionUserFilter

} // namespace ieee80211
} // namespace inet
