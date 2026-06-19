//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/framesequence/HtTxOpFs.h"

#include "inet/linklayer/ieee80211/mac/framesequence/PrimitiveFrameSequences.h"

namespace inet {
namespace ieee80211 {

HtTxOpFs::HtTxOpFs() :
    // G.4 HT sequences
    // ht-txop-sequence =
    //   L-sig-protected-sequence |
    //   ht-nav-protected-sequence |
    //   dual-cts-protected-sequence |
    //   1 {initiator-sequence};
    AlternativesFs({
        new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isRtsCtsNeeded)), new DataFs()}), // 0: L-sig-protected
        new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isRtsCtsNeeded)), new DataFs()}), // 1: HT-NAV-protected
        new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isRtsCtsNeeded)), new DataFs()}), // 2: Dual-CTS-protected
        new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isRtsCtsNeeded)), new DataFs(), new AckFs()})  // 3: Simple initiator sequence
    },
    ALTERNATIVESFS_SELECTOR(selectHtTxOpSequence))
{
}

int HtTxOpFs::selectHtTxOpSequence(AlternativesFs *frameSequence, FrameSequenceContext *context)
{
    if (!context)
        return 3;
    auto inProgressFrames = context->getInProgressFrames();
    if (!inProgressFrames || !inProgressFrames->hasInProgressFrames())
        return 3;
    auto frameToTransmit = inProgressFrames->getFrameToTransmit();
    if (!frameToTransmit)
        return 3;
    const auto& macHeader = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
    if (context->getRtsPolicy() && context->getRtsPolicy()->isRtsNeeded(frameToTransmit, macHeader)) {
        return 1; // HT-NAV protected sequence (using RTS/CTS)
    }
    return 3; // Simple sequence
}

bool HtTxOpFs::isRtsCtsNeeded(OptionalFs *frameSequence, FrameSequenceContext *context)
{
    if (!context)
        return false;
    auto inProgressFrames = context->getInProgressFrames();
    if (!inProgressFrames || !inProgressFrames->hasInProgressFrames())
        return false;
    auto frameToTransmit = inProgressFrames->getFrameToTransmit();
    if (!frameToTransmit)
        return false;
    const auto& macHeader = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
    return context->getRtsPolicy()->isRtsNeeded(frameToTransmit, macHeader);
}

} // namespace ieee80211
} // namespace inet

