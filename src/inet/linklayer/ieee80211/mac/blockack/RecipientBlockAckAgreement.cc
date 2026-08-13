//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"

#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreementHandler.h"

namespace inet {
namespace ieee80211 {

RecipientBlockAckAgreement::RecipientBlockAckAgreement(MacAddress originatorAddress, Tid tid, SequenceNumberCyclic startingSequenceNumber, int bufferSize, simtime_t lastUsedTime) :
    startingSequenceNumber(startingSequenceNumber),
    bufferSize(bufferSize),
    blockAckTimeoutValue(lastUsedTime)
{
    calculateExpirationTime();
    blockAckRecord = new BlockAckRecord(originatorAddress, tid,
            startingSequenceNumber, bufferSize);
}

void RecipientBlockAckAgreement::blockAckPolicyFrameReceived(const Ptr<const Ieee80211DataHeader>& header)
{
    // IEEE Std 802.11-2024, 10.25.6.3 and 10.25.6.5: the recipient
    // scoreboard consumes both explicit Block Ack policy MPDUs and Normal
    // Ack policy MPDUs that form an implicit-BlockAck A-MPDU.
    ASSERT(header->getAckPolicy() == BLOCK_ACK ||
            header->getAckPolicy() == NORMAL_ACK);
    auto previousGeneration = blockAckRecord->getGeneration();
    blockAckRecord->blockAckPolicyFrameReceived(header);
    if (blockAckRecord->getGeneration() != previousGeneration)
        ++generation;
}

std::ostream& operator<<(std::ostream& os, const RecipientBlockAckAgreement& agreement)
{
    os << "originator address = " << agreement.blockAckRecord->getOriginatorAddress() << ", "
       << "tid = " << agreement.blockAckRecord->getTid() << ", "
       << "starting sequence number = " << agreement.startingSequenceNumber << ", "
       << "buffer size = " << agreement.bufferSize << ", "
       << "block ack timeout value = " << agreement.blockAckTimeoutValue;
    return os;
}

} /* namespace ieee80211 */
} /* namespace inet */
