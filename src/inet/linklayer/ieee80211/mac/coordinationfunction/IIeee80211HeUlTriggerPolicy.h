//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEE80211HEULTRIGGERPOLICY_H
#define __INET_IIEE80211HEULTRIGGERPOLICY_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace ieee80211 {

/** Strategy interface for deciding whether an AP should initiate HE uplink OFDMA. */
class INET_API IIeee80211HeUlTriggerPolicy
{
  public:
    enum TriggerType {
        NO_TRIGGER = -1,
        BASIC_TRIGGER = 0,
        BSRP_TRIGGER = 4,
        NFRP_TRIGGER = 7,
    };

    /** Aggregate association, report, backlog, retry, and timing state for a decision. */
    struct Context {
        int associatedStations = 0;
        int freshReports = 0;
        int backloggedStations = 0;
        int retryStations = 0;
        simtime_t elapsedSinceLastTrigger = SIMTIME_MAX;
    };

    virtual ~IIeee80211HeUlTriggerPolicy() {}
    virtual TriggerType selectTrigger(const Context& context) const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
