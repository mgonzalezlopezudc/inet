//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211PEERRATECONTROL_H
#define __INET_IIEEE80211PEERRATECONTROL_H

#include <vector>

#include "inet/linklayer/common/MacAddress.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"

namespace inet {
namespace ieee80211 {

/**
 * Optional peer-aware extension of IRateControl for unicast data selection.
 *
 * The caller supplies modes that have already passed operation and peer
 * capability checks. The controller is responsible only for choosing among
 * those candidates and recording feedback for the exact selected mode.
 */
class INET_API IIeee80211PeerRateControl
{
  public:
    virtual ~IIeee80211PeerRateControl() {}

    virtual const physicallayer::IIeee80211Mode *selectRate(const MacAddress& peer,
            const std::vector<const physicallayer::IIeee80211Mode *>& candidates) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
