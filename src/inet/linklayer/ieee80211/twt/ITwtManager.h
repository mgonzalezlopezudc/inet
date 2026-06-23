//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ITWTMANAGER_H
#define __INET_ITWTMANAGER_H

#include <vector>

#include "inet/linklayer/ieee80211/twt/TwtAgreement.h"

namespace inet {
namespace ieee80211 {

class INET_API ITwtManager
{
  public:
    virtual ~ITwtManager() {}
    virtual bool isEnabled() const = 0;
    virtual void installAgreement(const TwtAgreement& agreement) = 0;
    virtual void removeAgreement(const MacAddress& peer, uint8_t flowId, bool broadcast, uint8_t broadcastId) = 0;
    virtual bool updateNextWakeTime(const MacAddress& peer, uint8_t flowId, simtime_t nextWakeTime) = 0;
    virtual bool isStationAwake() const = 0;
    virtual bool isPeerEligible(const MacAddress& peer) const = 0;
    virtual void notifyPeerAwake(const MacAddress& peer) = 0;
    virtual void installBroadcastSchedule(const TwtBroadcastSchedule& schedule) = 0;
    virtual void removeBroadcastSchedule(uint8_t broadcastId) = 0;
    virtual bool findBroadcastSchedule(uint8_t broadcastId, TwtBroadcastSchedule& schedule) const = 0;
    virtual std::vector<TwtBroadcastSchedule> getBroadcastSchedules() const = 0;
    virtual bool addBroadcastMember(uint8_t broadcastId, const MacAddress& peer) = 0;
    virtual void removeBroadcastMember(uint8_t broadcastId, const MacAddress& peer) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
