//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

#include "inet/linklayer/ieee80211/mac/framesequence/PrimitiveFrameSequences.h"
#include "inet/linklayer/ieee80211/mac/framesequence/TxOpFs.h"

namespace inet {
namespace ieee80211 {

HcfFs::HcfFs() :
    // G.3 EDCA and HCCA sequences
    // hcf-sequence =
    //   ( [ CTS ] 1{( Data + group [+ QoS ] ) | Management + broadcast ) +pifs} |
    //   ( [ CTS ] 1{txop-sequence} ) |
    //   (* HC only, polled TXOP delivery *)
    //   ( [ RTS CTS ] non-cf-ack-piggybacked-qos-poll-sequence )
    //   (* HC only, polled TXOP delivery *)
    //   cf-ack-piggybacked-qos-poll-sequence |
    //   (* HC only, self TXOP delivery or termination *)
    //   Data + self + null + CF-Poll + QoS;
    AlternativesFs({new SequentialFs({new OptionalFs(new SelfCtsFs(), OPTIONALFS_PREDICATE(isSelfCtsNeeded)),
                                      new RepeatingFs(new AlternativesFs({new DataFs(), new ManagementFs()}, ALTERNATIVESFS_SELECTOR(selectDataOrManagementSequence)),
                                                      REPEATINGFS_PREDICATE(hasMoreTxOpsAndMulticast))}),
                    new SequentialFs({new OptionalFs(new SelfCtsFs(), OPTIONALFS_PREDICATE(isSelfCtsNeeded)),
                                      new RepeatingFs(new TxOpFs(), REPEATINGFS_PREDICATE(hasMoreTxOps))})},
                   ALTERNATIVESFS_SELECTOR(selectHcfSequence))
{
}

int HcfFs::selectHcfSequence(AlternativesFs *frameSequence, FrameSequenceContext *context)
{
    auto frameToTransmit = context->getInProgressFrames()->getFrameToTransmit();
    if (frameToTransmit == nullptr)
        throw cRuntimeError("HCF frame sequence started without an eligible in-progress frame");
    return frameToTransmit->peekAtFront<Ieee80211MacHeader>()->getReceiverAddress().isMulticast() ? 0 : 1;
}

int HcfFs::selectDataOrManagementSequence(AlternativesFs *frameSequence, FrameSequenceContext *context)
{
    auto frameToTransmit = context->getInProgressFrames()->getFrameToTransmit();
    const auto& header = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
    if (dynamicPtrCast<const Ieee80211DataHeader>(header))
        return 0;
    else if (dynamicPtrCast<const Ieee80211MgmtHeader>(header))
        return 1;
    else
        throw cRuntimeError("frameToTransmit must be either a Data or a Management frame");
}

bool HcfFs::isSelfCtsNeeded(OptionalFs *frameSequence, FrameSequenceContext *context)
{
    return false;
}

bool HcfFs::hasMoreTxOps(RepeatingFs *frameSequence, FrameSequenceContext *context)
{
    bool hasFrameToTransmit = context->getInProgressFrames()->hasInProgressFrames();
    if (hasFrameToTransmit) {
        auto nextFrameToTransmit = context->getInProgressFrames()->getFrameToTransmit();
        const auto& nextHeader = nextFrameToTransmit->peekAtFront<Ieee80211MacHeader>();
        if (frameSequence->getCount() == 0)
            // IEEE Std 802.11-2024, 10.23.2.9: a zero TXOP limit still
            // permits the single frame exchange that obtained the medium.
            return true;
        if (nextHeader->getReceiverAddress().isMulticast())
            return false;

        auto qosContext = context->getQoSContext();
        auto txop = qosContext->txopProcedure;
        auto rateSelection = qosContext->rateSelection;
        if (rateSelection == nullptr)
            throw cRuntimeError("HCF TXOP admission requires QoS rate selection");

        auto modeReq = nextFrameToTransmit->findTag<physicallayer::Ieee80211ModeReq>();
        auto dataMode = modeReq == nullptr ?
                rateSelection->computeMode(nextFrameToTransmit, nextHeader, txop) :
                modeReq->getMode();
        if (modeReq == nullptr)
            nextFrameToTransmit->addTag<physicallayer::Ieee80211ModeReq>()->setMode(dataMode);

        simtime_t requiredDuration = context->getIfs() +
                dataMode->getDuration(nextFrameToTransmit->getDataLength());
        bool ackNeeded = false;
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(nextHeader)) {
            OriginatorBlockAckAgreement *agreement = qosContext->blockAckAgreementHandler == nullptr ?
                    nullptr : qosContext->blockAckAgreementHandler->getAgreement(
                            dataHeader->getReceiverAddress(), dataHeader->getTid());
            ackNeeded = qosContext->ackPolicy->computeAckPolicy(
                    nextFrameToTransmit, dataHeader, agreement) == NORMAL_ACK;
        }
        else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(nextHeader))
            ackNeeded = qosContext->ackPolicy->isAckNeeded(mgmtHeader);
        if (ackNeeded) {
            auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(nextHeader);
            requiredDuration += context->getIfs() +
                    rateSelection->computeResponseAckFrameMode(nextFrameToTransmit,
                            dataOrMgmtHeader)->getDuration(LENGTH_ACK);
        }
        // IEEE Std 802.11-2024, 10.23.2.8 and 10.23.2.10: start another
        // exchange only if its complete PPDU/response sequence fits within
        // the remaining TXOP. All durations come from the selected modes and
        // the active ACK policy; no independent timing table is maintained.
        return requiredDuration <= txop->getRemaining();
    }
    return false;
}

bool HcfFs::hasMoreTxOpsAndMulticast(RepeatingFs *frameSequence, FrameSequenceContext *context)
{
    return hasMoreTxOps(frameSequence, context) && context->getInProgressFrames()->getFrameToTransmit()->peekAtFront<Ieee80211MacHeader>()->getReceiverAddress().isMulticast();
}

} // namespace ieee80211
} // namespace inet
