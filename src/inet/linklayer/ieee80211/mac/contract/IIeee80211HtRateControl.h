//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211HTRATECONTROL_H
#define __INET_IIEEE80211HTRATECONTROL_H

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211HtControl.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtCsiCache.h"

namespace inet {

namespace physicallayer {
class IIeee80211Mode;
}

namespace ieee80211 {

/** Optional policy contract for packet-level HT MCS request and feedback. */
class INET_API IIeee80211HtRateControl
{
  public:
    virtual ~IIeee80211HtRateControl() {}

    virtual void processReceivedHtMcsRequest(const MacAddress& peer, uint8_t msi,
            const physicallayer::IIeee80211Mode *receivedMode) = 0;
    virtual void processReceivedHtMcsFeedback(const MacAddress& peer, uint8_t mfsi,
            uint8_t mfb) = 0;
    virtual bool getPendingHtMcsControl(const MacAddress& peer,
            bool mcsRequestAllowed, bool mcsFeedbackAllowed,
            Ieee80211HtMcsControl& control) = 0;
    virtual HtCsiCache& getHtCsiCache() = 0;
    virtual void invalidateHtPeer(const MacAddress& peer) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
