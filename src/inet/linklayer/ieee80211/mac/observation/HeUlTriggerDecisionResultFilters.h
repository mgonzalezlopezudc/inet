//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULTRIGGERDECISIONRESULTFILTERS_H
#define __INET_HEULTRIGGERDECISIONRESULTFILTERS_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"

namespace inet {
namespace ieee80211 {

/**
 * Recorder-side projection base for the atomic HE UL Trigger decision event.
 */
class INET_API HeUlTriggerDecisionUserFilter : public cObjectResultFilter
{
  protected:
    virtual intval_t getValue(const HeUlTriggerDecisionEvent& event,
            const HeUlTriggerDecisionEvent::UserInfo& user,
            size_t userOrdinal) const = 0;

  public:
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t,
            cObject *object, cObject *details) override;
    using cObjectResultFilter::receiveSignal;
};

} // namespace ieee80211
} // namespace inet

#endif
