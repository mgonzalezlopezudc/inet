//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTDLMUSCHEDULER_H
#define __INET_VHTDLMUSCHEDULER_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211VhtDlMuScheduler.h"

namespace inet {
namespace ieee80211 {

class INET_API VhtDlMuScheduler : public SimpleModule, public IIeee80211VhtDlMuScheduler
{
  public:
    virtual std::vector<unsigned int> schedule(const SchedulingContext& context) const override;

    /** Source-compatible test/helper wrapper; delegates through immutable snapshots. */
    std::vector<Candidate> schedule(const Context& context) const
    {
        std::vector<Candidate> selected;
        for (auto index : schedule(SchedulingContext(context))) {
            if (index >= context.candidates.size())
                throw cRuntimeError("VHT DL MU scheduler returned an invalid candidate index");
            selected.push_back(context.candidates[index]);
        }
        return selected;
    }
};

} // namespace ieee80211
} // namespace inet

#endif
