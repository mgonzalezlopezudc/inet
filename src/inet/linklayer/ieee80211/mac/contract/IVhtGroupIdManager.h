//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IVHTGROUPIDMANAGER_H
#define __INET_IVHTGROUPIDMANAGER_H

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include <optional>

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

class INET_API IVhtGroupIdManager
{
  public:
    enum class State { ABSENT, PENDING, ACTIVE };
    struct Membership {
        MacAddress peer;
        uint8_t groupId = 0;
        uint8_t userPosition = 0;
        uint64_t associationGeneration = 0;
        Hz channelWidth = Hz(0);
    };
    class ILocalMembershipListener {
      public:
        virtual ~ILocalMembershipListener() = default;
        virtual void localVhtGroupMembershipChanged(const std::optional<Membership>& membership) = 0;
    };

    virtual ~IVhtGroupIdManager() {}

    virtual State getState(const MacAddress& peer, uint8_t groupId,
            uint64_t associationGeneration, Hz channelWidth) const = 0;
    virtual bool isActive(const MacAddress& peer, uint8_t groupId, uint8_t userPosition,
            uint64_t associationGeneration, Hz channelWidth) const = 0;
    virtual void beginPending(const MacAddress& peer, uint8_t groupId, uint8_t userPosition,
            uint64_t associationGeneration, Hz channelWidth) = 0;
    virtual bool acknowledge(const MacAddress& peer, uint8_t groupId,
            uint64_t associationGeneration, Hz channelWidth) = 0;
    virtual bool consume(const MacAddress& peer,
            const Ptr<const Ieee80211VhtGroupIdManagement>& action,
            uint64_t associationGeneration, Hz channelWidth) = 0;
    virtual void invalidatePeer(const MacAddress& peer) = 0;
    virtual void invalidateAll() = 0;
    virtual std::optional<Membership> getLocalMembership() const = 0;
    virtual void setLocalMembershipListener(ILocalMembershipListener *listener) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
