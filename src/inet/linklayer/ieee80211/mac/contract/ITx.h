//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_ITX_H
#define __INET_ITX_H

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"

namespace inet {
namespace ieee80211 {

/**
 * Abstract interface for unconditionally transmitting a frame immediately
 * or after waiting for a specified inter-frame space (usually SIFS).
 */
class INET_API ITx
{
  public:
    class INET_API ICallback {
      public:
        virtual ~ICallback() {}

        virtual void transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) = 0;
    };

  public:
    virtual ~ITx() {}

    virtual void transmitFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, ICallback *callback) = 0;
    virtual void transmitFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, simtime_t ifs, ICallback *callback) = 0;
    /**
     * Cancels a transmission that is still waiting for its inter-frame
     * space. The owner must be the callback supplied to transmitFrame().
     * Returns true only when no copy has been handed to the lower layer yet.
     * A transmission that is already in progress is left untouched.
     */
    virtual bool cancelPendingTransmission(ICallback *owner) = 0;

    /** Transmit using a TXOP-local mode without modifying the queued packet's
     * ModeReq tag.  Implementations must stage the mode on their private
     * transmission duplicate. */
    virtual void transmitFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header,
            const physicallayer::IIeee80211Mode *mode, ICallback *callback) = 0;
    virtual void transmitFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, simtime_t ifs,
            const physicallayer::IIeee80211Mode *mode, ICallback *callback) = 0;
    virtual void radioTransmissionFinished() = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
