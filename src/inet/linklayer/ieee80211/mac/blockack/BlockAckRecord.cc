//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/blockack/BlockAckRecord.h"

#include "inet/common/stlutils.h"

namespace inet {
namespace ieee80211 {

bool BlockAckRecordSnapshot::getAckState(SequenceNumberCyclic sequenceNumber,
        FragmentNumber fragmentNumber) const
{
    if (containsKey(acknowledgmentState,
            SequenceControlField(sequenceNumber.get(), fragmentNumber)))
        return true;
    return sequenceNumber < winStartR;
}

BlockAckRecord::BlockAckRecord(MacAddress originatorAddress, Tid tid,
        SequenceNumberCyclic winStartR, int windowSize) :
    originatorAddress(originatorAddress),
    tid(tid),
    winStartR(winStartR),
    windowSize(windowSize)
{
    ASSERT(windowSize > 0);
}

void BlockAckRecord::blockAckPolicyFrameReceived(const Ptr<const Ieee80211DataHeader>& header)
{
    SequenceNumberCyclic sequenceNumber = header->getSequenceNumber();
    auto winEndR = winStartR + windowSize - 1;
    if (sequenceNumber > winEndR)
        advanceWindow(sequenceNumber - windowSize + 1);
    if (sequenceNumber < winStartR)
        return;
    FragmentNumber fragmentNumber = header->getFragmentNumber();
    auto inserted = acknowledgmentState.emplace(
            SequenceControlField(sequenceNumber.get(), fragmentNumber), true);
    if (inserted.second)
        ++generation;
}

bool BlockAckRecord::getAckState(SequenceNumberCyclic sequenceNumber, FragmentNumber fragmentNumber) const
{
    // The status of MPDUs that are considered “old” and prior to the sequence number
    // range for which the receiver maintains status shall be reported as successfully
    // received (i.e., the corresponding bit in the bitmap shall be set to 1).
    if (containsKey(acknowledgmentState,
            SequenceControlField(sequenceNumber.get(), fragmentNumber))) {
        return true;
    }
    // Sequence numbers preceding WinStartR are no longer in the maintained
    // scoreboard and are reported as received. A missing sequence number in
    // the current receive window is not acknowledged.
    return sequenceNumber < winStartR;
}

void BlockAckRecord::removeAckStates(SequenceNumberCyclic sequenceNumber)
{
    auto it = acknowledgmentState.begin();
    while (it != acknowledgmentState.end()) {
        if (SequenceNumberCyclic(it->first.getSequenceNumber()) < sequenceNumber)
            it = acknowledgmentState.erase(it);
        else
            it++;
    }
}

void BlockAckRecord::advanceWindow(
        SequenceNumberCyclic startingSequenceNumber)
{
    if (winStartR < startingSequenceNumber) {
        winStartR = startingSequenceNumber;
        removeAckStates(winStartR);
        ++generation;
    }
}

BlockAckRecordSnapshot BlockAckRecord::getSnapshot(uint64_t associationEpoch) const
{
    BlockAckRecordSnapshot snapshot;
    snapshot.key = std::make_pair(originatorAddress, tid);
    snapshot.winStartR = winStartR;
    snapshot.windowSize = windowSize;
    snapshot.acknowledgmentState = acknowledgmentState;
    snapshot.generation = generation;
    snapshot.associationEpoch = associationEpoch;
    return snapshot;
}

} /* namespace ieee80211 */
} /* namespace inet */
