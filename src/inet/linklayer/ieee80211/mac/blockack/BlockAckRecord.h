//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_BLOCKACKRECORD_H
#define __INET_BLOCKACKRECORD_H

#include "inet/linklayer/ieee80211/mac/common/SequenceControlField.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementKey.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

class RecipientBlockAckAgreement;

struct BlockAckRecordSnapshot
{
    BlockAckAgreementKey key;
    SequenceNumberCyclic winStartR;
    int windowSize = -1;
    std::map<SequenceControlField, bool> acknowledgmentState;
    uint64_t generation = 0;
    uint64_t associationEpoch = 0;

    bool getAckState(SequenceNumberCyclic sequenceNumber, FragmentNumber fragmentNumber) const;
};

//
// The recipient shall maintain a Block Ack record consisting of originator address, TID, and a record of
// reordering buffer size indexed by the received MPDU sequence control value. This record holds the
// acknowledgment state of the data frames received from the originator.
//
class INET_API BlockAckRecord
{
  protected:
    MacAddress originatorAddress = MacAddress::UNSPECIFIED_ADDRESS;
    Tid tid = -1;
    SequenceNumberCyclic winStartR;
    int windowSize = -1;
    std::map<SequenceControlField, bool> acknowledgmentState;
    uint64_t generation = 0;

  public:
    BlockAckRecord(MacAddress originatorAddress, Tid tid,
            SequenceNumberCyclic winStartR, int windowSize);
    virtual ~BlockAckRecord() {}

  private:
    friend class RecipientBlockAckAgreement;

  private:
    void blockAckPolicyFrameReceived(const Ptr<const Ieee80211DataHeader>& header);

  private:
    void removeAckStates(SequenceNumberCyclic sequenceNumber);
    void advanceWindow(SequenceNumberCyclic startingSequenceNumber);

  public:
    bool getAckState(SequenceNumberCyclic sequenceNumber, FragmentNumber fragmentNumber) const;
    BlockAckRecordSnapshot getSnapshot(uint64_t associationEpoch = 0) const;

    MacAddress getOriginatorAddress() const { return originatorAddress; }
    Tid getTid() const { return tid; }
    SequenceNumberCyclic getWinStartR() const { return winStartR; }
    int getWindowSize() const { return windowSize; }
    uint64_t getGeneration() const { return generation; }
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
