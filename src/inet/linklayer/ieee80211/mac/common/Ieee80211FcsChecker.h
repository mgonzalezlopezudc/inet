//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211FCSCHECKER_H
#define __INET_IEEE80211FCSCHECKER_H

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/contract/IRx.h"

namespace inet {
namespace ieee80211 {

class INET_API Ieee80211FcsChecker
{
  public:
    static bool isFcsOk(Packet *packet,
            AggregateReceptionContext aggregateContext);
};

} // namespace ieee80211
} // namespace inet

#endif
