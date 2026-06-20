//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_HTTXOPFS_H
#define __INET_HTTXOPFS_H

#include "inet/linklayer/ieee80211/mac/framesequence/GenericFrameSequences.h"

namespace inet {
namespace ieee80211 {

/**
 * HT/VHT TXOP frame-sequence selector.
 *
 * It chooses the appropriate aggregation and RTS/CTS alternatives for an HT
 * TXOP; VHT extensions reuse this path to keep TXOP and aggregation handling
 * consistent across the two PHY families.
 */
class INET_API HtTxOpFs : public AlternativesFs
{
  public:
    HtTxOpFs();

    virtual int selectHtTxOpSequence(AlternativesFs *frameSequence, FrameSequenceContext *context);
    virtual bool isRtsCtsNeeded(OptionalFs *frameSequence, FrameSequenceContext *context);
};

} // namespace ieee80211
} // namespace inet

#endif
