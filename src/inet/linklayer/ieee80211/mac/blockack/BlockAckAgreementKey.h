//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_BLOCKACKAGREEMENTKEY_H
#define __INET_BLOCKACKAGREEMENTKEY_H

#include <utility>

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"

namespace inet {
namespace ieee80211 {

using BlockAckAgreementKey = std::pair<MacAddress, Tid>;

} // namespace ieee80211
} // namespace inet

#endif // __INET_BLOCKACKAGREEMENTKEY_H
