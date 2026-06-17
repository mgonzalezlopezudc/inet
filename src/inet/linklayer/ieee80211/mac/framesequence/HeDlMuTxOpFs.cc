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
#include "inet/linklayer/ieee80211/mac/framesequence/GenericFrameSequences.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/rateselection/RateSelection.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

class HeDlMuPpduFs : public IFrameSequence
{
  protected:
    HeDlMuTxOpFs *owner = nullptr;
    int step = -1;

  public:
    explicit HeDlMuPpduFs(HeDlMuTxOpFs *owner) : owner(owner) {}

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override { step = 0; }

    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override
    {
        switch (step) {
            case 0:
                owner->containerPacket = owner->buildMuContainerPacket(context);
                return new TransmitStep(owner->containerPacket, context->getIfs(), true);
            case 1:
                return nullptr;
            default:
                throw cRuntimeError("HeDlMuPpduFs: unknown step %d", step);
        }
    }

    virtual bool completeStep(FrameSequenceContext *context) override
    {
        switch (step) {
            case 0:
                step++;
                return true;
            default:
                throw cRuntimeError("HeDlMuPpduFs: unknown step %d", step);
        }
    }

    virtual std::string getHistory() const override { return "HE-MU-PPDU"; }
};

class HeDlMuPerStaBlockAckFs : public IFrameSequence
{
  protected:
    HeDlMuTxOpFs *owner = nullptr;
    int allocationIndex = -1;
    int firstStep = -1;
    int step = -1;

  protected:
    const HeDlMuTxOpFs::ActiveAllocation& getActiveAllocation() const
    {
        return owner->activeAllocations.at(allocationIndex);
    }

    IQosRateSelection *getRateSelection() const
    {
        auto hcfModule = check_and_cast<cModule *>(owner->callback);
        return check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));
    }

    Packet *findTransmittedPacket(FrameSequenceContext *context) const
    {
        const auto& targetSta = getActiveAllocation().staAddress;
        auto inProgress = context->getInProgressFrames();
        for (int i = 0; i < inProgress->getLength(); ++i) {
            Packet *packet = inProgress->getFrames(i);
            const auto& header = packet->peekAtFront<Ieee80211MacHeader>();
            if (header->getReceiverAddress() == targetSta)
                return packet;
        }
        return nullptr;
    }

    Ptr<Ieee80211BlockAckReq> buildBlockAckReq(FrameSequenceContext *context, Packet *transmittedPacket) const
    {
        auto receiverAddress = getActiveAllocation().staAddress;
        Tid tid = getActiveAllocation().tid;
        SequenceNumberCyclic startingSequenceNumber;
        if (transmittedPacket != nullptr) {
            auto macHeader = transmittedPacket->peekAtFront<Ieee80211MacHeader>();
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(macHeader)) {
                tid = dataHeader->getTid();
                startingSequenceNumber = dataHeader->getSequenceNumber();
            }
        }

        auto qosContext = context->getQoSContext();
        if (qosContext != nullptr && qosContext->blockAckProcedure != nullptr)
            return qosContext->blockAckProcedure->buildBasicBlockAckReqFrame(receiverAddress, tid, startingSequenceNumber);

        auto blockAckReq = makeShared<Ieee80211BasicBlockAckReq>();
        blockAckReq->setReceiverAddress(receiverAddress);
        blockAckReq->setTidInfo(tid);
        blockAckReq->setStartingSequenceNumber(startingSequenceNumber);
        return blockAckReq;
    }

    simtime_t computeRemainingBarDuration(const IIeee80211Mode *responseMode) const
    {
        auto blockAckDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
        auto barDuration = responseMode->getDuration(B(38));
        auto remainingDuration = owner->modeSet->getSifsTime() + blockAckDuration;
        for (int nextIndex = allocationIndex + 1; nextIndex < (int)owner->activeAllocations.size(); nextIndex++)
            remainingDuration += owner->modeSet->getSifsTime() + barDuration + owner->modeSet->getSifsTime() + blockAckDuration;
        return remainingDuration;
    }

    simtime_t computeBlockAckTimeout(Packet *lastTransmittedPacket) const
    {
        auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
        auto responseMode = getRateSelection()->computeResponseBlockAckFrameMode(lastTransmittedPacket, dummyReq);
        return owner->modeSet->getSifsTime() + responseMode->getDuration(LENGTH_BASIC_BLOCKACK) + owner->modeSet->getSlotTime();
    }

    IFrameSequenceStep *prepareBarStep(FrameSequenceContext *context)
    {
        auto transmittedPacket = findTransmittedPacket(context);
        auto blockAckReq = buildBlockAckReq(context, transmittedPacket);
        auto responseMode = getRateSelection()->computeResponseBlockAckFrameMode(
                transmittedPacket != nullptr ? transmittedPacket : owner->containerPacket, blockAckReq);
        blockAckReq->setDurationField(computeRemainingBarDuration(responseMode));
        auto blockAckPacket = new Packet("BasicBlockAckReq", blockAckReq);
        blockAckPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
        return new TransmitStep(blockAckPacket, context->getIfs(), true);
    }

    IFrameSequenceStep *prepareBlockAckStep(FrameSequenceContext *context)
    {
        auto txStep = check_and_cast<ITransmitStep *>(context->getLastStep());
        return new ReceiveStep(computeBlockAckTimeout(txStep->getFrameToTransmit()));
    }

    bool completeBlockAckStep(FrameSequenceContext *context)
    {
        auto receiveStep = check_and_cast<IReceiveStep *>(context->getStep(firstStep + step));
        auto receivedPacket = receiveStep->getReceivedFrame();
        auto transmittedPacket = findTransmittedPacket(context);
        if (receivedPacket != nullptr)
            receiveStep->setCompletion(IFrameSequenceStep::Completion::ACCEPTED);
        else {
            receiveStep->setCompletion(IFrameSequenceStep::Completion::REJECTED);
            if (transmittedPacket != nullptr) {
                EV_WARN << "HeDlMuTxOpFs: sequential BlockAck timeout for STA " << getActiveAllocation().staAddress
                        << ", triggering failure recovery." << endl;
                owner->callback->originatorProcessFailedFrame(transmittedPacket);
            }
        }
        step++;
        return true;
    }

  public:
    HeDlMuPerStaBlockAckFs(HeDlMuTxOpFs *owner, int allocationIndex) :
        owner(owner),
        allocationIndex(allocationIndex)
    {
    }

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override
    {
        this->firstStep = firstStep;
        step = 0;
    }

    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override
    {
        if (allocationIndex == 0) {
            switch (step) {
                case 0:
                    return prepareBlockAckStep(context);
                case 1:
                    return nullptr;
                default:
                    throw cRuntimeError("HeDlMuPerStaBlockAckFs: unknown first-allocation step %d", step);
            }
        }
        else {
            switch (step) {
                case 0:
                    return prepareBarStep(context);
                case 1:
                    return prepareBlockAckStep(context);
                case 2:
                    return nullptr;
                default:
                    throw cRuntimeError("HeDlMuPerStaBlockAckFs: unknown step %d", step);
            }
        }
    }

    virtual bool completeStep(FrameSequenceContext *context) override
    {
        if (allocationIndex == 0) {
            switch (step) {
                case 0:
                    return completeBlockAckStep(context);
                default:
                    throw cRuntimeError("HeDlMuPerStaBlockAckFs: unknown first-allocation step %d", step);
            }
        }
        else {
            switch (step) {
                case 0:
                    step++;
                    return true;
                case 1:
                    return completeBlockAckStep(context);
                default:
                    throw cRuntimeError("HeDlMuPerStaBlockAckFs: unknown step %d", step);
            }
        }
    }

    virtual std::string getHistory() const override { return allocationIndex == 0 ? "BLOCKACK" : "BLOCKACKREQ BLOCKACK"; }
};

class HeDlMuSequentialBlockAckFs : public IFrameSequence
{
  protected:
    HeDlMuTxOpFs *owner = nullptr;
    int firstStep = -1;
    int step = -1;
    int allocationIndex = -1;
    HeDlMuPerStaBlockAckFs *currentSequence = nullptr;

  protected:
    void startCurrentSequence(FrameSequenceContext *context)
    {
        delete currentSequence;
        currentSequence = new HeDlMuPerStaBlockAckFs(owner, allocationIndex);
        currentSequence->startSequence(context, firstStep + step);
    }

  public:
    explicit HeDlMuSequentialBlockAckFs(HeDlMuTxOpFs *owner) : owner(owner) {}

    virtual ~HeDlMuSequentialBlockAckFs() { delete currentSequence; }

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override
    {
        this->firstStep = firstStep;
        step = 0;
        allocationIndex = 0;
        delete currentSequence;
        currentSequence = nullptr;
        if (!owner->activeAllocations.empty())
            startCurrentSequence(context);
    }

    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override
    {
        while (allocationIndex < (int)owner->activeAllocations.size()) {
            auto nextStep = currentSequence->prepareStep(context);
            if (nextStep != nullptr)
                return nextStep;
            allocationIndex++;
            if (allocationIndex < (int)owner->activeAllocations.size())
                startCurrentSequence(context);
        }
        return nullptr;
    }

    virtual bool completeStep(FrameSequenceContext *context) override
    {
        step++;
        return currentSequence->completeStep(context);
    }

    virtual std::string getHistory() const override
    {
        return currentSequence == nullptr ? "SEQ-BLOCKACK" : currentSequence->getHistory();
    }
};

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
    sequence = new SequentialFs({new HeDlMuPpduFs(this), new HeDlMuSequentialBlockAckFs(this)});
}

void HeDlMuTxOpFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    this->firstStep = firstStep;
    step = 0;
    sequence->startSequence(context, firstStep);
}

HeDlMuTxOpFs::~HeDlMuTxOpFs()
{
    delete sequence;
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

    // Assemble the HE MU PPDU container packet.
    auto container = new Packet("HE-MU-PPDU");

    // Standard QoS data header — broadcast receiver signals HE MU frame.
    auto containerHdr = makeShared<Ieee80211DataHeader>();
    containerHdr->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    containerHdr->setType(ST_DATA_WITH_QOS);
    containerHdr->setChunkLength(b(288)); // minimal 802.11 QoS data header size

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

            if (staPacket->getDataLength() >= B(4) && dynamicPtrCast<const Ieee80211MacTrailer>(staPacket->peekAtBack(B(4))) != nullptr) {
                auto trailer = staPacket->removeAtBack<Ieee80211MacTrailer>(B(4));
                trailer->setFcsMode(FCS_DECLARED_CORRECT);
                staPacket->insertAtBack(trailer);
            }

            ackHandler->frameGotInProgress(dataOrMgmtHdrWritable);
        }

        auto payloadHeader = makeShared<Ieee80211HeMuRuPayloadHeader>();
        payloadHeader->setRuIndex(alloc.ru.index);
        payloadHeader->setStaId(computeHeMuStaId(alloc.staAddress));
        payloadHeader->setMpduLength(B(staPacket->getByteLength()));
        container->insertAtBack(payloadHeader);
        container->insertAtBack(staPacket->peekData());

        ActiveAllocation activeAlloc;
        activeAlloc.staAddress = alloc.staAddress;
        activeAlloc.tid = dataHeader->getTid();
        activeAlloc.ruIndex = alloc.ru.index;
        activeAlloc.packet = staPacket;
        activeAllocations.push_back(activeAlloc);

        // Add to inProgressFrames!
        context->getInProgressFrames()->addInProgressFrame(staPacket);
    }

    container->insertAtBack(makeShared<Ieee80211MacTrailer>());

    if (activeAllocations.size() < 2)
        throw cRuntimeError("HeDlMuTxOpFs: fewer than two packets assembled for MU-OFDMA transmission");

    EV_INFO << "HeDlMuTxOpFs: assembled HE MU PPDU with "
            << activeAllocations.size() << " RU allocations. Total sequential duration = " << totalDuration << endl;
    return container;
}

IFrameSequenceStep *HeDlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    return sequence->prepareStep(context);
}

bool HeDlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    step++;
    return sequence->completeStep(context);
}

std::string HeDlMuTxOpFs::getHistory() const
{
    return step == -1 ? "HE-DL-MU" : sequence->getHistory();
}

} // namespace ieee80211
} // namespace inet
