//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MODEREQUESTGUARD_H
#define __INET_IEEE80211MODEREQUESTGUARD_H

#include "inet/common/packet/Packet.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

/** Temporarily exposes a TXOP-local mode to legacy duration helpers. */
class Ieee80211ModeRequestGuard
{
  protected:
    Packet *packet;
    bool hadOriginalModeRequest;
    const physicallayer::IIeee80211Mode *originalMode;

  public:
    explicit Ieee80211ModeRequestGuard(Packet *packet) :
        packet(packet),
        hadOriginalModeRequest(packet->findTag<physicallayer::Ieee80211ModeReq>() != nullptr),
        originalMode(hadOriginalModeRequest ? packet->findTag<physicallayer::Ieee80211ModeReq>()->getMode() : nullptr)
    {
    }

    void setMode(const physicallayer::IIeee80211Mode *mode)
    {
        packet->addTagIfAbsent<physicallayer::Ieee80211ModeReq>()->setMode(mode);
    }

    ~Ieee80211ModeRequestGuard()
    {
        if (hadOriginalModeRequest)
            packet->getTagForUpdate<physicallayer::Ieee80211ModeReq>()->setMode(originalMode);
        else
            packet->removeTagIfPresent<physicallayer::Ieee80211ModeReq>();
    }
};

} // namespace ieee80211
} // namespace inet

#endif
