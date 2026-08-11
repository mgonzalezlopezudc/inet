//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_ORIGINATORBLOCKACKAGREEMENT_H
#define __INET_ORIGINATORBLOCKACKAGREEMENT_H

#include "inet/linklayer/ieee80211/mac/common/SequenceControlField.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementKey.h"

namespace inet {
namespace ieee80211 {

class OriginatorBlockAckAgreementHandler;

struct OriginatorBlockAckAgreementSnapshot
{
    BlockAckAgreementKey key;
    SequenceNumberCyclic startingSequenceNumber;
    int bufferSize = -1;
    bool isAMsduSupported = false;
    bool isDelayedBlockAckPolicySupported = false;
    bool isAddbaResponseReceived = false;
    bool isAddbaRequestSent = false;
    bool isAddbaRequestInProgress = false;
    bool expirationHandlingInProgress = false;
    simtime_t blockAckTimeoutValue = -1;
    simtime_t expirationTime = -1;
    int numSentBaPolicyFrames = 0;
    uint64_t generation = 0;
    uint64_t associationEpoch = 0;
};

class INET_API OriginatorBlockAckAgreement : public cObject
{
  protected:
    MacAddress receiverAddr = MacAddress::UNSPECIFIED_ADDRESS;
    Tid tid = -1;
    int numSentBaPolicyFrames = 0;
    SequenceNumberCyclic startingSequenceNumber;
    int bufferSize = -1;
    bool isAMsduSupported = false;
    bool isDelayedBlockAckPolicySupported = false;
    bool isAddbaResponseReceived = false;
    bool isAddbaRequestSent = false;
    bool isAddbaRequestInProgress = false;
    bool expirationHandlingInProgress = false;
    simtime_t blockAckTimeoutValue = -1;
    simtime_t expirationTime = -1;
    uint64_t generation = 0;
    uint64_t associationEpoch = 0;

  public:
    OriginatorBlockAckAgreement(MacAddress receiverAddr, Tid tid, SequenceNumberCyclic startingSequenceNumber, int bufferSize, bool isAMsduSupported, bool isDelayedBlockAckPolicySupported) :
        receiverAddr(receiverAddr),
        tid(tid),
        startingSequenceNumber(startingSequenceNumber),
        bufferSize(bufferSize),
        isAMsduSupported(isAMsduSupported),
        isDelayedBlockAckPolicySupported(isDelayedBlockAckPolicySupported)
    {
    }

    virtual ~OriginatorBlockAckAgreement() {}

    virtual int getBufferSize() const { return bufferSize; }
    virtual SequenceNumberCyclic getStartingSequenceNumber() const { return startingSequenceNumber; }
    virtual void setStartingSequenceNumber(SequenceNumberCyclic sequenceNumber) { if (startingSequenceNumber != sequenceNumber) { startingSequenceNumber = sequenceNumber; ++generation; } }
    virtual bool getIsAddbaResponseReceived() const { return isAddbaResponseReceived; }
    virtual bool getIsAddbaRequestSent() const { return isAddbaRequestSent; }
    virtual bool getIsAddbaRequestInProgress() const { return isAddbaRequestInProgress; }
    virtual bool getExpirationHandlingInProgress() const { return expirationHandlingInProgress; }
    virtual bool getIsAMsduSupported() const { return isAMsduSupported; }
    virtual bool getIsDelayedBlockAckPolicySupported() const { return isDelayedBlockAckPolicySupported; }
    virtual MacAddress getReceiverAddr() const { return receiverAddr; }
    virtual Tid getTid() const { return tid; }
    virtual const simtime_t getBlockAckTimeoutValue() const { return blockAckTimeoutValue; }
    virtual int getNumSentBaPolicyFrames() const { return numSentBaPolicyFrames; }

    virtual void setBufferSize(int bufferSize) { if (this->bufferSize != bufferSize) { this->bufferSize = bufferSize; ++generation; } }
    virtual void setIsAddbaResponseReceived(bool value) { if (isAddbaResponseReceived != value) { isAddbaResponseReceived = value; ++generation; } }
    virtual void setIsAddbaRequestSent(bool value) { if (isAddbaRequestSent != value) { isAddbaRequestSent = value; ++generation; } }
    virtual void setIsAddbaRequestInProgress(bool value) { if (isAddbaRequestInProgress != value) { isAddbaRequestInProgress = value; ++generation; } }
    virtual void setExpirationHandlingInProgress(bool value) { if (expirationHandlingInProgress != value) { expirationHandlingInProgress = value; ++generation; } }
    virtual void setIsAMsduSupported(bool value) { if (isAMsduSupported != value) { isAMsduSupported = value; ++generation; } }
    virtual void setIsDelayedBlockAckPolicySupported(bool value) { if (isDelayedBlockAckPolicySupported != value) { isDelayedBlockAckPolicySupported = value; ++generation; } }
    virtual void setBlockAckTimeoutValue(const simtime_t value) { if (blockAckTimeoutValue != value) { blockAckTimeoutValue = value; ++generation; } }

    virtual void baPolicyFrameSent() { ++numSentBaPolicyFrames; ++generation; }
    virtual void calculateExpirationTime() { expirationTime = blockAckTimeoutValue == 0 ? SIMTIME_MAX : simTime() + blockAckTimeoutValue; ++generation; }
    virtual simtime_t getExpirationTime() const { return expirationTime; }

    void setAssociationEpoch(uint64_t value) { associationEpoch = value; }
    uint64_t getGeneration() const { return generation; }
    uint64_t getAssociationEpoch() const { return associationEpoch; }
    bool isSnapshotCurrent(const OriginatorBlockAckAgreementSnapshot& snapshot) const {
        return snapshot.associationEpoch == associationEpoch &&
                snapshot.generation == generation;
    }
    OriginatorBlockAckAgreementSnapshot getSnapshot() const {
        return {std::make_pair(receiverAddr, tid), startingSequenceNumber, bufferSize,
                isAMsduSupported, isDelayedBlockAckPolicySupported,
                isAddbaResponseReceived, isAddbaRequestSent,
                isAddbaRequestInProgress, expirationHandlingInProgress,
                blockAckTimeoutValue, expirationTime, numSentBaPolicyFrames,
                generation, associationEpoch};
    }
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
