// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_IVHTSOUNDINGCOORDINATOR_H
#define __INET_IVHTSOUNDINGCOORDINATOR_H

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"

namespace inet {
namespace physicallayer { class Ieee80211ModeSet; }
namespace ieee80211 {

class Ieee80211Mac;

class INET_API IVhtSoundingCoordinator
{
  public:
    virtual ~IVhtSoundingCoordinator() = default;
    virtual bool mayAttempt(const MacAddress& peer) const = 0;
    virtual void recordAttempt(const MacAddress& peer) = 0;
    virtual bool processNdpAnnouncement(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header, Ieee80211Mac *mac,
            bool enabled, Hz operatingWidth) = 0;
    virtual bool processHeaderlessNdp(Packet *packet, Ieee80211Mac *mac,
            physicallayer::Ieee80211ModeSet *modeSet, ITx *tx,
            ITx::ICallback *callback, bool enabled) = 0;
    virtual void reset() = 0;
    virtual void invalidatePeer(const MacAddress& peer) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
