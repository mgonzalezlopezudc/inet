//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IACKHANDLER_H
#define __INET_IACKHANDLER_H

#include <set>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

class INET_API IAckHandler
{
  public:
    virtual ~IAckHandler() {}

    virtual bool isEligibleToTransmit(const Ptr<const Ieee80211DataOrMgmtHeader>& header) = 0;
    virtual bool isOutstandingFrame(const Ptr<const Ieee80211DataOrMgmtHeader>& header) = 0;
    virtual void frameGotInProgress(const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader) = 0;
    virtual std::set<int> getOccupiedBlockAckSequenceNumbers(
            const MacAddress& receiverAddress, Tid tid) const { return {}; }
};

} // namespace ieee80211
} // namespace inet

#endif
