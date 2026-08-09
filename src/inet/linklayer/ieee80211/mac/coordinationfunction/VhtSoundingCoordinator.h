//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTSOUNDINGCOORDINATOR_H
#define __INET_VHTSOUNDINGCOORDINATOR_H

#include <map>

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/contract/IVhtSoundingCoordinator.h"

namespace inet {
namespace ieee80211 {

class INET_API VhtSoundingCoordinator : public SimpleModule, public IVhtSoundingCoordinator
{
  protected:
    bool ndpAnnouncementAccepted = false;
    MacAddress soundingAccessPoint;
    uint8_t dialogToken = 0;
    uint16_t associationId = 0;
    bool feedbackTypeMu = false;
    uint8_t requestedNc = 1;
    Hz channelWidth = Hz(0);
    int soundingTransmitterRadioId = -1;
    simtime_t ndpaReceptionEnd = -1;
    std::map<MacAddress, simtime_t> nextAttemptTimes;
    simtime_t retryInterval = SimTime(0.1);

    virtual void initialize(int stage) override;

  public:
    virtual bool mayAttempt(const MacAddress& peer) const override;
    virtual void recordAttempt(const MacAddress& peer) override;
    virtual bool processNdpAnnouncement(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header, Ieee80211Mac *mac,
            bool enabled, Hz operatingWidth) override;
    virtual bool processHeaderlessNdp(Packet *packet, Ieee80211Mac *mac,
            physicallayer::Ieee80211ModeSet *modeSet, ITx *tx,
            ITx::ICallback *callback, bool enabled) override;
    virtual void reset() override;
    virtual void invalidatePeer(const MacAddress& peer) override;
};

} // namespace ieee80211
} // namespace inet

#endif
