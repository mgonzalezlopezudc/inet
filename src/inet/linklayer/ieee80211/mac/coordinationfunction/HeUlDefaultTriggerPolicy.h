//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULDEFAULTTRIGGERPOLICY_H
#define __INET_HEULDEFAULTTRIGGERPOLICY_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"

namespace inet {
namespace ieee80211 {

/**
 * Default HE uplink trigger policy.
 *
 * It first requests BSRP reports when report information is stale, then
 * requests Basic Triggers for known backlog or retry traffic, subject to a
 * configurable minimum interval.
 */
class INET_API HeUlDefaultTriggerPolicy : public IIeee80211HeUlTriggerPolicy, public SimpleModule
{
  protected:
    simtime_t minimumTriggerInterval;

  protected:
    virtual void initialize(int stage) override;

  public:
    virtual TriggerType selectTrigger(const Context& context) const override;
};

} // namespace ieee80211
} // namespace inet

#endif
