//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/framesequence/PrimitiveFrameSequences.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"

namespace inet {
namespace ieee80211 {

namespace {

auto expectedResponse(Ieee80211FrameType type)
{
    return [type](Packet *packet, FrameSequenceContext *context) {
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        return context->isForUs(header) && header->getType() == type;
    };
}

bool canUseCompressedBlockAckReq(FrameSequenceContext *context, MacAddress receiverAddress, Tid tid)
{
    auto outstandingFrames = context->getInProgressFrames()->getOutstandingFrames();
    bool hasMatchingOutstandingFrame = false;
    for (auto frame : outstandingFrames) {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(frame->peekAtFront<Ieee80211MacHeader>());
        if (dataHeader == nullptr || dataHeader->getReceiverAddress() != receiverAddress || dataHeader->getTid() != tid)
            continue;
        hasMatchingOutstandingFrame = true;
        if (dataHeader->getFragmentNumber() != 0 || dataHeader->getMoreFragments())
            return false;
    }
    return hasMatchingOutstandingFrame;
}

} // namespace

// TODO remove isForUs checks it's already done in framesequencehandler

void SelfCtsFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *SelfCtsFs::prepareStep(FrameSequenceContext *context)
{
    // TODO Implement
    return nullptr;
}

bool SelfCtsFs::completeStep(FrameSequenceContext *context)
{
    // TODO Implement
    return false;
}

void RtsFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *RtsFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto dataOrMgmtPacket = context->getInProgressFrames()->getFrameToTransmit();
            auto rtsFrame = context->getRtsProcedure()->buildRtsFrame(dataOrMgmtPacket->peekAtFront<Ieee80211DataOrMgmtHeader>());
            auto rtsPacket = new Packet("RTS");
            rtsPacket->insertAtBack(rtsFrame);
            rtsPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
            return new RtsTransmitStep(dataOrMgmtPacket, rtsPacket, context->getIfs());
        }
        case 1:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool RtsFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        default:
            throw cRuntimeError("Unknown step");
    }
}

void CtsFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *CtsFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto txStep = check_and_cast<RtsTransmitStep *>(context->getLastStep());
            auto rtsPacket = txStep->getFrameToTransmit();
            return new ReceiveStep(context->getCtsTimeout(rtsPacket, rtsPacket->peekAtFront<Ieee80211RtsFrame>()),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE, expectedResponse(ST_CTS));
        }
        case 1:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool CtsFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            return context->isForUs(receivedHeader) && receivedHeader->getType() == ST_CTS;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

void DataFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *DataFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto packet = context->getInProgressFrames()->getFrameToTransmit();
            return new TransmitStep(packet, context->getIfs());
        }
        case 1:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool DataFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        default:
            throw cRuntimeError("Unknown step");
    }
}

void ManagementAckFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *ManagementAckFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto packet = context->getInProgressFrames()->getFrameToTransmit();
            return new TransmitStep(packet, context->getIfs());
        }
        case 1: {
            auto txStep = check_and_cast<TransmitStep *>(context->getLastStep());
            auto packet = txStep->getFrameToTransmit();
            auto mgmtHeader = packet->peekAtFront<Ieee80211MgmtHeader>();
            return new ReceiveStep(context->getAckTimeout(packet, mgmtHeader),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE, expectedResponse(ST_ACK));
        }
        case 2:
            return nullptr;

        default:
            throw cRuntimeError("Unknown step");
    }
}

bool ManagementAckFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        case 1: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            return context->isForUs(receivedHeader) && receivedHeader->getType() == ST_ACK;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

void ManagementFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *ManagementFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto packet = context->getInProgressFrames()->getFrameToTransmit();
            return new TransmitStep(packet, context->getIfs());
        }
        case 1:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool ManagementFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        default:
            throw cRuntimeError("Unknown step");
    }
}

void AckFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *AckFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto txStep = check_and_cast<TransmitStep *>(context->getLastStep());
            auto packet = txStep->getFrameToTransmit();
            auto dataOrMgmtHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
            return new ReceiveStep(context->getAckTimeout(packet, dataOrMgmtHeader),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE, expectedResponse(ST_ACK));
        }
        case 1:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool AckFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            return context->isForUs(receivedHeader) && receivedHeader->getType() == ST_ACK;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

void RtsCtsFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *RtsCtsFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto packet = context->getInProgressFrames()->getFrameToTransmit();
            auto dataOrMgmtHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
            auto rtsFrame = context->getRtsProcedure()->buildRtsFrame(dataOrMgmtHeader);
            auto rtsPacket = new Packet("RTS");
            rtsPacket->insertAtBack(rtsFrame);
            rtsPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
            return new RtsTransmitStep(packet, rtsPacket, context->getIfs());
        }
        case 1: {
            auto txStep = check_and_cast<RtsTransmitStep *>(context->getLastStep());
            auto packet = txStep->getFrameToTransmit();
            auto rtsFrame = packet->peekAtFront<Ieee80211RtsFrame>();
            return new ReceiveStep(context->getCtsTimeout(packet, rtsFrame),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE, expectedResponse(ST_CTS));
        }
        case 2:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool RtsCtsFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        case 1: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            return context->isForUs(receivedHeader) && receivedHeader->getType() == ST_CTS;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

void FragFrameAckFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *FragFrameAckFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto frame = context->getInProgressFrames()->getFrameToTransmit();
            return new TransmitStep(frame, context->getIfs());
        }
        case 1: {
            auto txStep = check_and_cast<TransmitStep *>(context->getLastStep());
            auto packet = txStep->getFrameToTransmit();
            auto dataOrMgmtHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
            return new ReceiveStep(context->getAckTimeout(packet, dataOrMgmtHeader),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE, expectedResponse(ST_ACK));
        }
        case 2:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool FragFrameAckFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        case 1: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            return context->isForUs(receivedHeader) && receivedHeader->getType() == ST_ACK;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

void LastFrameAckFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *LastFrameAckFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto frame = context->getInProgressFrames()->getFrameToTransmit();
            return new TransmitStep(frame, context->getIfs());
        }
        case 1: {
            auto txStep = check_and_cast<TransmitStep *>(context->getLastStep());
            auto packet = txStep->getFrameToTransmit();
            auto dataOrMgmtHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
            return new ReceiveStep(context->getAckTimeout(packet, dataOrMgmtHeader),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE, expectedResponse(ST_ACK));
        }
        case 2:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool LastFrameAckFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        case 1: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            return context->isForUs(receivedHeader) && receivedHeader->getType() == ST_ACK;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

void BlockAckReqBlockAckFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *BlockAckReqBlockAckFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0: {
            auto blockAckReqParams = context->getQoSContext()->ackPolicy->computeBlockAckReqParameters(context->getInProgressFrames(), context->getQoSContext()->txopProcedure);
            auto receiverAddr = std::get<0>(blockAckReqParams);
            auto startingSequenceNumber = std::get<1>(blockAckReqParams);
            auto tid = std::get<2>(blockAckReqParams);

            auto inProgress = context->getInProgressFrames();
            auto hcfModule = inProgress != nullptr ? inProgress->getParentModule() : nullptr;
            auto macModule = hcfModule != nullptr ? dynamic_cast<Ieee80211Mac *>(hcfModule->getParentModule()) : nullptr;
            auto mib = macModule != nullptr ? macModule->getMib() : nullptr;

            auto negotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(receiverAddr) : nullptr;
            Packet *blockAckPacket = nullptr;

            if (negotiated != nullptr && negotiated->intersection.multiTidAggregationTx) {
                // Collect starting sequence numbers by TID from in-progress frames for receiverAddr
                std::map<Tid, SequenceNumberCyclic> recordsByTid;
                for (int i = 0; i < inProgress->getLength(); i++) {
                    auto f = inProgress->getFrames(i);
                    auto macHdr = f->peekAtFront<Ieee80211MacHeader>();
                    if (macHdr != nullptr && macHdr->getReceiverAddress() == receiverAddr) {
                        if (auto dataHdr = dynamicPtrCast<const Ieee80211DataHeader>(macHdr)) {
                            auto t = dataHdr->getTid();
                            auto seqNum = dataHdr->getSequenceNumber();
                            auto it = recordsByTid.find(t);
                            if (it == recordsByTid.end() || seqNum.get() < it->second.get()) {
                                recordsByTid[t] = seqNum;
                            }
                        }
                    }
                }
                if (recordsByTid.empty()) {
                    recordsByTid[tid] = startingSequenceNumber;
                }
                auto multiTidReq = makeShared<Ieee80211MultiTidBlockAckReq>();
                multiTidReq->setReceiverAddress(receiverAddr);
                multiTidReq->setTransmitterAddress(macModule->getAddress());
                multiTidReq->setRecordsArraySize(recordsByTid.size());
                unsigned int idx = 0;
                for (const auto& entry : recordsByTid) {
                    Ieee80211MultiTidBlockAckReqRecord rec;
                    rec.tid = entry.first;
                    rec.startingSequenceNumber = entry.second.get();
                    multiTidReq->setRecords(idx++, rec);
                }
                multiTidReq->setChunkLength(B(18 + 4 * recordsByTid.size()));
                blockAckPacket = new Packet("MultiTidBlockAckReq", multiTidReq);
            }
            else {
                auto blockAckReq = canUseCompressedBlockAckReq(context, receiverAddr, tid) ?
                        context->getQoSContext()->blockAckProcedure->buildCompressedBlockAckReqFrame(receiverAddr, tid, startingSequenceNumber) :
                        context->getQoSContext()->blockAckProcedure->buildBasicBlockAckReqFrame(receiverAddr, tid, startingSequenceNumber);
                blockAckPacket = new Packet(dynamicPtrCast<const Ieee80211CompressedBlockAckReq>(blockAckReq) ? "CompressedBlockAckReq" : "BasicBlockAckReq", blockAckReq);
            }
            blockAckPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
            return new TransmitStep(blockAckPacket, context->getIfs(), true);
        }
        case 1: {
            auto txStep = check_and_cast<ITransmitStep *>(context->getLastStep());
            auto packet = txStep->getFrameToTransmit();
            auto blockAckReq = packet->peekAtFront<Ieee80211BlockAckReq>();
            return new ReceiveStep(context->getQoSContext()->ackPolicy->getBlockAckTimeout(packet, blockAckReq),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE, expectedResponse(ST_BLOCKACK));
        }
        case 2:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool BlockAckReqBlockAckFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        case 1: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            return context->isForUs(receivedHeader) && receivedHeader->getType() == ST_BLOCKACK;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

} // namespace ieee80211
} // namespace inet
