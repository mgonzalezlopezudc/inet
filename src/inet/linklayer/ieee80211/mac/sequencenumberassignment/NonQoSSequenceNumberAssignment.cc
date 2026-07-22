//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/sequencenumberassignment/NonQoSSequenceNumberAssignment.h"

namespace inet {
namespace ieee80211 {

void NonQoSSequenceNumberAssignment::assignSequenceNumber(const Ptr<Ieee80211DataOrMgmtHeader>& header)
{
    ASSERT(header->getType() != ST_DATA_WITH_QOS);
    lastSeqNum = lastSeqNum + 1;
    const MacAddress& address = header->getReceiverAddress();
    auto it = lastSentSeqNums.find(address);
    if (it == lastSentSeqNums.end())
        lastSentSeqNums[address] = lastSeqNum;
    else {
        if (it->second == lastSeqNum)
            lastSeqNum = lastSeqNum + 1; // make it different from the last sequence number sent to that RA (spec: "add 2")
        it->second = lastSeqNum;
    }
    header->setSequenceNumber(lastSeqNum);
}

std::unique_ptr<ISequenceNumberAssignment> NonQoSSequenceNumberAssignment::clone() const
{
    return std::make_unique<NonQoSSequenceNumberAssignment>(*this);
}

void NonQoSSequenceNumberAssignment::copyStateFrom(const ISequenceNumberAssignment& other)
{
    auto source = dynamic_cast<const NonQoSSequenceNumberAssignment *>(&other);
    if (source == nullptr)
        throw cRuntimeError("Cannot copy incompatible non-QoS sequence-number assignment state");
    lastSeqNum = source->lastSeqNum;
    lastSentSeqNums = source->lastSentSeqNums;
}

} /* namespace ieee80211 */
} /* namespace inet */
