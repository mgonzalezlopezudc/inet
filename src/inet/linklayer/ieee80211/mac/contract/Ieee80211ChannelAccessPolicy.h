//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211CHANNELACCESSPOLICY_H
#define __INET_IEEE80211CHANNELACCESSPOLICY_H

// The concrete channel-access policy belongs to the coordination-function
// implementation.  Keep this forwarding include for source compatibility
// with existing integrations that used the old contract path.
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Ieee80211ChannelAccessPolicy.h"

namespace inet {
namespace ieee80211 {

} // namespace ieee80211
} // namespace inet

#endif
