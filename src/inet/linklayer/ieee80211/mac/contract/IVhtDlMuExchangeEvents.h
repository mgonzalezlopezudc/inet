//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IVHTDLMUEXCHANGEEVENTS_H
#define __INET_IVHTDLMUEXCHANGEEVENTS_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/VhtDlMuExchangeTypes.h"

namespace inet {

class Packet;

namespace ieee80211 {

class INET_API IVhtDlMuExchangeEvents
{
  public:
    virtual ~IVhtDlMuExchangeEvents() = default;
    virtual void vhtDlMuPlanCommitted(VhtDlMuExchangeId id,
            Packet *containerPacket,
            const std::vector<std::vector<Packet *>>& userPackets) = 0;
    virtual void vhtDlMuFrameFailed(VhtDlMuExchangeId id, Packet *packet) = 0;
    virtual void vhtDlMuUserResult(VhtDlMuExchangeId id,
            unsigned int userIndex, VhtDlMuUserResult result) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
