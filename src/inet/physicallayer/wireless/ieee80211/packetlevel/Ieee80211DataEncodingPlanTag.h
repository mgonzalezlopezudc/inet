//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211DATAENCODINGPLANTAG_H
#define __INET_IEEE80211DATAENCODINGPLANTAG_H

#include <memory>

#include "inet/common/TagBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DataEncodingPlan.h"

namespace inet {
namespace physicallayer {

/** Internal immutable handoff within one local PHY pipeline; never crosses the radio medium. */
class INET_API Ieee80211DataEncodingPlanTag : public TagBase
{
  protected:
    std::shared_ptr<const Ieee80211DataEncodingPlan> plan;

  public:
    Ieee80211DataEncodingPlanTag() = default;
    Ieee80211DataEncodingPlanTag(const Ieee80211DataEncodingPlanTag&) = default;

    virtual Ieee80211DataEncodingPlanTag *dup() const override { return new Ieee80211DataEncodingPlanTag(*this); }

    void setPlan(const Ieee80211DataEncodingPlan& value) { plan = std::make_shared<const Ieee80211DataEncodingPlan>(value); }
    bool hasPlan() const { return plan != nullptr; }
    const Ieee80211DataEncodingPlan& getPlan() const;

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
