//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_RECIPIENTBLOCKACKAGREEMENT_H
#define __INET_RECIPIENTBLOCKACKAGREEMENT_H

#include "inet/linklayer/ieee80211/mac/blockack/BlockAckRecord.h"

namespace inet {
namespace ieee80211 {

struct RecipientBlockAckAgreementSnapshot
{
    BlockAckRecordSnapshot record;
    SequenceNumberCyclic startingSequenceNumber;
    int bufferSize = -1;
    simtime_t blockAckTimeoutValue = 0;
    bool isAddbaResponseSent = false;
    simtime_t expirationTime = -1;
    uint64_t generation = 0;
    uint64_t associationEpoch = 0;
};

class INET_API IRecipientBlockAckAgreementOwner
{
  public:
    virtual ~IRecipientBlockAckAgreementOwner() {}
    virtual RecipientBlockAckAgreementSnapshot getSnapshot() const = 0;
    virtual void blockAckPolicyFrameReceived(const Ptr<const Ieee80211DataHeader>& header) = 0;
    virtual void advanceWindow(SequenceNumberCyclic startingSequenceNumber) = 0;
};

class INET_API RecipientBlockAckAgreement : public cObject, public IRecipientBlockAckAgreementOwner
{
  protected:
    BlockAckRecord *blockAckRecord = nullptr;

    SequenceNumberCyclic startingSequenceNumber;
    int bufferSize = -1;
    simtime_t blockAckTimeoutValue = 0;
    bool isAddbaResponseSent = false;
    simtime_t expirationTime = -1;
    uint64_t generation = 0;
    uint64_t associationEpoch = 0;

  public:
    RecipientBlockAckAgreement(MacAddress originatorAddress, Tid tid, SequenceNumberCyclic startingSequenceNumber, int bufferSize, simtime_t blockAckTimeoutValue);
    virtual ~RecipientBlockAckAgreement() { delete blockAckRecord; }

    virtual void blockAckPolicyFrameReceived(const Ptr<const Ieee80211DataHeader>& header) override;

    virtual const BlockAckRecord *getBlockAckRecord() const { return blockAckRecord; }
    virtual simtime_t getBlockAckTimeoutValue() const { return blockAckTimeoutValue; }
    virtual int getBufferSize() const { return bufferSize; }
    virtual SequenceNumberCyclic getStartingSequenceNumber() const { return startingSequenceNumber; }

    virtual void addbaResposneSent() { if (!isAddbaResponseSent) { isAddbaResponseSent = true; ++generation; } }
    virtual void calculateExpirationTime() { expirationTime = blockAckTimeoutValue == 0 ? SIMTIME_MAX : simTime() + blockAckTimeoutValue; ++generation; }
    virtual simtime_t getExpirationTime() const { return expirationTime; }
    virtual void advanceWindow(SequenceNumberCyclic startingSequenceNumber) override {
        auto previousGeneration = blockAckRecord->getGeneration();
        blockAckRecord->advanceWindow(startingSequenceNumber);
        if (blockAckRecord->getGeneration() != previousGeneration)
            ++generation;
    }

    virtual RecipientBlockAckAgreementSnapshot getSnapshot() const override {
        auto snapshot = RecipientBlockAckAgreementSnapshot{blockAckRecord->getSnapshot(associationEpoch),
                startingSequenceNumber, bufferSize, blockAckTimeoutValue,
                isAddbaResponseSent, expirationTime, generation, associationEpoch};
        return snapshot;
    }
    virtual void setAssociationEpoch(uint64_t value) { associationEpoch = value; }
    virtual uint64_t getGeneration() const { return generation; }
    virtual uint64_t getAssociationEpoch() const { return associationEpoch; }
    virtual bool isSnapshotCurrent(const RecipientBlockAckAgreementSnapshot& snapshot) const {
        auto current = getSnapshot();
        return snapshot.associationEpoch == current.associationEpoch &&
                snapshot.generation == current.generation &&
                snapshot.record.generation == current.record.generation;
    }
    virtual bool getAckState(SequenceNumberCyclic sequenceNumber, FragmentNumber fragmentNumber) const {
        return blockAckRecord->getAckState(sequenceNumber, fragmentNumber);
    }
    virtual SequenceNumberCyclic getWinStartR() const { return blockAckRecord->getWinStartR(); }
    friend std::ostream& operator<<(std::ostream& os, const RecipientBlockAckAgreement& agreement);
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
