//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTGROUPIDMANAGER_H
#define __INET_VHTGROUPIDMANAGER_H

#include <map>

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/contract/IVhtGroupIdManager.h"

namespace inet {
namespace ieee80211 {

class INET_API VhtGroupIdManager : public SimpleModule, public IVhtGroupIdManager
{
  public:
    struct Entry {
        State state = State::ABSENT;
        uint8_t groupId = 0;
        uint8_t userPosition = 0;
        uint64_t associationGeneration = 0;
        Hz channelWidth = Hz(0);
        simtime_t expiryTime = -1;
    };

  protected:
    std::map<MacAddress, Entry> entries;
    simtime_t pendingTimeout = SimTime(20, SIMTIME_MS);
    cMessage *expiryTimer = nullptr;
    std::optional<Membership> localMembership;
    ILocalMembershipListener *localMembershipListener = nullptr;

    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    virtual void rescheduleExpiryTimer();
    virtual void expirePending();

  public:
    virtual ~VhtGroupIdManager();

    static bool isMember(const Ieee80211VhtGroupIdManagement& action, uint8_t groupId);
    static uint8_t getUserPosition(const Ieee80211VhtGroupIdManagement& action, uint8_t groupId);
    static void setMembership(Ieee80211VhtGroupIdManagement& action,
            uint8_t groupId, uint8_t userPosition);

    virtual State getState(const MacAddress& peer, uint8_t groupId,
            uint64_t associationGeneration, Hz channelWidth) const override;
    virtual bool isActive(const MacAddress& peer, uint8_t groupId, uint8_t userPosition,
            uint64_t associationGeneration, Hz channelWidth) const override;
    virtual void beginPending(const MacAddress& peer, uint8_t groupId, uint8_t userPosition,
            uint64_t associationGeneration, Hz channelWidth) override;
    virtual bool acknowledge(const MacAddress& peer, uint8_t groupId,
            uint64_t associationGeneration, Hz channelWidth) override;
    virtual bool consume(const MacAddress& peer,
            const Ptr<const Ieee80211VhtGroupIdManagement>& action,
            uint64_t associationGeneration, Hz channelWidth) override;
    virtual void invalidatePeer(const MacAddress& peer) override;
    virtual void invalidateAll() override;
    virtual std::optional<Membership> getLocalMembership() const override { return localMembership; }
    virtual void setLocalMembershipListener(ILocalMembershipListener *listener) override;
};

} // namespace ieee80211
} // namespace inet

#endif
