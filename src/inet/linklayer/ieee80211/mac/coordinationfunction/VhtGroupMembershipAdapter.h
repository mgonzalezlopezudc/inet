//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTGROUPMEMBERSHIPADAPTER_H
#define __INET_VHTGROUPMEMBERSHIPADAPTER_H

#include "inet/linklayer/ieee80211/mac/contract/IVhtGroupIdManager.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211VhtPacketRadio.h"

namespace inet {
namespace ieee80211 {

/** Projects Group-ID-owner membership into the packet radio. */
class INET_API VhtGroupMembershipAdapter :
        public IVhtGroupIdManager::ILocalMembershipListener
{
  private:
    physicallayer::IIeee80211VhtPacketRadio *radio = nullptr;

  public:
    void setRadio(physicallayer::IIeee80211VhtPacketRadio *value) { radio = value; }

    virtual void localVhtGroupMembershipChanged(
            const std::optional<IVhtGroupIdManager::Membership>& membership) override
    {
        if (radio == nullptr)
            return;
        if (!membership.has_value())
            radio->setVhtMuRxSelection({});
        else
            radio->setVhtMuRxSelection({true, membership->groupId,
                    membership->userPosition, membership->channelWidth});
    }
};

} // namespace ieee80211
} // namespace inet

#endif
