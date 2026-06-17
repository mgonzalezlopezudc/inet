//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
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
    struct SelectedAllocation {
        IIeee80211HeDlScheduler::RuAllocation allocation;
        Packet *packet = nullptr;
        Ptr<const Ieee80211DataHeader> dataHeader;
    };

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
    auto qosContext = context->getQoSContext();
    auto originatorBAHandler = qosContext != nullptr ? qosContext->blockAckAgreementHandler : nullptr;
    if (originatorBAHandler == nullptr && hcf != nullptr)
        originatorBAHandler = hcf->getOriginatorBlockAckAgreementHandler();
    auto hcfModule = check_and_cast<cModule *>(callback);
    auto rateSelection = check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));

    // Set the frame mode on the container packet first so response mode calculations don't fail due to missing mode
    auto containerMode = rateSelection->computeMode(container, containerHdr, nullptr);
    RateSelection::setFrameMode(container, containerHdr, containerMode);

    auto warnIneligible = [] (Packet *packet, const MacAddress& receiverAddress, Tid tid, int ruIndex, const char *reason) {
        EV_WARN << "HeDlMuTxOpFs: skipping MU-ineligible packet "
                << (packet == nullptr ? "<none>" : packet->getName())
                << " for receiver " << receiverAddress
                << ", TID " << (int)tid
                << ", RU " << ruIndex
                << ": " << reason << endl;
    };

    auto getIneligibilityReason = [] (IOriginatorBlockAckAgreementHandler *handler, const MacAddress& receiverAddress, Tid tid) -> const char * {
        if (handler == nullptr)
            return "null originator Block Ack agreement handler";
        auto agreement = handler->getAgreement(receiverAddress, tid);
        if (agreement == nullptr)
            return "missing originator Block Ack agreement";
        if (!agreement->getIsAddbaResponseReceived())
            return "ADDBA response not received";
        return nullptr;
    };

    std::vector<Packet *> selectedPackets;
    std::vector<SelectedAllocation> selectedAllocations;
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
        if (staPacket == nullptr) {
            warnIneligible(nullptr, alloc.staAddress, -1, alloc.ru.index, "no queued packet for scheduled receiver");
            continue;
        }

        selectedPackets.push_back(staPacket);
        auto macHdr = staPacket->peekAtFront<Ieee80211MacHeader>();
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(macHdr);
        if (dataHeader == nullptr) {
            warnIneligible(staPacket, alloc.staAddress, -1, alloc.ru.index, "packet is not a data frame");
            continue;
        }
        if (dataHeader->getType() != ST_DATA_WITH_QOS) {
            warnIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index, "packet is not QoS data");
            continue;
        }
        if (dataHeader->getReceiverAddress() != alloc.staAddress) {
            warnIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index, "packet receiver does not match scheduler allocation");
            continue;
        }
        if (!hasActiveOriginatorBlockAckAgreement(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid())) {
            warnIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index,
                    getIneligibilityReason(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid()));
            continue;
        }

        SelectedAllocation selectedAllocation;
        selectedAllocation.allocation = alloc;
        selectedAllocation.packet = staPacket;
        selectedAllocation.dataHeader = dataHeader;
        selectedAllocations.push_back(selectedAllocation);
    }

    if (selectedAllocations.size() < 2) {
        EV_WARN << "HeDlMuTxOpFs: aborting HE MU PPDU assembly because only "
                << selectedAllocations.size() << " active Block Ack allocations remain before queue mutation." << endl;
        delete container;
        throw cRuntimeError("HeDlMuTxOpFs: fewer than two active Block Ack allocations for MU-OFDMA transmission");
    }

    auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
    auto responseMode = rateSelection->computeResponseBlockAckFrameMode(container, dummyReq);
    for (size_t idx = 0; idx < selectedAllocations.size(); ++idx) {
        simtime_t responseDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
        simtime_t barDuration = idx >= 1 ? responseMode->getDuration(B(38)) : simtime_t::ZERO;
        if (idx >= 1)
            totalDuration += modeSet->getSifsTime() + barDuration + modeSet->getSifsTime() + responseDuration;
        else
            totalDuration += modeSet->getSifsTime() + responseDuration;

        auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(selectedAllocations[idx].dataHeader);
        auto staMode = rateSelection->computeMode(selectedAllocations[idx].packet, dataOrMgmtHdr, nullptr);
        RateSelection::setFrameMode(selectedAllocations[idx].packet, dataOrMgmtHdr, staMode);
    }

    // Set the totalDuration on the container header to protect the sequential responses
    containerHdr->setDurationField(totalDuration);
    container->insertAtBack(containerHdr);
    container->insertAtBack(makeShared<Ieee80211MacTrailer>());

    std::vector<SelectedAllocation> finalAllocations;
    for (const auto& selectedAllocation : selectedAllocations) {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(selectedAllocation.packet->peekAtFront<Ieee80211MacHeader>());
        if (dataHeader == nullptr || dataHeader->getType() != ST_DATA_WITH_QOS ||
                dataHeader->getReceiverAddress() != selectedAllocation.allocation.staAddress ||
                !hasActiveOriginatorBlockAckAgreement(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid())) {
            Tid tid = dataHeader == nullptr ? -1 : dataHeader->getTid();
            auto receiverAddress = dataHeader == nullptr ? selectedAllocation.allocation.staAddress : dataHeader->getReceiverAddress();
            warnIneligible(selectedAllocation.packet, receiverAddress, tid, selectedAllocation.allocation.ru.index,
                    "failed final validation before queue removal");
            continue;
        }
        finalAllocations.push_back(selectedAllocation);
    }

    if (finalAllocations.size() < 2) {
        EV_WARN << "HeDlMuTxOpFs: aborting HE MU PPDU assembly because only "
                << finalAllocations.size() << " active Block Ack allocations remain after final validation." << endl;
        delete container;
        throw cRuntimeError("HeDlMuTxOpFs: fewer than two active Block Ack allocations after final validation");
    }

    // 2. Build the final MU container packet and assign duration/sequence numbers to sub-packets.
    for (const auto& selectedAllocation : finalAllocations) {
        const auto& alloc = selectedAllocation.allocation;
        Packet *staPacket = selectedAllocation.packet;
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(staPacket->peekAtFront<Ieee80211MacHeader>());
        if (dataHeader == nullptr || dataHeader->getType() != ST_DATA_WITH_QOS ||
                !hasActiveOriginatorBlockAckAgreement(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid())) {
            Tid tid = dataHeader == nullptr ? -1 : dataHeader->getTid();
            auto receiverAddress = dataHeader == nullptr ? alloc.staAddress : dataHeader->getReceiverAddress();
            warnIneligible(staPacket, receiverAddress, tid, alloc.ru.index, "failed final validation before tag allocation");
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
            if (auto dataHdrWritable = dynamicPtrCast<Ieee80211DataHeader>(dataOrMgmtHdrWritable))
                dataHdrWritable->setAckPolicy(BLOCK_ACK);
            // Set the duration field to totalDuration
            dataOrMgmtHdrWritable->setDurationField(totalDuration);
            staPacket->insertAtFront(dataOrMgmtHdrWritable);

            ackHandler->frameGotInProgress(dataOrMgmtHdrWritable);
        }

        // Store a duplicate in the tag (tag owns the copy).
        Packet *dupPkt = staPacket->dup();
        muTag->addAllocation(alloc.ru, alloc.staAddress, dupPkt);
        ActiveAllocation activeAlloc;
        activeAlloc.staAddress = alloc.staAddress;
        activeAlloc.tid = dataHeader->getTid();
        activeAlloc.ruIndex = alloc.ru.index;
        activeAllocations.push_back(activeAlloc);

        // Add to inProgressFrames!
        context->getInProgressFrames()->addInProgressFrame(staPacket);
    }

    if (muTag->getAllocations().size() < 2)
        throw cRuntimeError("HeDlMuTxOpFs: fewer than two packets assembled for MU-OFDMA transmission");

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

        if (step % 2 == 0) {
            auto qosContext = context->getQoSContext();
            auto receiverAddr = targetSta;
            Tid tid = activeAllocations[idx].tid;
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
            auto hcfModule = check_and_cast<cModule *>(callback);
            auto rateSelection = check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));
            auto responseMode = rateSelection->computeResponseBlockAckFrameMode(transmittedPacket != nullptr ? transmittedPacket : containerPacket, blockAckReq);
            simtime_t blockAckDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
            simtime_t barDuration = responseMode->getDuration(B(38));
            simtime_t remainingDuration = modeSet->getSifsTime() + blockAckDuration;
            for (int nextIdx = idx + 1; nextIdx < numActive; nextIdx++)
                remainingDuration += modeSet->getSifsTime() + barDuration + modeSet->getSifsTime() + blockAckDuration;
            blockAckReq->setDurationField(remainingDuration);
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
            auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
            auto responseMode = rateSelection->computeResponseBlockAckFrameMode(lastTransmittedPacket, dummyReq);
            responseDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);

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
