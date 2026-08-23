//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211PHYHEADERCRC_H
#define __INET_IEEE80211PHYHEADERCRC_H

#include <cstdint>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/**
 * Computes the complemented CRC-8 used by HT-SIG and VHT-SIG-A.
 * The input is the 34 protected bits in transmission order; the returned
 * value is serialized from bit 7 through bit 0.
 */
INET_API uint8_t computeIeee80211PhyHeaderCrc(const std::vector<bool>& protectedBits);

} // namespace physicallayer
} // namespace inet

#endif
