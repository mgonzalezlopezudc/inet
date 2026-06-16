//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_BLOCKACKAGREEMENTUTILS_H
#define __INET_BLOCKACKAGREEMENTUTILS_H

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"

namespace inet {
namespace ieee80211 {

inline bool hasActiveOriginatorBlockAckAgreement(IOriginatorBlockAckAgreementHandler *handler,
        const MacAddress& receiverAddress, Tid tid)
{
    auto agreement = handler == nullptr ? nullptr : handler->getAgreement(receiverAddress, tid);
    return agreement != nullptr && agreement->getIsAddbaResponseReceived();
}

} // namespace ieee80211
} // namespace inet

#endif // __INET_BLOCKACKAGREEMENTUTILS_H
