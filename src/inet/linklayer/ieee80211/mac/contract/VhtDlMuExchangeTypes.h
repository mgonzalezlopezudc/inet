//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTDLMUEXCHANGETYPES_H
#define __INET_VHTDLMUEXCHANGETYPES_H

#include <cstdint>

namespace inet {
namespace ieee80211 {

using VhtDlMuExchangeId = uint64_t;
constexpr VhtDlMuExchangeId NO_VHT_DL_MU_EXCHANGE = 0;

enum class VhtDlMuUserResult {
    TRANSMITTED,
    BLOCK_ACK_RECEIVED,
    BLOCK_ACK_TIMED_OUT,
};

} // namespace ieee80211
} // namespace inet

#endif
