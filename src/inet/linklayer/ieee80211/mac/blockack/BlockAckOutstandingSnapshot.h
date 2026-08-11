//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_BLOCKACKOUTSTANDINGSNAPSHOT_H
#define __INET_BLOCKACKOUTSTANDINGSNAPSHOT_H

#include <set>

#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementKey.h"
#include "inet/linklayer/ieee80211/mac/common/SequenceControlField.h"

namespace inet {
namespace ieee80211 {

struct BlockAckOutstandingSnapshot
{
    BlockAckAgreementKey key;
    std::set<SequenceControlField> occupiedSequencePositions;
    std::set<SequenceControlField> retryEligibleSequencePositions;
    uint64_t generation = 0;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_BLOCKACKOUTSTANDINGSNAPSHOT_H
