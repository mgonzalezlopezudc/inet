//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/framesequence/TxOpFs.h"

#include "inet/linklayer/ieee80211/mac/framesequence/PrimitiveFrameSequences.h"

namespace inet {
namespace ieee80211 {

INET_API bool isHtImplicitBlockAckEligible(const FrameSequenceContext *context,
        Packet *frameToTransmit,
        const Ptr<const Ieee80211DataHeader>& dataHeader)
{
    auto txop = context->getQoSContext()->txopProcedure;
    if (!context->getUseHtImplicitBlockAck() ||
            !txop->allowsMpduAggregation() ||
            dataHeader == nullptr ||
            dataHeader->getType() != ST_DATA_WITH_QOS ||
            dataHeader->getReceiverAddress().isMulticast() ||
            dataHeader->getFragmentNumber() != 0 ||
            dataHeader->getMoreFragments() ||
            context->getRtsPolicy()->isRtsNeeded(frameToTransmit, dataHeader))
        return false;
    auto agreement = context->getQoSContext()->blockAckAgreementHandler == nullptr ?
            nullptr :
            context->getQoSContext()->blockAckAgreementHandler->getAgreement(
                    dataHeader->getReceiverAddress(), dataHeader->getTid());
    if (context->getQoSContext()->ackPolicy->computeAckPolicy(
                frameToTransmit, dataHeader, agreement) != BLOCK_ACK)
        return false;
    const auto& frames = context->getHtImplicitBlockAckFrames();
    if (frames.size() < 2 || frames.front() != frameToTransmit)
        return false;
    for (auto frame : frames) {
        auto header = frame->peekAtFront<Ieee80211DataHeader>();
        if (frame->getTotalLength() > B(4095) ||
                header->getFragmentNumber() != 0 || header->getMoreFragments())
            return false;
    }
    return true;
}

/*
 * TODO add [ RTS CTS ] (txop-part-requiring-ack txop-part-providing-ack )|
 */
TxOpFs::TxOpFs() :
    // Excerpt from G.3 EDCA and HCCA sequences
    // txop-sequence =
    //   ( ( ( RTS CTS ) | CTS + self ) Data + individual + QoS +( block-ack | no-ack ) ) |
    //   [ RTS CTS ] (txop-part-requiring-ack txop-part-providing-ack )|
    //   [ RTS CTS ] (Management | ( Data + QAP )) + individual ACK |
    //   [ RTS CTS ] (BlockAckReq BlockAck ) |
    //   ht-txop-sequence;
    AlternativesFs({new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isRtsCtsNeeded)),
                                      new DataFs()}),
                    new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isRtsCtsNeeded)),
                                      new DataFs(),
                                      new AckFs()}), // TODO should be in txop-part-requiring-ack
                    new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isBlockAckReqRtsCtsNeeded)),
                                      new BlockAckReqBlockAckFs()}),
                    new SequentialFs({new OptionalFs(new RtsCtsFs(), OPTIONALFS_PREDICATE(isRtsCtsNeeded)),
                                      new AlternativesFs({new ManagementAckFs(),
                                                          /* TODO DATA + QAP*/},
                                                         ALTERNATIVESFS_SELECTOR(selectMgmtOrDataQap))}),
                    new HtAmpduBlockAckFs()},
                   ALTERNATIVESFS_SELECTOR(selectTxOpSequence))
{
}

int TxOpFs::selectMgmtOrDataQap(AlternativesFs *frameSequence, FrameSequenceContext *context)
{
    return 0;
}

int TxOpFs::selectTxOpSequence(AlternativesFs *frameSequence, FrameSequenceContext *context)
{
    auto frameToTransmit = context->getInProgressFrames()->getFrameToTransmit();
    const auto& macHeader = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
    if (context->getQoSContext()->ackPolicy->isBlockAckReqNeeded(context->getInProgressFrames(), context->getQoSContext()->txopProcedure))
        return 2;
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(macHeader))
        return 3;
    else {
        auto dataHeaderToTransmit = dynamicPtrCast<const Ieee80211DataHeader>(macHeader);
        if (isHtImplicitBlockAckEligible(context, frameToTransmit,
                    dataHeaderToTransmit))
            return 4;
        OriginatorBlockAckAgreement *agreement = nullptr;
        if (context->getQoSContext()->blockAckAgreementHandler)
            agreement = context->getQoSContext()->blockAckAgreementHandler->getAgreement(dataHeaderToTransmit->getReceiverAddress(), dataHeaderToTransmit->getTid());
        auto ackPolicy = context->getQoSContext()->ackPolicy->computeAckPolicy(frameToTransmit, dataHeaderToTransmit, agreement);
        if (ackPolicy == AckPolicy::BLOCK_ACK)
            return 0;
        else if (ackPolicy == AckPolicy::NORMAL_ACK)
            return 1;
        else
            throw cRuntimeError("Unknown AckPolicy");
    }
}

bool TxOpFs::isRtsCtsNeeded(OptionalFs *frameSequence, FrameSequenceContext *context)
{
    auto protectedFrame = context->getInProgressFrames()->getFrameToTransmit();
    return context->getQoSContext()->txopProcedure->isInitialProtectionPending() ||
            context->getRtsPolicy()->isRtsNeeded(protectedFrame, protectedFrame->peekAtFront<Ieee80211MacHeader>());
}

bool TxOpFs::isBlockAckReqRtsCtsNeeded(OptionalFs *frameSequence, FrameSequenceContext *context)
{
    return false; // FIXME QosRtsPolicy should handle this case
}

} // namespace ieee80211
} // namespace inet
