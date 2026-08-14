//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHEDLMUEXCHANGEEVENTS_H
#define __INET_IHEDLMUEXCHANGEEVENTS_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/HeDlMuExchangeTypes.h"

namespace inet {
namespace ieee80211 {

class INET_API IHeDlMuExchangeEvents
{
  public:
    virtual ~IHeDlMuExchangeEvents() = default;
    virtual void heDlMuPlanFinalized(HeDlMuExchangeId id,
            const std::vector<HeDlMuMember>& members) = 0;
    virtual void heDlMuPlanCommitted(HeDlMuExchangeId id,
            Packet *containerPacket, const std::vector<HeDlMuMember>& members) = 0;
    virtual void heDlMuMemberTransmitted(HeDlMuExchangeId id,
            const HeDlMuMember& member) = 0;
    virtual void heDlMuUserOutcome(HeDlMuExchangeId id,
            const MacAddress& peer, HeDlMuUserOutcome outcome) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
