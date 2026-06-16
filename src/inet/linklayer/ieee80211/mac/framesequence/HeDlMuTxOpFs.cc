//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/rateselection/RateSelection.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

HeDlMuTxOpFs::HeDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                             const std::vector<MacAddress>& candidates,
                             Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback)
    : dlScheduler(dlScheduler),
      candidates(candidates),
      modeSet(modeSet),
      pendingQueue(pendingQueue),
      ackHandler(ackHandler),
      callback(callback)
{
}

void HeDlMuTxOpFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
}

Packet *HeDlMuTxOpFs::buildMuContainerPacket(FrameSequenceContext *context)
{
    activeAllocations.clear();
    // Determine channel bandwidth and center frequency from the modeSet's first HE mode.
    Hz channelBandwidth = Hz(20e6);       // default: 20 MHz
    Hz channelCenterFrequency = Hz(5.18e9); // default: 5 GHz band, ch36

    if (modeSet != nullptr && modeSet->getNumModes() > 0) {
        auto firstMode = modeSet->getMode(0);
        if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(firstMode)) {
            channelBandwidth = heMode->getDataMode()->getBandwidth();
            channelCenterFrequency = (heMode->getCenterFrequencyMode() == Ieee80211HeMode::BAND_2_4GHZ)
                    ? Hz(2.412e9) : Hz(5.18e9);
        }
    }

    // Obtain per-STA RU assignments from the scheduler.
    auto allocations = dlScheduler->schedule(candidates, channelCenterFrequency, channelBandwidth);
    if (allocations.empty())
        throw cRuntimeError("HeDlMuTxOpFs: scheduler returned empty RU allocation");

    // Assemble the container packet and populate Ieee80211HeMuTag.
    auto container = new Packet("HE-MU-PPDU");

    // Standard QoS data header — broadcast receiver signals HE MU frame.
    auto containerHdr = makeShared<Ieee80211DataHeader>();
    containerHdr->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    containerHdr->setType(ST_DATA_WITH_QOS);
    containerHdr->setChunkLength(b(288)); // minimal 802.11 QoS data header size

    auto muTag = container->addTag<Ieee80211HeMuTag>();

    // 1. Calculate the total sequential ACK sequence duration
    simtime_t totalDuration = simtime_t::ZERO;
    auto hcf = dynamic_cast<Hcf *>(callback);
    auto originatorBAHandler = hcf ? hcf->getOriginatorBlockAckAgreementHandler() : nullptr;
    auto hcfModule = check_and_cast<cModule *>(callback);
    auto rateSelection = check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));

    // Set the frame mode on the container packet first so response mode calculations don't fail due to missing mode
    auto containerMode = rateSelection->computeMode(container, containerHdr, nullptr);
    RateSelection::setFrameMode(container, containerHdr, containerMode);

    std::vector<Packet *> selectedPackets;
    for (size_t idx = 0; idx < allocations.size(); ++idx) {
        const auto& alloc = allocations[idx];
        Packet *staPacket = nullptr;
        int n = pendingQueue->getNumPackets();
        for (int i = 0; i < n; ++i) {
            Packet *pkt = pendingQueue->getPacket(i);
            const auto& hdr = pkt->peekAtFront<Ieee80211MacHeader>();
            if (hdr->getReceiverAddress() == alloc.staAddress) {
                if (std::find(selectedPackets.begin(), selectedPackets.end(), pkt) == selectedPackets.end()) {
                    staPacket = pkt;
                    break;
                }
            }
        }
        if (staPacket != nullptr) {
            selectedPackets.push_back(staPacket);

            bool hasBlockAckAgreement = false;
            auto macHdr = staPacket->peekAtFront<Ieee80211MacHeader>();
            if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macHdr)) {
                // Set the frame mode on the sub-packet first so response mode calculations don't fail due to missing mode
                auto staMode = rateSelection->computeMode(staPacket, dataOrMgmtHdr, nullptr);
                RateSelection::setFrameMode(staPacket, dataOrMgmtHdr, staMode);

                if (auto dataHdr = dynamicPtrCast<const Ieee80211DataHeader>(dataOrMgmtHdr)) {
                    if (dataHdr->getType() == ST_DATA_WITH_QOS && originatorBAHandler != nullptr) {
                        auto agreement = originatorBAHandler->getAgreement(alloc.staAddress, dataHdr->getTid());
                        if (agreement != nullptr && agreement->getIsAddbaResponseReceived()) {
                            hasBlockAckAgreement = true;
                        }
                    }
                }
            }

            simtime_t responseDuration = simtime_t::ZERO;
            simtime_t barDuration = simtime_t::ZERO;
            if (hasBlockAckAgreement) {
                auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
                auto responseMode = rateSelection->computeResponseBlockAckFrameMode(container, dummyReq);
                responseDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
                if (idx >= 1) {
                    barDuration = responseMode->getDuration(B(38));
                }
            }
            else {
                if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macHdr)) {
                    auto responseMode = rateSelection->computeResponseAckFrameMode(staPacket, dataOrMgmtHdr);
                    responseDuration = responseMode->getDuration(LENGTH_ACK);
                }
            }

            if (idx >= 1 && hasBlockAckAgreement) {
                totalDuration += modeSet->getSifsTime() + barDuration + modeSet->getSifsTime() + responseDuration;
            }
            else {
                totalDuration += modeSet->getSifsTime() + responseDuration;
            }
        }
    }

    // Set the totalDuration on the container header to protect the sequential responses
    containerHdr->setDurationField(totalDuration);
    container->insertAtBack(containerHdr);
    container->insertAtBack(makeShared<Ieee80211MacTrailer>());

    // 2. Build the final MU container packet and assign duration/sequence numbers to sub-packets
    for (const auto& alloc : allocations) {
        // Find the first queued packet destined for this STA.
        Packet *staPacket = nullptr;
        int n = pendingQueue->getNumPackets();
        for (int i = 0; i < n; ++i) {
            Packet *pkt = pendingQueue->getPacket(i);
            const auto& hdr = pkt->peekAtFront<Ieee80211MacHeader>();
            if (hdr->getReceiverAddress() == alloc.staAddress) {
                staPacket = pkt;
                break;
            }
        }
        if (staPacket == nullptr) {
            EV_WARN << "HeDlMuTxOpFs: no queued packet for STA " << alloc.staAddress
                    << ", skipping RU " << alloc.ru.index << endl;
            continue;
        }

        // Remove from pending queue, assign sequence number, set duration, and notify the ack handler.
        pendingQueue->removePacket(staPacket);
        auto macHdr = staPacket->peekAtFront<Ieee80211MacHeader>();
        if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macHdr)) {
            auto dataOrMgmtHdrWritable = staPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
            auto heHcf = dynamic_cast<HeHcf *>(callback);
            if (heHcf != nullptr && !dataOrMgmtHdrWritable->getRetry()) {
                auto originatorQosDataService = check_and_cast<OriginatorQosMacDataService *>(heHcf->getOriginatorMacDataService());
                originatorQosDataService->assignSequenceNumber(dataOrMgmtHdrWritable);
            }
            // Set the duration field to totalDuration
            dataOrMgmtHdrWritable->setDurationField(totalDuration);
            staPacket->insertAtFront(dataOrMgmtHdrWritable);

            ackHandler->frameGotInProgress(dataOrMgmtHdrWritable);
        }

        // Store a duplicate in the tag (tag owns the copy).
        Packet *dupPkt = staPacket->dup();
        muTag->addAllocation(alloc.ru.index, dupPkt);
        ActiveAllocation activeAlloc;
        activeAlloc.staAddress = alloc.staAddress;
        activeAlloc.ruIndex = alloc.ru.index;
        activeAllocations.push_back(activeAlloc);

        // Add to inProgressFrames!
        context->getInProgressFrames()->addInProgressFrame(staPacket);
    }

    if (muTag->getAllocations().empty())
        throw cRuntimeError("HeDlMuTxOpFs: no packets assembled for MU-OFDMA transmission");

    EV_INFO << "HeDlMuTxOpFs: assembled HE MU PPDU with "
            << muTag->getAllocations().size() << " RU allocations. Total sequential duration = " << totalDuration << endl;
    return container;
}

IFrameSequenceStep *HeDlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    int numActive = activeAllocations.size();
    if (step == 0) {
        containerPacket = buildMuContainerPacket(context);
        return new TransmitStep(containerPacket, context->getIfs(), true);
    }
    else if (step >= 1 && step < 2 * numActive) {
        int idx = step / 2;
        auto targetSta = activeAllocations[idx].staAddress;
        Packet *transmittedPacket = nullptr;
        auto inProgress = context->getInProgressFrames();
        int n = inProgress->getLength();
        for (int i = 0; i < n; ++i) {
            Packet *pkt = inProgress->getFrames(i);
            const auto& hdr = pkt->peekAtFront<Ieee80211MacHeader>();
            if (hdr->getReceiverAddress() == targetSta) {
                transmittedPacket = pkt;
                break;
            }
        }

        bool hasBlockAckAgreement = false;
        if (transmittedPacket != nullptr) {
            auto macHdr = transmittedPacket->peekAtFront<Ieee80211MacHeader>();
            if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macHdr)) {
                if (auto dataHdr = dynamicPtrCast<const Ieee80211DataHeader>(dataOrMgmtHdr)) {
                    auto hcf = dynamic_cast<Hcf *>(callback);
                    auto originatorBAHandler = hcf ? hcf->getOriginatorBlockAckAgreementHandler() : nullptr;
                    if (dataHdr->getType() == ST_DATA_WITH_QOS && originatorBAHandler != nullptr) {
                        auto agreement = originatorBAHandler->getAgreement(targetSta, dataHdr->getTid());
                        if (agreement != nullptr && agreement->getIsAddbaResponseReceived()) {
                            hasBlockAckAgreement = true;
                        }
                    }
                }
            }
        }

        if (step % 2 == 0) {
            auto qosContext = context->getQoSContext();
            auto receiverAddr = targetSta;
            Tid tid = 0;
            SequenceNumberCyclic startingSequenceNumber;
            if (transmittedPacket != nullptr) {
                auto macHdr = transmittedPacket->peekAtFront<Ieee80211MacHeader>();
                if (auto dataHdr = dynamicPtrCast<const Ieee80211DataHeader>(macHdr)) {
                    tid = dataHdr->getTid();
                    startingSequenceNumber = dataHdr->getSequenceNumber();
                }
            }
            Ptr<Ieee80211BlockAckReq> blockAckReq;
            if (qosContext != nullptr && qosContext->blockAckProcedure != nullptr) {
                blockAckReq = qosContext->blockAckProcedure->buildBasicBlockAckReqFrame(receiverAddr, tid, startingSequenceNumber);
            }
            else {
                auto basicBlockAckReq = makeShared<Ieee80211BasicBlockAckReq>();
                basicBlockAckReq->setReceiverAddress(receiverAddr);
                basicBlockAckReq->setTidInfo(tid);
                basicBlockAckReq->setStartingSequenceNumber(startingSequenceNumber);
                blockAckReq = basicBlockAckReq;
            }
            auto blockAckPacket = new Packet("BasicBlockAckReq", blockAckReq);
            blockAckPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
            return new TransmitStep(blockAckPacket, context->getIfs(), true);
        }
        else {
            auto hcfModule = check_and_cast<cModule *>(callback);
            auto rateSelection = check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));
            simtime_t responseDuration = simtime_t::ZERO;
            auto txStep = check_and_cast<ITransmitStep *>(context->getLastStep());
            auto lastTransmittedPacket = txStep->getFrameToTransmit();
            if (hasBlockAckAgreement) {
                auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
                auto responseMode = rateSelection->computeResponseBlockAckFrameMode(lastTransmittedPacket, dummyReq);
                responseDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
            }
            else if (transmittedPacket != nullptr) {
                auto macHdr = transmittedPacket->peekAtFront<Ieee80211MacHeader>();
                if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macHdr)) {
                    auto responseMode = rateSelection->computeResponseAckFrameMode(transmittedPacket, dataOrMgmtHdr);
                    responseDuration = responseMode->getDuration(LENGTH_ACK);
                }
            }
            else {
                auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
                auto responseMode = rateSelection->computeResponseBlockAckFrameMode(lastTransmittedPacket, dummyReq);
                responseDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
            }

            simtime_t timeout = modeSet->getSifsTime() + responseDuration + modeSet->getSlotTime();
            return new ReceiveStep(timeout);
        }
    }
    else if (step == 2 * numActive) {
        return nullptr; // sequence done
    }
    else {
        throw cRuntimeError("HeDlMuTxOpFs: unknown step %d", step);
    }
}

bool HeDlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    int numActive = activeAllocations.size();
    if (step == 0) {
        step++;
        return true;
    }
    else if (step >= 1 && step < 2 * numActive) {
        if (step % 2 == 0) {
            step++;
            return true;
        }
        else {
            int idx = step / 2;
            auto targetSta = activeAllocations[idx].staAddress;
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
            auto receivedPacket = receiveStep->getReceivedFrame();

            Packet *transmittedPacket = nullptr;
            auto inProgress = context->getInProgressFrames();
            int n = inProgress->getLength();
            for (int i = 0; i < n; ++i) {
                Packet *pkt = inProgress->getFrames(i);
                const auto& hdr = pkt->peekAtFront<Ieee80211MacHeader>();
                if (hdr->getReceiverAddress() == targetSta) {
                    transmittedPacket = pkt;
                    break;
                }
            }

            if (receivedPacket != nullptr) {
                receiveStep->setCompletion(IFrameSequenceStep::Completion::ACCEPTED);
                if (transmittedPacket != nullptr) {
                    callback->originatorProcessReceivedFrame(receivedPacket, transmittedPacket);
                }
            }
            else {
                receiveStep->setCompletion(IFrameSequenceStep::Completion::REJECTED);
                if (transmittedPacket != nullptr) {
                    EV_WARN << "HeDlMuTxOpFs: sequential BlockAck timeout for STA " << targetSta
                            << ", triggering failure recovery." << endl;
                    callback->originatorProcessFailedFrame(transmittedPacket);
                }
            }
            step++;
            return true;
        }
    }
    else {
        throw cRuntimeError("HeDlMuTxOpFs: unknown step %d", step);
    }
}

} // namespace ieee80211
} // namespace inet
