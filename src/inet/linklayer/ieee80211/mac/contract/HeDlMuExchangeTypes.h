//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLMUEXCHANGETYPES_H
#define __INET_HEDLMUEXCHANGETYPES_H

#include <cstdint>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"

namespace inet {

class Packet;

namespace ieee80211 {

using HeDlMuExchangeId = uint64_t;
constexpr HeDlMuExchangeId NO_HE_DL_MU_EXCHANGE = 0;

struct INET_API HeDlMuMember
{
    HcfPacketIdentity packetIdentity;
    Packet *packet = nullptr;
    MacAddress peer;
    Tid tid = 0;
    AccessCategory accessCategory = AC_BE;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    int ruToneSize = 0;
};

enum class HeDlMuUserOutcome {
    BLOCK_ACK_RECEIVED,
    BLOCK_ACK_TIMED_OUT,
};

} // namespace ieee80211
} // namespace inet

#endif
