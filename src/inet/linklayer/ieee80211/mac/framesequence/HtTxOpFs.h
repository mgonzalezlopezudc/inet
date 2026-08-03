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
 * Deprecated compatibility selector for dormant HT TXOP alternatives.
 *
 * This exported type is retained for source and ABI compatibility, but the
 * production HCF path does not construct it. The current supported HT
 * protection subset uses the ordinary TxOpFs RTS/CTS path; L-SIG TXOP,
 * initiator-as-method, and dual-CTS alternatives remain intentionally
 * unsupported.
 */
class [[deprecated("Use TxOpFs; only its initial legacy RTS/CTS HT protection subset is supported")]] INET_API HtTxOpFs : public AlternativesFs
{
  public:
    HtTxOpFs();

    virtual int selectHtTxOpSequence(AlternativesFs *frameSequence, FrameSequenceContext *context);
    virtual bool isRtsCtsNeeded(OptionalFs *frameSequence, FrameSequenceContext *context);
};

} // namespace ieee80211
} // namespace inet

#endif
