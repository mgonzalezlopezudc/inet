//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/framesequence/PrimitiveFrameSequences.h"

#include <set>

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"

namespace inet {
namespace ieee80211 {

namespace {

auto expectedResponse(Ieee80211FrameType type)
{
    return [type](Packet *packet, FrameSequenceContext *context) {
        // IEEE Std 802.11-2024, 9.7.1: an A-MPDU starts with an MPDU
        // delimiter. The primitive exchanges below expect only CTS, ACK, or
        // BlockAck control responses, never an A-MPDU, so reject it without
        // reinterpreting its delimiter or PHY content as a MAC header.
        auto frontChunk = packet->peekAtFront();
        auto delimiter = dynamicPtrCast<const Ieee80211MpduSubframeHeader>(frontChunk);
        if (delimiter != nullptr)
            return false;
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
            bool validCts = context->isForUs(receivedHeader) && receivedHeader->getType() == ST_CTS;
            auto txop = context->getQoSContext() == nullptr ? nullptr : context->getQoSContext()->txopProcedure;
            if (validCts && txop != nullptr && txop->isInitialProtectionPending())
                // IEEE Std 802.11-2024, 10.3.2.9 and 10.23.2.8: only the
                // valid CTS in the initial RTS/CTS sequence completes the
                // immutable TXOP protection state.
                txop->completeInitialProtection();
            return validCts;
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

void HtAmpduBlockAckFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

IFrameSequenceStep *HtAmpduBlockAckFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            return new TransmitStep(
                    context->getInProgressFrames()->getFrameToTransmit(),
                    context->getIfs());
        case 1: {
            auto transmitStep = check_and_cast<ITransmitStep *>(
                    context->getStep(firstStep));
            auto packet = transmitStep->getFrameToTransmit();
            auto dataHeader = packet->peekAtFront<Ieee80211DataHeader>();
            auto blockAckReq = makeShared<Ieee80211CompressedBlockAckReq>();
            blockAckReq->setReceiverAddress(dataHeader->getReceiverAddress());
            blockAckReq->setTidInfo(dataHeader->getTid());
            blockAckReq->setStartingSequenceNumber(dataHeader->getSequenceNumber());
            // The BAR object is only a typed timeout descriptor; it is never
            // inserted into a packet or transmitted over the medium.
            return new ReceiveStep(
                    context->getQoSContext()->ackPolicy->getBlockAckTimeout(
                            packet, blockAckReq),
                    IReceiveStep::TimeoutHandling::ABORT_SEQUENCE,
                    expectedResponse(ST_BLOCKACK));
        }
        case 2:
            return nullptr;
        default:
            throw cRuntimeError("Unknown step");
    }
}

bool HtAmpduBlockAckFs::completeStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            step++;
            return true;
        case 1: {
            auto receiveStep = check_and_cast<IReceiveStep *>(
                    context->getStep(firstStep + step));
            step++;
            auto blockAck = dynamicPtrCast<const Ieee80211CompressedBlockAck>(
                    receiveStep->getReceivedFrame()->
                    peekAtFront<Ieee80211MacHeader>());
            auto dataHeader = check_and_cast<ITransmitStep *>(
                    context->getStep(firstStep))->getFrameToTransmit()->
                    peekAtFront<Ieee80211DataHeader>();
            if (blockAck == nullptr ||
                    blockAck->getReceiverAddress() != context->getAddress() ||
                    blockAck->getTransmitterAddress() !=
                            dataHeader->getReceiverAddress() ||
                    blockAck->getTidInfo() != dataHeader->getTid())
                return false;
            auto startingSequenceNumber =
                    blockAck->getStartingSequenceNumber();
            auto sequenceNumber = dataHeader->getSequenceNumber();
            // IEEE Std 802.11-2024 Table 9-13 and 10.25.6.5: Ack Policy 00
            // in a non-single-MPDU A-MPDU is an implicit BAR and elicits one
            // immediate Compressed BlockAck after SIFS.
            return sequenceNumber >= startingSequenceNumber &&
                    sequenceNumber < startingSequenceNumber + 64;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

void BlockAckReqBlockAckFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
    expectedMultiTidResponseFormat = ExpectedMultiTidResponseFormat::UNKNOWN;
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
            auto nicModule = getContainingNicModule(inProgress);
            auto macModule = check_and_cast<Ieee80211Mac *>(nicModule->getSubmodule("mac"));
            auto mib = macModule->getMib();

            auto negotiated = mib->findNegotiatedHeCapabilities(receiverAddr);
            Packet *blockAckPacket = nullptr;

            bool useHeMultiTidBlockAck = negotiated != nullptr &&
                    negotiated->localTxPeerRx.valid &&
                    negotiated->localTxPeerRx.multiTidAggregation;
            if (useHeMultiTidBlockAck ||
                    context->getUseLegacyHtMultiTidBlockAck()) {
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
                            if (it == recordsByTid.end() || seqNum < it->second) {
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
                expectedMultiTidResponseFormat = useHeMultiTidBlockAck ?
                        ExpectedMultiTidResponseFormat::HE_MULTI_STA :
                        ExpectedMultiTidResponseFormat::LEGACY_MULTI_TID;
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
        case 0: {
            if (expectedMultiTidResponseFormat ==
                    ExpectedMultiTidResponseFormat::UNKNOWN) {
                auto transmitStep = check_and_cast<ITransmitStep *>(
                        context->getStep(firstStep));
                if (dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(
                        transmitStep->getFrameToTransmit()->
                        peekAtFront<Ieee80211BlockAckReq>()))
                    expectedMultiTidResponseFormat =
                            context->getUseLegacyHtMultiTidBlockAck() ?
                            ExpectedMultiTidResponseFormat::LEGACY_MULTI_TID :
                            ExpectedMultiTidResponseFormat::HE_MULTI_STA;
            }
            step++;
            return true;
        }
        case 1: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            step++;
            auto receivedPacket = receiveStep->getReceivedFrame();
            const auto& receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
            if (!context->isForUs(receivedHeader) ||
                    receivedHeader->getType() != ST_BLOCKACK)
                return false;
            auto transmitStep = check_and_cast<ITransmitStep *>(
                    context->getStep(firstStep));
            auto blockAckReq = transmitStep->getFrameToTransmit()->
                    peekAtFront<Ieee80211BlockAckReq>();
            auto multiTidBlockAckReq =
                    dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(
                            blockAckReq);
            if (multiTidBlockAckReq == nullptr)
                return true;

            std::set<std::pair<Tid, uint16_t>> requestedRecords;
            for (unsigned int i = 0;
                    i < multiTidBlockAckReq->getRecordsArraySize(); ++i) {
                const auto& record = multiTidBlockAckReq->getRecords(i);
                if (!requestedRecords.emplace(record.tid,
                            record.startingSequenceNumber).second)
                    return false;
            }

            if (expectedMultiTidResponseFormat ==
                    ExpectedMultiTidResponseFormat::LEGACY_MULTI_TID) {
                auto multiTidBlockAck =
                        dynamicPtrCast<const Ieee80211MultiTidBlockAck>(
                                receivedHeader);
                if (multiTidBlockAck == nullptr ||
                        multiTidBlockAck->getBlockAckPolicy() ||
                        multiTidBlockAck->getReceiverAddress() !=
                                multiTidBlockAckReq->getTransmitterAddress() ||
                        multiTidBlockAck->getTransmitterAddress() !=
                                multiTidBlockAckReq->getReceiverAddress() ||
                        multiTidBlockAck->getRecordsArraySize() !=
                                multiTidBlockAckReq->getRecordsArraySize())
                    return false;

                std::set<std::pair<Tid, uint16_t>> responseRecords;
                for (unsigned int i = 0;
                        i < multiTidBlockAck->getRecordsArraySize(); ++i) {
                    const auto& record = multiTidBlockAck->getRecords(i);
                    auto key = std::make_pair(static_cast<Tid>(record.tid),
                            record.startingSequenceNumber);
                    if (requestedRecords.count(key) != 1 ||
                            !responseRecords.insert(key).second)
                        return false;
                }
                // This INET extension models the historical IEEE 802.11n
                // Multi-TID BlockAck, which is not a current 802.11 BA format.
                // Accept only an exact immediate per-TID response to the BAR.
                return responseRecords == requestedRecords;
            }

            auto multiStaBlockAck =
                    dynamicPtrCast<const Ieee80211MultiStaBlockAck>(
                            receivedHeader);
            if (multiStaBlockAck == nullptr ||
                    multiStaBlockAck->getBlockAckPolicy() ||
                    multiStaBlockAck->getReceiverAddress() !=
                            multiTidBlockAckReq->getTransmitterAddress() ||
                    multiStaBlockAck->getTransmitterAddress() !=
                            multiTidBlockAckReq->getReceiverAddress() ||
                    multiStaBlockAck->getRecordsArraySize() !=
                            multiTidBlockAckReq->getRecordsArraySize())
                return false;

            auto inProgress = context->getInProgressFrames();
            auto macModule = check_and_cast<Ieee80211Mac *>(
                    getContainingNicModule(inProgress)->getSubmodule("mac"));
            auto mib = macModule->getMib();
            uint16_t expectedAid = 0;
            if (mib->bssStationData.stationType == Ieee80211Mib::STATION) {
                auto associationId = mib->bssStationData.associationId;
                if (associationId <= 0 || associationId > 2007)
                    return false;
                expectedAid = associationId;
            }
            else if (mib->bssStationData.stationType !=
                    Ieee80211Mib::ACCESS_POINT)
                return false;

            std::set<std::pair<Tid, uint16_t>> responseRecords;
            for (unsigned int i = 0;
                    i < multiStaBlockAck->getRecordsArraySize(); ++i) {
                const auto& record = multiStaBlockAck->getRecords(i);
                auto key = std::make_pair(static_cast<Tid>(record.tid),
                        record.startingSequenceNumber);
                if (record.aid != expectedAid || !record.responseReceived ||
                        requestedRecords.count(key) != 1 ||
                        !responseRecords.insert(key).second)
                    return false;
            }
            // IEEE Std 802.11-2024, 10.25.5, 26.4.2 and 26.4.5:
            // only an exact per-AID/TID Multi-STA response completes an HE
            // Multi-TID BAR exchange; malformed responses follow timeout
            // recovery.
            return responseRecords == requestedRecords;
        }
        default:
            throw cRuntimeError("Unknown step");
    }
}

} // namespace ieee80211
} // namespace inet
