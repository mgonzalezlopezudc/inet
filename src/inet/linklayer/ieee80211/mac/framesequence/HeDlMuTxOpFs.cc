//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

#include <algorithm>
#include <atomic>
#include <map>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mac/framesequence/GenericFrameSequences.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/rateselection/RateSelection.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

namespace {

B calculateAmpduPsduLength(const std::vector<Packet *>& packets)
{
    if (packets.empty())
        return B(0);
    B length(0);
    for (size_t i = 0; i < packets.size(); ++i) {
        B subframeLength = B(4) + B(packets[i]->getByteLength());
        length += subframeLength;
        if (i + 1 != packets.size())
            length += B((4 - subframeLength.get<B>() % 4) % 4);
    }
    return length;
}

} // namespace

class HeDlMuPpduFs : public IFrameSequence
{
  protected:
    HeDlMuTxOpFs *owner = nullptr;
    int step = -1;

  public:
    explicit HeDlMuPpduFs(HeDlMuTxOpFs *owner) : owner(owner) { ASSERT(owner != nullptr); }

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override { ASSERT(context != nullptr); step = 0; }

    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override
    {
        ASSERT(context != nullptr);
        switch (step) {
            case 0:
                owner->containerPacket = owner->buildMuContainerPacket(context);
                if (owner->containerPacket == nullptr) {
                    EV_WARN << "HeDlMuPpduFs: container packet build failed, aborting HE DL MU sequence\n";
                    step = 1;
                    return nullptr;
                }
                EV_DEBUG << "HeDlMuPpduFs: transmitting HE DL MU container packet\n";
                return new TransmitStep(owner->containerPacket, context->getIfs(), true);
            case 1:
                return nullptr;
            default:
                throw cRuntimeError("HeDlMuPpduFs: unknown step %d", step);
        }
    }

    virtual bool completeStep(FrameSequenceContext *context) override
    {
        ASSERT(context != nullptr);
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
        ASSERT(allocationIndex >= 0 && allocationIndex < (int)owner->activeAllocations.size());
        return owner->activeAllocations.at(allocationIndex);
    }

    IQosRateSelection *getRateSelection() const
    {
        auto hcfModule = check_and_cast<cModule *>(owner->callback);
        return check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));
    }

    Packet *findTransmittedPacket(FrameSequenceContext *context) const
    {
        // The HE MU PPDU is represented by a container packet in the frame
        // sequence. Its per-STA MPDUs are retained in the active allocation,
        // not necessarily as individual frames in the context. Use that
        // recorded MPDU for the BAR sequence number and response mode.
        if (getActiveAllocation().packet != nullptr)
            return getActiveAllocation().packet;

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

        auto hcfModule = dynamic_cast<cModule *>(owner->callback);
        auto macModule = hcfModule != nullptr ? dynamic_cast<Ieee80211Mac *>(hcfModule->getParentModule()) : nullptr;
        auto mib = macModule != nullptr ? macModule->getMib() : nullptr;
        auto negotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(receiverAddress) : nullptr;
        if (negotiated != nullptr && negotiated->intersection.multiTidAggregationTx) {
            auto multiTidReq = makeShared<Ieee80211MultiTidBlockAckReq>();
            multiTidReq->setReceiverAddress(receiverAddress);
            multiTidReq->setTransmitterAddress(macModule->getAddress());
            multiTidReq->setRecordsArraySize(1);
            Ieee80211MultiTidBlockAckReqRecord rec;
            rec.tid = tid;
            rec.startingSequenceNumber = startingSequenceNumber.get();
            multiTidReq->setRecords(0, rec);
            multiTidReq->setChunkLength(B(22));
            return multiTidReq;
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
        auto hcfModule = dynamic_cast<cModule *>(owner->callback);
        auto macModule = hcfModule != nullptr ? dynamic_cast<Ieee80211Mac *>(hcfModule->getParentModule()) : nullptr;
        auto mib = macModule != nullptr ? macModule->getMib() : nullptr;
        auto negotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(getActiveAllocation().staAddress) : nullptr;
        bool multiTid = (negotiated != nullptr && negotiated->intersection.multiTidAggregationTx);

        auto blockAckDuration = responseMode->getDuration(multiTid ? b(B(29)) : LENGTH_BASIC_BLOCKACK);
        auto barDuration = responseMode->getDuration(multiTid ? B(22) : B(38));
        auto remainingDuration = owner->modeSet->getSifsTime() + blockAckDuration;
        for (int nextIndex = allocationIndex + 1; nextIndex < (int)owner->activeAllocations.size(); nextIndex++) {
            auto nextNegotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(owner->activeAllocations.at(nextIndex).staAddress) : nullptr;
            bool nextMultiTid = (nextNegotiated != nullptr && nextNegotiated->intersection.multiTidAggregationTx);
            auto nextBlockAckDuration = responseMode->getDuration(nextMultiTid ? b(B(29)) : LENGTH_BASIC_BLOCKACK);
            auto nextBarDuration = responseMode->getDuration(nextMultiTid ? B(22) : B(38));
            remainingDuration += owner->modeSet->getSifsTime() + nextBarDuration + owner->modeSet->getSifsTime() + nextBlockAckDuration;
        }
        return remainingDuration;
    }

    simtime_t computeBlockAckTimeout(Packet *lastTransmittedPacket) const
    {
        auto hcfModule = dynamic_cast<cModule *>(owner->callback);
        auto macModule = hcfModule != nullptr ? dynamic_cast<Ieee80211Mac *>(hcfModule->getParentModule()) : nullptr;
        auto mib = macModule != nullptr ? macModule->getMib() : nullptr;
        auto negotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(getActiveAllocation().staAddress) : nullptr;
        bool multiTid = (negotiated != nullptr && negotiated->intersection.multiTidAggregationTx);

        Ptr<Ieee80211BlockAckReq> dummyReq;
        if (multiTid) {
            auto multiTidReq = makeShared<Ieee80211MultiTidBlockAckReq>();
            multiTidReq->setRecordsArraySize(1);
            dummyReq = multiTidReq;
        } else {
            dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
        }

        auto responseMode = getRateSelection()->computeResponseBlockAckFrameMode(lastTransmittedPacket, dummyReq);
        return owner->modeSet->getSifsTime() + responseMode->getDuration(multiTid ? b(B(29)) : LENGTH_BASIC_BLOCKACK) + owner->modeSet->getSlotTime();
    }

    IFrameSequenceStep *prepareBarStep(FrameSequenceContext *context)
    {
        auto transmittedPacket = findTransmittedPacket(context);
        auto blockAckReq = buildBlockAckReq(context, transmittedPacket);
        auto responseMode = getRateSelection()->computeResponseBlockAckFrameMode(
                transmittedPacket != nullptr ? transmittedPacket : owner->containerPacket, blockAckReq);
        blockAckReq->setDurationField(computeRemainingBarDuration(responseMode));
        auto blockAckPacket = new Packet(dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(blockAckReq) ? "MultiTidBlockAckReq" : "BasicBlockAckReq", blockAckReq);
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
        if (receivedPacket != nullptr) {
            receiveStep->setCompletion(IFrameSequenceStep::Completion::ACCEPTED);
            EV_DEBUG << "HE DL MU TxOp FS: received BlockAck from STA " << getActiveAllocation().staAddress << "\n";
        }
        else {
            receiveStep->setCompletion(IFrameSequenceStep::Completion::REJECTED);
            if (transmittedPacket != nullptr) {
                EV_WARN << "HE DL MU TxOp FS: sequential BlockAck timeout for STA " << getActiveAllocation().staAddress
                        << ", triggering failure recovery." << endl;
                owner->callback->originatorProcessFailedFrame(transmittedPacket);
                for (auto packet : getActiveAllocation().packets)
                    if (packet != transmittedPacket)
                        owner->callback->originatorProcessFailedFrame(packet);
            }
            else {
                EV_WARN << "HE DL MU TxOp FS: sequential BlockAck timeout for STA " << getActiveAllocation().staAddress
                        << " but no transmitted packet recorded" << endl;
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
        ASSERT(owner != nullptr);
    }

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override
    {
        ASSERT(context != nullptr);
        this->firstStep = firstStep;
        step = 0;
    }

    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override
    {
        ASSERT(context != nullptr);
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

    virtual bool completeStep(FrameSequenceContext *context) override
    {
        ASSERT(context != nullptr);
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

    virtual std::string getHistory() const override { return "BLOCKACKREQ BLOCKACK"; }
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
    explicit HeDlMuSequentialBlockAckFs(HeDlMuTxOpFs *owner) : owner(owner) { ASSERT(owner != nullptr); }

    virtual ~HeDlMuSequentialBlockAckFs() { delete currentSequence; }

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override
    {
        ASSERT(context != nullptr);
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
        ASSERT(context != nullptr);
        while (allocationIndex < (int)owner->activeAllocations.size()) {
            ASSERT(currentSequence != nullptr);
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
        ASSERT(context != nullptr);
        ASSERT(currentSequence != nullptr);
        step++;
        return currentSequence->completeStep(context);
    }

    virtual std::string getHistory() const override
    {
        return currentSequence == nullptr ? "SEQ-BLOCKACK" : currentSequence->getHistory();
    }
};

class HeDlMuBarBlockAckFs : public IFrameSequence
{
  protected:
    HeDlMuTxOpFs *owner = nullptr;
    int step = -1;

    Packet *buildMuBarTrigger() const
    {
        ASSERT(!owner->activeAllocations.empty());
        auto header = makeShared<Ieee80211TriggerFrame>();
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        auto hcfModule = check_and_cast<cModule *>(owner->callback);
        auto mac = check_and_cast<Ieee80211Mac *>(
                getContainingNicModule(hcfModule)->getSubmodule("mac"));
        ASSERT(mac != nullptr);
        header->setTransmitterAddress(mac->getAddress());
        header->setTriggerType(2); // MU-BAR Trigger
        header->setTriggerId(owner->ackTriggerId);
        header->setUsersArraySize(owner->activeAllocations.size());
        simtime_t commonDuration = SIMTIME_ZERO;
        for (size_t i = 0; i < owner->activeAllocations.size(); ++i) {
            const auto& allocation = owner->activeAllocations[i];
            Ieee80211HeTriggerUserInfo user;
            user.aid = allocation.associationId;
            user.ruIndex = allocation.ruIndex;
            user.ruToneSize = allocation.ru.toneSize;
            user.ruToneOffset = allocation.ru.toneOffset;
            user.mcs = 0;
            user.tid = allocation.tid;
            header->setUsers(i, user);
            commonDuration = std::max(commonDuration,
                    estimateHeMuUserDuration(LENGTH_BASIC_BLOCKACK,
                            allocation.ru.toneSize, 0));
        }
        header->setCommonDuration(commonDuration);
        header->setDurationField(owner->modeSet->getSifsTime() + commonDuration);
        header->setChunkLength(B(24 + 6 * owner->activeAllocations.size()));
        auto packet = new Packet("HE-MU-BAR-Trigger", header);
        packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
        EV_INFO << "HE DL MU-BAR FS: built MU-BAR trigger for " << owner->activeAllocations.size()
                 << " STAs, triggerId = " << owner->ackTriggerId << "\n";
        return packet;
    }

    void processResponses(FrameSequenceContext *context)
    {
        auto collection = check_and_cast<ReceiveCollectionStep *>(context->getLastStep());
        std::set<MacAddress> responded;
        EV_INFO << "HE DL MU-BAR FS: processing MU-BAR responses, triggerId = " << owner->ackTriggerId << "\n";
        for (auto packet : collection->getReceivedFrames()) {
            auto blockAck = dynamicPtrCast<const Ieee80211BasicBlockAck>(
                    packet->peekAtFront<Ieee80211MacHeader>());
            if (blockAck == nullptr) {
                EV_WARN << "HE DL MU-BAR FS: received non-BlockAck frame in MU-BAR response window\n";
                continue;
            }
            auto expected = std::find_if(owner->activeAllocations.begin(),
                    owner->activeAllocations.end(), [&] (const auto& allocation) {
                        return allocation.staAddress == blockAck->getTransmitterAddress() &&
                                allocation.tid == blockAck->getTidInfo();
                    });
            auto rx = packet->findTag<Ieee80211HeMuRxTag>();
            if (expected == owner->activeAllocations.end() ||
                    rx == nullptr ||
                    rx->getTriggerId() != owner->ackTriggerId ||
                    rx->getRuIndex() != expected->ruIndex ||
                    responded.count(expected->staAddress) != 0) {
                EV_WARN << "HE DL MU-BAR FS: ignoring unexpected BlockAck from "
                        << blockAck->getTransmitterAddress()
                        << " tid=" << (int)blockAck->getTidInfo()
                        << " (matched=" << (expected != owner->activeAllocations.end())
                        << " tag=" << (rx != nullptr)
                        << " triggerId=" << (rx != nullptr ? rx->getTriggerId() : 0)
                        << ")\n";
                continue;
            }
            responded.insert(expected->staAddress);
            EV_INFO << "HE DL MU-BAR FS: accepted BlockAck from " << expected->staAddress << "\n";
            owner->callback->originatorProcessReceivedFrame(packet->dup(), owner->containerPacket);
        }
        for (const auto& allocation : owner->activeAllocations) {
            if (responded.count(allocation.staAddress) != 0)
                continue;
            EV_WARN << "HE DL MU-BAR FS: MU-BAR response timeout for STA "
                    << allocation.staAddress << endl;
            for (auto packet : allocation.packets)
                owner->callback->originatorProcessFailedFrame(packet);
        }
    }

  public:
    explicit HeDlMuBarBlockAckFs(HeDlMuTxOpFs *owner) : owner(owner) { ASSERT(owner != nullptr); }

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override { ASSERT(context != nullptr); step = 0; }

    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override
    {
        ASSERT(context != nullptr);
        if (owner->activeAllocations.empty())
            return nullptr;
        switch (step) {
            case 0:
                return new TransmitStep(buildMuBarTrigger(), owner->modeSet->getSifsTime(), true);
            case 1: {
                auto trigger = check_and_cast<ITransmitStep *>(context->getLastStep())->getFrameToTransmit();
                auto header = trigger->peekAtFront<Ieee80211TriggerFrame>();
                return new ReceiveCollectionStep(owner->modeSet->getSifsTime() +
                        header->getCommonDuration() + owner->modeSet->getSlotTime());
            }
            case 2:
                return nullptr;
            default:
                throw cRuntimeError("Invalid HE MU-BAR frame sequence step");
        }
    }

    virtual bool completeStep(FrameSequenceContext *context) override
    {
        ASSERT(context != nullptr);
        if (step == 1)
            processResponses(context);
        step++;
        return true;
    }

    virtual std::string getHistory() const override { return "MU-BAR HE-TB-BLOCKACK"; }
};

HeDlMuTxOpFs::HeDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                             const IIeee80211HeDlScheduler::ScheduleContext& scheduleContext,
                             Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback,
                             int maxAmpduMpduCount,
                             int maxHeMuPsduLength,
                             simtime_t maxHeMuPpduDuration,
                             AckMethod ackMethod)
    : dlScheduler(dlScheduler),
      scheduleContext(scheduleContext),
      modeSet(modeSet),
      pendingQueue(pendingQueue),
      ackHandler(ackHandler),
      callback(callback),
      maxAmpduMpduCount(maxAmpduMpduCount),
      maxHeMuPsduLength(maxHeMuPsduLength),
      maxHeMuPpduDuration(maxHeMuPpduDuration),
      ackMethod(ackMethod)
{
    ASSERT(dlScheduler != nullptr);
    ASSERT(modeSet != nullptr);
    ASSERT(pendingQueue != nullptr);
    ASSERT(ackHandler != nullptr);
    ASSERT(callback != nullptr);
    static std::atomic<uint32_t> nextTriggerId{1};
    ackTriggerId = nextTriggerId++;
    if (maxAmpduMpduCount <= 0)
        throw cRuntimeError("maxAmpduMpduCount must be positive");
    if (maxHeMuPsduLength <= 0)
        throw cRuntimeError("maxHeMuPsduLength must be positive");
    if (maxHeMuPpduDuration <= SIMTIME_ZERO)
        throw cRuntimeError("maxHeMuPpduDuration must be positive");
    sequence = ackMethod == AckMethod::MU_BAR_TRIGGER ?
            static_cast<IFrameSequence *>(new SequentialFs({new HeDlMuPpduFs(this), new HeDlMuBarBlockAckFs(this)})) :
            static_cast<IFrameSequence *>(new SequentialFs({new HeDlMuPpduFs(this), new HeDlMuSequentialBlockAckFs(this)}));
}

HeDlMuTxOpFs::HeDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                             const std::vector<MacAddress>& candidates,
                             Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback)
    : HeDlMuTxOpFs(dlScheduler, [&] {
          IIeee80211HeDlScheduler::ScheduleContext context;
          for (size_t i = 0; i < candidates.size(); ++i) {
              IIeee80211HeDlScheduler::CandidateInfo candidate;
              candidate.staAddress = candidates[i];
              candidate.anchor = i == 0;
              context.candidates.push_back(candidate);
          }
          if (!candidates.empty())
              context.anchorSta = candidates.front();
          if (modeSet != nullptr && modeSet->getNumModes() > 0) {
              auto firstMode = modeSet->getMode(0);
              context.channelBandwidth = firstMode->getDataMode()->getBandwidth();
              if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(firstMode))
                  context.channelCenterFrequency = heMode->getCenterFrequencyMode() == Ieee80211HeMode::BAND_2_4GHZ ?
                          Hz(2.412e9) : Hz(5.18e9);
          }
          return context;
      }(), modeSet, pendingQueue, ackHandler, callback, 16, 6500631,
      SimTime(5.484, SIMTIME_MS), AckMethod::EXPLICIT_SEQUENTIAL_BAR)
{
}

void HeDlMuTxOpFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    ASSERT(context != nullptr);
    ASSERT(sequence != nullptr);
    this->firstStep = firstStep;
    step = 0;
    sequence->startSequence(context, firstStep);
    EV_INFO << "Starting HE DL MU FS at step " << firstStep << "\n";
}

HeDlMuTxOpFs::~HeDlMuTxOpFs()
{
    delete sequence;
}

Packet *HeDlMuTxOpFs::buildMuContainerPacket(FrameSequenceContext *context)
{
    // IEEE 802.11-2024 Clause 27.3.11.13 ("HE MU PPDU").
    // The AP schedules multiple users simultaneously in downlink by mapping their payloads (A-MPDUs)
    // to separate Resource Units (RUs) or spatial stream groups (DL MU-MIMO).
    // This method collects pending packets from STA queues, validates them, and builds a single
    // HE-MU-PPDU container frame wrapping all user payloads.
    ASSERT(context != nullptr);
    ASSERT(dlScheduler != nullptr);
    activeAllocations.clear();
    auto hcf = dynamic_cast<Hcf *>(callback);
    auto notifyPlanningFailure = [&] {
        if (auto heHcf = dynamic_cast<HeHcf *>(callback)) {
            auto ac = scheduleContext.candidates.empty() ? AccessCategory::AC_BE :
                    scheduleContext.candidates.front().accessCategory;
            heHcf->handleDlMuPlanningFailure(ac);
        }
    };
    struct SelectedAllocation {
        IIeee80211HeDlScheduler::RuAllocation allocation;
        queueing::IPacketQueue *sourceQueue = nullptr;
        Packet *packet = nullptr;
        Ptr<const Ieee80211DataHeader> dataHeader;
        std::vector<Packet *> packets;
        B psduLength = B(0);
        uint16_t associationId = 0;
    };

    // Obtain per-STA RU assignments from the scheduler.
    EV_INFO << "HE DL MU scheduling " << scheduleContext.candidates.size()
             << " candidates, ackMethod = "
             << (ackMethod == AckMethod::MU_BAR_TRIGGER ? "MU-BAR trigger" : "sequential BAR") << "\n";
    if (std::isnan(scheduleContext.channelCenterFrequency.get()) ||
            std::isnan(scheduleContext.channelBandwidth.get())) {
        if (hcf != nullptr)
            throw cRuntimeError("Scheduler context is missing active radio channel geometry");
        if (modeSet != nullptr && modeSet->getNumModes() > 0) {
            auto firstMode = modeSet->getMode(0);
            scheduleContext.channelBandwidth = firstMode->getDataMode()->getBandwidth();
            if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(firstMode))
                scheduleContext.channelCenterFrequency =
                        heMode->getCenterFrequencyMode() == Ieee80211HeMode::BAND_2_4GHZ ?
                        Hz(2.412e9) : Hz(5.18e9);
        }
    }
    if (std::isnan(scheduleContext.channelCenterFrequency.get()) ||
            std::isnan(scheduleContext.channelBandwidth.get()))
        throw cRuntimeError("Scheduler context is missing channel geometry");
    auto allocations = dlScheduler->schedule(scheduleContext);
    EV_INFO << "Scheduler returned " << allocations.size() << " allocations\n";
    if (allocations.empty()) {
        EV_WARN << "Scheduler returned no usable RU allocations; deferring to SU." << endl;
        notifyPlanningFailure();
        return nullptr;
    }
    ASSERT(!allocations.empty());

    // Assemble the HE MU PPDU container packet.
    auto container = new Packet("HE-MU-PPDU");

    // Standard QoS data header — broadcast receiver signals HE MU frame.
    // The container header is not itself a user payload; it carries the NAV
    // duration and allows the packet to traverse the existing MAC/PHY transmit path.
    auto containerHdr = makeShared<Ieee80211DataHeader>();
    containerHdr->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    containerHdr->setType(ST_DATA_WITH_QOS);
    containerHdr->setChunkLength(b(288)); // minimal 802.11 QoS data header size
    if (auto heHcf = dynamic_cast<HeHcf *>(callback)) {
        auto originatorQosDataService = check_and_cast<OriginatorQosMacDataService *>(heHcf->getOriginatorMacDataService());
        ASSERT(originatorQosDataService != nullptr);
        originatorQosDataService->assignSequenceNumber(containerHdr);
    }

    // 1. Calculate the total sequential ACK sequence duration
    simtime_t totalDuration = simtime_t::ZERO;
    auto qosContext = context->getQoSContext();
    auto originatorBAHandler = qosContext != nullptr ? qosContext->blockAckAgreementHandler : nullptr;
    if (originatorBAHandler == nullptr && hcf != nullptr)
        originatorBAHandler = hcf->getOriginatorBlockAckAgreementHandler();
    auto hcfModule = check_and_cast<cModule *>(callback);
    auto rateSelection = check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));
    ASSERT(rateSelection != nullptr);

    // Set the frame mode on the container packet first so response mode calculations don't fail due to missing mode
    auto containerMode = rateSelection->computeMode(container, containerHdr, nullptr);
    RateSelection::setFrameMode(container, containerHdr, containerMode);

    auto warnIneligible = [] (Packet *packet, const MacAddress& receiverAddress, Tid tid, int ruIndex, const char *reason) {
        EV_WARN << "HE DL MU TXOP FS: skipping MU-ineligible packet "
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

    auto getCandidateAccessCategory = [&] (const MacAddress& staAddress, AccessCategory fallbackAc) {
        for (const auto& candidate : scheduleContext.candidates)
            if (candidate.staAddress == staAddress)
                return candidate.accessCategory;
        return fallbackAc;
    };

    auto resolveStaQueue = [&] (const MacAddress& staAddress) {
        for (const auto& candidate : scheduleContext.candidates)
            if (candidate.staAddress == staAddress && candidate.sourceQueue != nullptr)
                return candidate.sourceQueue;
        auto candidateAc = getCandidateAccessCategory(staAddress, AccessCategory::AC_BE);
        if (hcf != nullptr)
            return hcf->resolvePerStaQueue(staAddress, candidateAc);
        return pendingQueue;
    };

    std::vector<Packet *> selectedPackets;
    std::vector<SelectedAllocation> selectedAllocations;
    int skippedAllocations = 0;
    for (size_t idx = 0; idx < allocations.size(); ++idx) {
        const auto& alloc = allocations[idx];
        ASSERT(alloc.ru.index >= 0);
        ASSERT(alloc.ru.toneSize > 0);
        ASSERT(alloc.mcs >= 0 && alloc.mcs <= 11);
        ASSERT(alloc.numberOfSpatialStreams > 0);
        auto sourceQueue = resolveStaQueue(alloc.staAddress);
        if (sourceQueue == nullptr)
            sourceQueue = pendingQueue;
        ASSERT(sourceQueue != nullptr);
        Packet *staPacket = nullptr;
        int n = sourceQueue->getNumPackets();
        for (int i = 0; i < n; ++i) {
            Packet *pkt = sourceQueue->getPacket(i);
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
            skippedAllocations++;
            continue;
        }

        selectedPackets.push_back(staPacket);
        auto macHdr = staPacket->peekAtFront<Ieee80211MacHeader>();
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(macHdr);
        if (dataHeader == nullptr) {
            warnIneligible(staPacket, alloc.staAddress, -1, alloc.ru.index, "packet is not a data frame");
            skippedAllocations++;
            continue;
        }
        if (dataHeader->getType() != ST_DATA_WITH_QOS) {
            warnIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index, "packet is not QoS data");
            skippedAllocations++;
            continue;
        }
        if (dataHeader->getReceiverAddress() != alloc.staAddress) {
            warnIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index, "packet receiver does not match scheduler allocation");
            skippedAllocations++;
            continue;
        }
        if (!hasActiveOriginatorBlockAckAgreement(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid())) {
            warnIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index,
                    getIneligibilityReason(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid()));
            skippedAllocations++;
            continue;
        }

        SelectedAllocation selectedAllocation;
        selectedAllocation.allocation = alloc;
        selectedAllocation.sourceQueue = sourceQueue;
        selectedAllocation.packet = staPacket;
        selectedAllocation.dataHeader = dataHeader;
        if (hcf != nullptr) {
            auto hcfMac = check_and_cast<Ieee80211Mac *>(check_and_cast<cModule *>(hcf)->getParentModule());
            ASSERT(hcfMac != nullptr);
            auto aid = hcfMac->getMib()->getAssociationId(alloc.staAddress);
            if (aid <= 0) {
                warnIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(),
                        alloc.ru.index, "scheduled receiver has no association ID");
                skippedAllocations++;
                continue;
            }
            selectedAllocation.associationId = aid;
        }
        else
            selectedAllocation.associationId = computeHeMuStaId(alloc.staAddress);
        ASSERT(selectedAllocation.associationId != 0);
        selectedAllocations.push_back(selectedAllocation);
    }
    EV_DEBUG << "Building MU container packet: " << selectedAllocations.size()
             << " of " << allocations.size() << " scheduler allocations survived initial validation"
             << " (" << skippedAllocations << " skipped)\n";

    std::map<uint16_t, int> associationIdCounts;
    for (const auto& selectedAllocation : selectedAllocations)
        associationIdCounts[selectedAllocation.associationId]++;
    selectedAllocations.erase(
            std::remove_if(selectedAllocations.begin(), selectedAllocations.end(),
                    [&] (const SelectedAllocation& selectedAllocation) {
                        if (associationIdCounts[selectedAllocation.associationId] <= 1)
                            return false;
                        warnIneligible(selectedAllocation.packet,
                                selectedAllocation.dataHeader->getReceiverAddress(),
                                selectedAllocation.dataHeader->getTid(),
                                selectedAllocation.allocation.ru.index,
                                "association ID collides with another scheduled receiver");
                        return true;
                    }),
            selectedAllocations.end());

    if (selectedAllocations.size() < 2) {
        EV_WARN << "Aborting HE MU PPDU assembly because only "
                << selectedAllocations.size() << " active Block Ack allocations remain before queue mutation." << endl;
        delete container;
        notifyPlanningFailure();
        return nullptr;
    }
    ASSERT(selectedAllocations.size() >= 2);

    auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
    auto responseMode = rateSelection->computeResponseBlockAckFrameMode(container, dummyReq);
    for (size_t idx = 0; idx < selectedAllocations.size(); ++idx) {
        simtime_t responseDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
        simtime_t barDuration = responseMode->getDuration(B(38));
        if (ackMethod == AckMethod::EXPLICIT_SEQUENTIAL_BAR)
            totalDuration += modeSet->getSifsTime() + barDuration +
                    modeSet->getSifsTime() + responseDuration;

        auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(selectedAllocations[idx].dataHeader);
        ASSERT(dataOrMgmtHdr != nullptr);
        auto staMode = rateSelection->computeMode(selectedAllocations[idx].packet, dataOrMgmtHdr, nullptr);
        ASSERT(staMode != nullptr);
        RateSelection::setFrameMode(selectedAllocations[idx].packet, dataOrMgmtHdr, staMode);
    }
    if (ackMethod == AckMethod::MU_BAR_TRIGGER) {
        auto triggerDuration = responseMode->getDuration(B(26 + 12 * selectedAllocations.size()));
        auto responseDuration = estimateHeMuUserDuration(LENGTH_BASIC_BLOCKACK, 26, 0);
        totalDuration = modeSet->getSifsTime() + triggerDuration +
                modeSet->getSifsTime() + responseDuration;
    }

    // Set the totalDuration on the container header to protect the sequential responses
    containerHdr->setDurationField(totalDuration);
    container->insertAtBack(containerHdr);

    simtime_t packingDurationLimit = maxHeMuPpduDuration;
    simtime_t ppduDurationLimit = maxHeMuPpduDuration;
    simtime_t alignedDuration = SIMTIME_ZERO;
    for (const auto& selectedAllocation : selectedAllocations)
        alignedDuration = std::max(alignedDuration, selectedAllocation.allocation.estimatedDuration);
    if (alignedDuration > SIMTIME_ZERO)
        packingDurationLimit = std::min(packingDurationLimit, alignedDuration);

    simtime_t remainingTxop = scheduleContext.txopLimit;
    bool hasTxopLimit = remainingTxop > SIMTIME_ZERO;
    if (qosContext != nullptr && qosContext->txopProcedure != nullptr &&
            qosContext->txopProcedure->getLimit() > SIMTIME_ZERO) {
        auto liveRemainingTxop = std::max(SIMTIME_ZERO,
                qosContext->txopProcedure->getLimit() - qosContext->txopProcedure->getDuration());
        remainingTxop = hasTxopLimit ? std::min(remainingTxop, liveRemainingTxop) : liveRemainingTxop;
        hasTxopLimit = true;
    }
    if (hasTxopLimit) {
        auto txopPpduLimit = std::max(SIMTIME_ZERO, remainingTxop - totalDuration);
        packingDurationLimit = std::min(packingDurationLimit, txopPpduLimit);
        ppduDurationLimit = std::min(ppduDurationLimit, txopPpduLimit);
    }

    std::vector<SelectedAllocation> finalAllocations;
    int rejectedFinalValidation = 0;
    for (auto selectedAllocation : selectedAllocations) {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(selectedAllocation.packet->peekAtFront<Ieee80211MacHeader>());
        if (dataHeader == nullptr || dataHeader->getType() != ST_DATA_WITH_QOS ||
                dataHeader->getReceiverAddress() != selectedAllocation.allocation.staAddress ||
                !hasActiveOriginatorBlockAckAgreement(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid())) {
            Tid tid = dataHeader == nullptr ? -1 : dataHeader->getTid();
            auto receiverAddress = dataHeader == nullptr ? selectedAllocation.allocation.staAddress : dataHeader->getReceiverAddress();
            warnIneligible(selectedAllocation.packet, receiverAddress, tid, selectedAllocation.allocation.ru.index,
                    "failed final validation before queue removal");
            rejectedFinalValidation++;
            continue;
        }

        auto agreement = originatorBAHandler->getAgreement(dataHeader->getReceiverAddress(), dataHeader->getTid());
        int blockAckWindowLimit = agreement == nullptr ? 0 : agreement->getBufferSize();
        int occupiedSlots = ackHandler == nullptr ? 0 :
                ackHandler->getOccupiedBlockAckSequenceNumbers(
                        dataHeader->getReceiverAddress(), dataHeader->getTid()).size();
        int availableSlots = std::max(0, blockAckWindowLimit - occupiedSlots);
        int packetLimit = std::min(maxAmpduMpduCount, availableSlots);
        if (packetLimit <= 0) {
            warnIneligible(selectedAllocation.packet, dataHeader->getReceiverAddress(), dataHeader->getTid(),
                    selectedAllocation.allocation.ru.index, "Block Ack window has no available entries");
            rejectedFinalValidation++;
            continue;
        }

        auto queueForPacking = selectedAllocation.sourceQueue == nullptr ? pendingQueue : selectedAllocation.sourceQueue;
        ASSERT(queueForPacking != nullptr);
        // Aggregate additional eligible MPDUs for this STA up to the Block Ack window,
        // A-MPDU MPDU count, PSDU length, and aligned-duration limits.
        for (int i = 0; i < queueForPacking->getNumPackets() &&
                (int)selectedAllocation.packets.size() < packetLimit; ++i) {
            Packet *candidatePacket = queueForPacking->getPacket(i);
            auto candidateHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    candidatePacket->peekAtFront<Ieee80211MacHeader>());
            if (candidateHeader == nullptr || candidateHeader->getType() != ST_DATA_WITH_QOS ||
                    candidateHeader->getReceiverAddress() != selectedAllocation.allocation.staAddress ||
                    candidateHeader->getTid() != dataHeader->getTid() ||
                    !hasActiveOriginatorBlockAckAgreement(originatorBAHandler,
                            candidateHeader->getReceiverAddress(), candidateHeader->getTid()))
                continue;

            auto proposedPackets = selectedAllocation.packets;
            proposedPackets.push_back(candidatePacket);
            B proposedLength = calculateAmpduPsduLength(proposedPackets);
            if (proposedLength.get<B>() > maxHeMuPsduLength)
                break;
            if (estimateHeMuUserDuration(proposedLength, selectedAllocation.allocation.ru.toneSize,
                    selectedAllocation.allocation.mcs,
                    selectedAllocation.allocation.numberOfSpatialStreams,
                    selectedAllocation.allocation.dcm,
                    scheduleContext.guardInterval) > packingDurationLimit)
                break;
            selectedAllocation.packets = proposedPackets;
            selectedAllocation.psduLength = proposedLength;
        }
        if (selectedAllocation.packets.empty()) {
            warnIneligible(selectedAllocation.packet, dataHeader->getReceiverAddress(), dataHeader->getTid(),
                    selectedAllocation.allocation.ru.index,
                    "HoL MPDU exceeds aligned, TXOP, or HE PPDU packing limit");
            rejectedFinalValidation++;
            continue;
        }
        finalAllocations.push_back(selectedAllocation);
    }
    EV_DEBUG << "Building MU container packet: " << finalAllocations.size()
             << " allocations survived final validation (" << rejectedFinalValidation << " rejected)\n";

    if (finalAllocations.size() < 2) {
        EV_WARN << "Aborting HE MU PPDU assembly because only "
                << finalAllocations.size() << " active Block Ack allocations remain after final validation." << endl;
        delete container;
        notifyPlanningFailure();
        return nullptr;
    }

    auto calculatePlannedPpdu = [&] {
        std::vector<Ieee80211HeUserPhyParameters> users;
        std::map<int, int> ruStreamStartIndex;
        for (const auto& selectedAllocation : finalAllocations) {
            Ieee80211HeUserPhyParameters user;
            user.ru = selectedAllocation.allocation.ru;
            if (user.ru.toneSize <= 0) {
                // Defensive fallback: a zero-sized RU should never reach this point.
                EV_WARN << "Building MU container packet: zero RU tone size for "
                        << selectedAllocation.allocation.staAddress << ", falling back to 26-tone RU\n";
                user.ru.toneSize = 26;
                user.ru.dataSubcarriers = getHeRuDataSubcarrierCount(26);
                user.ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(26);
                user.ru.bandwidth = Hz(26 * 78125.0);
            }
            user.mcs = selectedAllocation.allocation.mcs;
            user.numberOfSpatialStreams = selectedAllocation.allocation.numberOfSpatialStreams;
            user.dcm = selectedAllocation.allocation.dcm;
            user.coding = scheduleContext.coding;
            user.psduLength = selectedAllocation.psduLength;
            user.staId = selectedAllocation.associationId;
            user.streamStartIndex = ruStreamStartIndex[user.ru.index];
            ruStreamStartIndex[user.ru.index] += user.numberOfSpatialStreams;
            users.push_back(user);
        }
        return computeHePpduParameters(users, scheduleContext.channelBandwidth,
                HE_MU_DOWNLINK, scheduleContext.guardInterval);
    };
    auto plannedPpdu = calculatePlannedPpdu();
    if (!plannedPpdu)
        EV_WARN << "HeDlMuTxOpFs::buildMuContainerPacket: computeHePpduParameters failed initially\n";
    int durationTrimIterations = 0;
    while ((!plannedPpdu || plannedPpdu.parameters.duration > ppduDurationLimit) &&
            finalAllocations.size() >= 2) {
        auto longest = std::max_element(finalAllocations.begin(), finalAllocations.end(),
                [] (const SelectedAllocation& a, const SelectedAllocation& b) {
                    return a.psduLength < b.psduLength;
                });
        ASSERT(longest != finalAllocations.end());
        if (longest->packets.size() > 1) {
            EV_DEBUG << "Building MU container packet: trimming last MPDU from "
                     << longest->allocation.staAddress << " to fit duration limit\n";
            longest->packets.pop_back();
            longest->psduLength = calculateAmpduPsduLength(longest->packets);
        }
        else {
            EV_DEBUG << "Building MU container packet: dropping " << longest->allocation.staAddress
                     << " to fit duration limit\n";
            finalAllocations.erase(longest);
        }
        if (finalAllocations.size() >= 2)
            plannedPpdu = calculatePlannedPpdu();
        durationTrimIterations++;
    }
    EV_DEBUG << "Building MU container packet: duration trim took " << durationTrimIterations
             << " iteration(s), final allocations = " << finalAllocations.size() << "\n";
    if (finalAllocations.size() < 2 || !plannedPpdu ||
            plannedPpdu.parameters.duration > ppduDurationLimit) {
        EV_WARN << "Building MU container packet: no complete HE MU PPDU fits the PHY/TXOP duration limit." << endl;
        delete container;
        notifyPlanningFailure();
        return nullptr;
    }
    ASSERT(plannedPpdu);
    totalDuration = SIMTIME_ZERO;
    auto finalBarDuration = responseMode->getDuration(B(38));
    auto finalBlockAckDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
    if (ackMethod == AckMethod::EXPLICIT_SEQUENTIAL_BAR) {
        for (size_t i = 0; i < finalAllocations.size(); ++i)
            totalDuration += modeSet->getSifsTime() + finalBarDuration +
                    modeSet->getSifsTime() + finalBlockAckDuration;
    }
    else {
        auto triggerDuration = responseMode->getDuration(B(26 + 12 * finalAllocations.size()));
        auto responseDuration = estimateHeMuUserDuration(LENGTH_BASIC_BLOCKACK, 26, 0);
        totalDuration = modeSet->getSifsTime() + triggerDuration +
                modeSet->getSifsTime() + responseDuration;
    }
    auto finalContainerHdr = container->removeAtFront<Ieee80211DataHeader>();
    finalContainerHdr->setDurationField(totalDuration);
    container->insertAtFront(finalContainerHdr);

    std::map<int, int> ruTotalNsts;
    std::map<int, int> ruUserCount;
    for (const auto& selectedAllocation : finalAllocations) {
        ASSERT(selectedAllocation.allocation.ru.index >= 0);
        ruTotalNsts[selectedAllocation.allocation.ru.index] += selectedAllocation.allocation.numberOfSpatialStreams;
        ruUserCount[selectedAllocation.allocation.ru.index]++;
    }
    std::map<int, int> ruStreamStartIndex;

    // 2. Build the final MU container packet and assign duration/sequence numbers to sub-packets.
    // Each selected STA becomes one RU payload section with per-user PHY parameters.
    for (const auto& selectedAllocation : finalAllocations) {
        const auto& alloc = selectedAllocation.allocation;
        Packet *firstPacket = selectedAllocation.packet;
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(firstPacket->peekAtFront<Ieee80211MacHeader>());
        ASSERT(dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS);
        ASSERT(hasActiveOriginatorBlockAckAgreement(originatorBAHandler,
                dataHeader->getReceiverAddress(), dataHeader->getTid()));

        const auto& staPackets = selectedAllocation.packets;

        for (auto staPacket : staPackets) {
            auto dequeueQueue = selectedAllocation.sourceQueue == nullptr ? pendingQueue : selectedAllocation.sourceQueue;
            dequeueQueue->removePacket(staPacket);
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
            context->getInProgressFrames()->addInProgressFrame(staPacket);
        }

        int ruIdx = alloc.ru.index;
        int startIndex = ruStreamStartIndex[ruIdx];
        ruStreamStartIndex[ruIdx] += alloc.numberOfSpatialStreams;
        bool isRuMuMimo = ruUserCount[ruIdx] > 1;

        auto payloadHeader = makeShared<Ieee80211HeMuRuPayloadHeader>();
        payloadHeader->setRuIndex(alloc.ru.index);
        payloadHeader->setRuToneSize(alloc.ru.toneSize);
        payloadHeader->setRuToneOffset(alloc.ru.toneOffset);
        payloadHeader->setStaId(selectedAllocation.associationId);
        payloadHeader->setMcs(alloc.mcs);
        payloadHeader->setNumberOfSpatialStreams(alloc.numberOfSpatialStreams);
        payloadHeader->setDcm(alloc.dcm);
        payloadHeader->setMpduLength(selectedAllocation.psduLength);
        payloadHeader->setStreamStartIndex(startIndex);
        payloadHeader->setMuMimo(isRuMuMimo);
        if (isRuMuMimo) {
            payloadHeader->setTotalNsts(ruTotalNsts[ruIdx]);
        }
        container->insertAtBack(payloadHeader);
        for (size_t i = 0; i < staPackets.size(); ++i) {
            auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
            delimiter->setLength(staPackets[i]->getByteLength());
            container->insertAtBack(delimiter);
            container->insertAtBack(staPackets[i]->peekAll());
            int padding = (4 - (B(4) + B(staPackets[i]->getByteLength())).get<B>() % 4) % 4;
            if (i + 1 != staPackets.size() && padding != 0)
                container->insertAtBack(makeShared<ByteCountChunk>(B(padding)));
        }

        ActiveAllocation activeAlloc;
        activeAlloc.staAddress = alloc.staAddress;
        activeAlloc.associationId = selectedAllocation.associationId;
        activeAlloc.tid = dataHeader->getTid();
        activeAlloc.ruIndex = alloc.ru.index;
        activeAlloc.ru = alloc.ru;
        activeAlloc.packet = staPackets.front();
        activeAlloc.packets = staPackets;
        activeAllocations.push_back(activeAlloc);
        EV_DEBUG << "HeDlMuTxOpFs::buildMuContainerPacket: added RU payload for " << alloc.staAddress
                 << " AID=" << selectedAllocation.associationId
                 << " RU=" << alloc.ru.index
                 << " tones=" << alloc.ru.toneSize
                 << " mcs=" << alloc.mcs
                 << " packets=" << staPackets.size() << "\n";
    }

    container->insertAtBack(makeShared<Ieee80211MacTrailer>());
    auto commonRequest = container->addTagIfAbsent<Ieee80211HeMuCommonReq>();
    commonRequest->setGuardInterval(scheduleContext.guardInterval);
    commonRequest->setCoding(scheduleContext.coding);
    commonRequest->setPacketExtensionDurationUs(scheduleContext.packetExtensionDurationUs);
    commonRequest->setPuncturedSubchannelMask(scheduleContext.puncturedSubchannelMask);

    ASSERT(activeAllocations.size() >= 2);

    EV_INFO << "Assembled HE MU PPDU with "
            << activeAllocations.size() << " RU allocations. Total sequential duration = " << totalDuration << endl;
    return container;
}

IFrameSequenceStep *HeDlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    ASSERT(sequence != nullptr);
    return sequence->prepareStep(context);
}

bool HeDlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    ASSERT(context != nullptr);
    ASSERT(sequence != nullptr);
    step++;
    return sequence->completeStep(context);
}

std::string HeDlMuTxOpFs::getHistory() const
{
    return step == -1 ? "HE-DL-MU" : sequence->getHistory();
}

} // namespace ieee80211
} // namespace inet
