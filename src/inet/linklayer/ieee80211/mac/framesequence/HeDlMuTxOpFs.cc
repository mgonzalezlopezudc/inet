//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

#include <algorithm>
#include <atomic>
#include <map>

// HE DL MU TXOP frame sequence.
//
// Builds and transmits an HE MU PPDU carrying downlink A-MPDUs for multiple
// STAs, then collects acknowledgments.  Relevant clauses:
//   - IEEE 802.11-2024 26.5.1: HE DL MU operation.
//   - 26.6.2/26.6.3: HE A-MPDU padding and multi-TID aggregation rules.
//   - 26.4.2/26.4.4: Multi-STA BA context and HE PPDU response rules.
//   - 27.3.11.13: HE MU PPDU format.
//   - 9.3.1.7/9.3.1.8/9.3.1.22: BAR, BlockAck, and Trigger frame formats.
//
// Implementation notes:
//   - The HE MU PPDU is represented as a single container Packet with a
//     broadcast QoS data header so that it can reuse the existing MAC/PHY
//     transmit path.  The container header is not a user payload.
//   - Two acknowledgment modes are supported:
//       * MU-BAR trigger: the AP sends a Trigger frame that solicits a
//         Multi-STA BlockAck in an HE TB PPDU (Clause 26.5.2).
//       * Sequential BAR: the AP sends individual BlockAckReq frames and
//         receives per-STA BlockAcks sequentially.  This is valid but less
//         efficient than the MU-BAR method.
//   - Multi-TID aggregation is advertised in capabilities and used when
//     building BARs, but the per-user PSDU packing inside the HE MU PPDU is
//     currently single-TID.

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mac/framesequence/GenericFrameSequences.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuPackingPlanner.h"
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
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

namespace {

std::map<Tid, SequenceNumberCyclic> collectStartingSequenceNumbersByTid(const std::vector<Packet *>& packets)
{
    std::map<Tid, SequenceNumberCyclic> records;
    for (auto packet : packets) {
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr)
            continue;
        auto it = records.find(header->getTid());
        if (it == records.end() || header->getSequenceNumber().get() < it->second.get())
            records[header->getTid()] = header->getSequenceNumber();
    }
    return records;
}

void warnDlMuIneligible(Packet *packet, const MacAddress& receiverAddress, Tid tid, int ruIndex, const char *reason)
{
    EV_WARN << "HE DL MU TXOP FS: skipping MU-ineligible packet "
            << (packet == nullptr ? "<none>" : packet->getName())
            << " for receiver " << receiverAddress
            << ", TID " << (int)tid
            << ", RU " << ruIndex
            << ": " << reason << endl;
}

const char *getDlMuIneligibilityReason(IOriginatorBlockAckAgreementHandler *handler, const MacAddress& receiverAddress, Tid tid)
{
    if (handler == nullptr)
        return "null originator Block Ack agreement handler";
    auto agreement = handler->getAgreement(receiverAddress, tid);
    if (agreement == nullptr)
        return "missing originator Block Ack agreement";
    if (!agreement->getIsAddbaResponseReceived())
        return "ADDBA response not received";
    return nullptr;
}

B getMuBarTriggerHeaderLength(size_t numberOfUsers)
{
    return B(24 + 9 * numberOfUsers);
}

B getMuBarTriggerFrameLength(size_t numberOfUsers)
{
    return getMuBarTriggerHeaderLength(numberOfUsers) + B(4);
}

simtime_t estimateTriggeredBlockAckDuration(int ruToneSize,
        Ieee80211HeGuardInterval guardInterval, Ieee80211HeCoding coding)
{
    Ieee80211HeRu ru;
    ru.toneSize = std::max(ruToneSize, 26);
    ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
    ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
    ru.bandwidth = Hz(ru.toneSize * 78125.0);
    return computeHeUserPhyParameters(LENGTH_BASIC_BLOCKACK, ru, 0, 1, false,
            guardInterval, coding).duration;
}

} // namespace

class HeDlMuPerStaBlockAckFs : public SequentialFs
{
  protected:
    HeDlMuTxOpFs *owner = nullptr;
    int allocationIndex = -1;

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
        // 9.3.1.7 defines Basic and Multi-TID BlockAckReq variants.  For
        // sequential BAR acknowledgement, each selected STA is polled with a
        // BAR whose starting sequence number matches the first MPDU sent to
        // that STA in the HE MU PPDU.
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
            // 26.6.3 allows DL HE MU multi-TID A-MPDUs when negotiated.  The
            // matching Multi-TID BAR carries one record per TID included in the
            // per-user A-MPDU.
            auto recordsByTid = collectStartingSequenceNumbersByTid(getActiveAllocation().packets);
            if (recordsByTid.empty())
                recordsByTid[tid] = startingSequenceNumber;
            auto multiTidReq = makeShared<Ieee80211MultiTidBlockAckReq>();
            multiTidReq->setReceiverAddress(receiverAddress);
            multiTidReq->setTransmitterAddress(macModule->getAddress());
            multiTidReq->setRecordsArraySize(recordsByTid.size());
            unsigned int index = 0;
            for (const auto& entry : recordsByTid) {
                Ieee80211MultiTidBlockAckReqRecord rec;
                rec.tid = entry.first;
                rec.startingSequenceNumber = entry.second.get();
                multiTidReq->setRecords(index++, rec);
            }
            multiTidReq->setChunkLength(B(18 + 4 * recordsByTid.size()));
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
        // 9.2.5 Duration fields reserve the remaining exchange.  For the
        // sequential BAR method, each BAR protects its own BlockAck response
        // plus any later BAR/BlockAck pairs in this TXOP.
        auto hcfModule = dynamic_cast<cModule *>(owner->callback);
        auto macModule = hcfModule != nullptr ? dynamic_cast<Ieee80211Mac *>(hcfModule->getParentModule()) : nullptr;
        auto mib = macModule != nullptr ? macModule->getMib() : nullptr;
        auto negotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(getActiveAllocation().staAddress) : nullptr;
        bool multiTid = (negotiated != nullptr && negotiated->intersection.multiTidAggregationTx);

        auto activeRecordCount = multiTid ? std::max<size_t>(1, collectStartingSequenceNumbersByTid(getActiveAllocation().packets).size()) : 0;
        auto blockAckDuration = responseMode->getDuration(multiTid ? b(B(18 + 12 * activeRecordCount)) : LENGTH_BASIC_BLOCKACK);
        auto barDuration = responseMode->getDuration(multiTid ? B(18 + 4 * activeRecordCount) : B(38));
        auto remainingDuration = owner->modeSet->getSifsTime() + blockAckDuration;
        for (int nextIndex = allocationIndex + 1; nextIndex < (int)owner->activeAllocations.size(); nextIndex++) {
            auto nextNegotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(owner->activeAllocations.at(nextIndex).staAddress) : nullptr;
            bool nextMultiTid = (nextNegotiated != nullptr && nextNegotiated->intersection.multiTidAggregationTx);
            auto nextRecordCount = nextMultiTid ? std::max<size_t>(1, collectStartingSequenceNumbersByTid(owner->activeAllocations.at(nextIndex).packets).size()) : 0;
            auto nextBlockAckDuration = responseMode->getDuration(nextMultiTid ? b(B(18 + 12 * nextRecordCount)) : LENGTH_BASIC_BLOCKACK);
            auto nextBarDuration = responseMode->getDuration(nextMultiTid ? B(18 + 4 * nextRecordCount) : B(38));
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
            multiTidReq->setRecordsArraySize(std::max<size_t>(1, collectStartingSequenceNumbersByTid(getActiveAllocation().packets).size()));
            dummyReq = multiTidReq;
        } else {
            dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
        }

        auto responseMode = getRateSelection()->computeResponseBlockAckFrameMode(lastTransmittedPacket, dummyReq);
        auto recordCount = multiTid ? std::max<size_t>(1, collectStartingSequenceNumbersByTid(getActiveAllocation().packets).size()) : 0;
        return owner->modeSet->getSifsTime() + responseMode->getDuration(multiTid ? b(B(18 + 12 * recordCount)) : LENGTH_BASIC_BLOCKACK) + owner->modeSet->getSlotTime();
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
        auto receiveStep = check_and_cast<IReceiveStep *>(context->getLastStep());
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
        return true;
    }

  public:
    HeDlMuPerStaBlockAckFs(HeDlMuTxOpFs *owner, int allocationIndex) :
        SequentialFs({new StepFs("BlockAckReq",
                                 [this](StepFs *, FrameSequenceContext *context) {
                                     return prepareBarStep(context);
                                 }),
                      new StepFs("BlockAck",
                                 [this](StepFs *, FrameSequenceContext *context) {
                                     return prepareBlockAckStep(context);
                                 },
                                 [this](StepFs *, FrameSequenceContext *context) {
                                     return completeBlockAckStep(context);
                                 })}),
        owner(owner),
        allocationIndex(allocationIndex)
    {
        ASSERT(owner != nullptr);
    }
};

class HeDlMuBarBlockAckFs : public OptionalFs
{
  protected:
    HeDlMuTxOpFs *owner = nullptr;

    Packet *buildMuBarTrigger() const
    {
        // 9.3.1.22.4: MU-BAR is Trigger type 2 and carries BAR Control and BAR
        // Information in each Trigger User Info field.  The Trigger solicits
        // simultaneous BlockAck responses as HE TB PPDUs.
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
        header->setGuardInterval(owner->scheduleContext.guardInterval);
        header->setCoding(owner->scheduleContext.coding);
        header->setPacketExtensionDurationUs(owner->scheduleContext.packetExtensionDurationUs);
        header->setPuncturedSubchannelMask(owner->scheduleContext.puncturedSubchannelMask);
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
            SequenceNumberCyclic startingSequenceNumber;
            auto tid = allocation.tid;
            if (allocation.packet != nullptr) {
                auto macHeader = allocation.packet->peekAtFront<Ieee80211MacHeader>();
                if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(macHeader)) {
                    tid = dataHeader->getTid();
                    startingSequenceNumber = dataHeader->getSequenceNumber();
                }
            }
            else {
                auto recordsByTid = collectStartingSequenceNumbersByTid(allocation.packets);
                auto it = recordsByTid.find(tid);
                if (it == recordsByTid.end())
                    it = recordsByTid.begin();
                if (it != recordsByTid.end()) {
                    tid = it->first;
                    startingSequenceNumber = it->second;
                }
            }
            user.tid = tid;
            // 26.4.2 requires Ack Type 0 for Multi-STA BlockAck responses to
            // MU-BAR.  This model solicits compressed single-TID BlockAcks per
            // user, so the BAR-dependent fields mirror a Compressed BAR.
            user.muBarBarAckPolicy = false;
            user.muBarMultiTid = false;
            user.muBarCompressedBitmap = true;
            user.muBarTidInfo = tid;
            user.muBarFragmentNumber = 0;
            user.muBarStartingSequenceNumber = startingSequenceNumber.get();
            header->setUsers(i, user);
            commonDuration = std::max(commonDuration,
                    estimateTriggeredBlockAckDuration(allocation.ru.toneSize,
                            owner->scheduleContext.guardInterval, owner->scheduleContext.coding));
        }
        header->setCommonDuration(commonDuration);
        header->setDurationField(owner->modeSet->getSifsTime() + commonDuration);
        header->setChunkLength(getMuBarTriggerHeaderLength(owner->activeAllocations.size()));
        auto packet = new Packet("HE-MU-BAR-Trigger", header);
        packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
        EV_INFO << "HE DL MU-BAR FS: built MU-BAR trigger for " << owner->activeAllocations.size()
                 << " STAs, triggerId = " << owner->ackTriggerId << "\n";
        return packet;
    }

    void processResponses(FrameSequenceContext *context)
    {
        // 26.5.2.3.3 ties an HE TB response to the triggering RU and Trigger
        // fields.  We accept only BlockAck frames whose simulated HE TB tag
        // matches this MU-BAR Trigger and the expected RU allocation.
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
            auto blockAckPacket = packet->dup();
            owner->callback->originatorProcessReceivedFrame(blockAckPacket, owner->containerPacket);
            delete blockAckPacket;
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
    explicit HeDlMuBarBlockAckFs(HeDlMuTxOpFs *owner) :
        OptionalFs(new SequentialFs({new StepFs("MU-BAR Trigger",
                                               [this](StepFs *, FrameSequenceContext *context) {
                                                   return new TransmitStep(buildMuBarTrigger(), this->owner->modeSet->getSifsTime(), true);
                                               }),
                                    new StepFs("HE-TB-BlockAck",
                                               [this](StepFs *, FrameSequenceContext *context) {
                                                   auto trigger = check_and_cast<ITransmitStep *>(context->getLastStep())->getFrameToTransmit();
                                                   auto header = trigger->peekAtFront<Ieee80211TriggerFrame>();
                                                   return new ReceiveCollectionStep(this->owner->modeSet->getSifsTime() +
                                                           header->getCommonDuration() + this->owner->modeSet->getSlotTime());
                                               },
                                               [this](StepFs *, FrameSequenceContext *context) {
                                                   processResponses(context);
                                                   return true;
                                               })}),
                   [this](OptionalFs *, FrameSequenceContext *) {
                       return !this->owner->activeAllocations.empty();
                   }),
        owner(owner)
    {
        ASSERT(owner != nullptr);
    }
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
      ackMethod(ackMethod),
      // G.5 HE sequences
      // he-txop-sequence =
      //   he-nav-protected-sequence |
      //   1{initiator-sequence};
      // he-dl-mu-sequence =
      //   ( BlockAck + delayed [+mu-users-respond] Ack |
      //   ( BlockAckReq + delayed [+mu-users-respond] Ack ) |
      //   ( Data[+HTC] + individual [+null] [+QoS+normal-ack] [+mu-user-respond] Ack |
      //   Ack );
      // he-ul-mu-sequence =
      //   MU-BAR Trigger BlockAck;
      // Implemented subset:
      //   HE-MU-PPDU ( MU-BAR Trigger BlockAck | 1{BlockAckReq BlockAck} );
      // The HE MU PPDU itself is built by the first StepFs; the acknowledgement
      // tail is either the standard MU-BAR trigger exchange or a sequential
      // BlockAckReq/BlockAck fallback for each selected STA.
      sequence(new SequentialFs({new StepFs("HE-MU-PPDU",
                                            [this](StepFs *, FrameSequenceContext *context) -> IFrameSequenceStep * {
                                                containerPacket = buildMuContainerPacket(context);
                                                if (containerPacket == nullptr) {
                                                    EV_WARN << "HeDlMuTxOpFs: container packet build failed, aborting HE DL MU sequence\n";
                                                    return static_cast<IFrameSequenceStep *>(nullptr);
                                                }
                                                EV_DEBUG << "HeDlMuTxOpFs: transmitting HE DL MU container packet\n";
                                                return new TransmitStep(containerPacket, context->getIfs(), true);
                                            }),
                                 new AlternativesFs({new HeDlMuBarBlockAckFs(this),
                                                     new IndexedRepeatingFs(
                                                             [this](IndexedRepeatingFs *, FrameSequenceContext *, int index) {
                                                                 return new HeDlMuPerStaBlockAckFs(this, index);
                                                             },
                                                             [this](IndexedRepeatingFs *, FrameSequenceContext *, int index) {
                                                                 return index < (int)activeAllocations.size();
                                                             })},
                                                     [this](AlternativesFs *, FrameSequenceContext *) {
                                                         return this->ackMethod == AckMethod::MU_BAR_TRIGGER ? 0 : 1;
                                                     })}))
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

HeDlMuTxOpFs::~HeDlMuTxOpFs() = default;

Packet *HeDlMuTxOpFs::buildMuContainerPacket(FrameSequenceContext *context)
{
    // IEEE 802.11-2024 26.5.1 / 27.3.11.13: an AP schedules one or more
    // per-user PSDUs in an HE MU PPDU using OFDMA RUs and, optionally,
    // MU-MIMO spatial streams.  This method builds INET's container Packet for
    // that PPDU and annotates each user payload with the corresponding RU PHY
    // parameters.
    ASSERT(context != nullptr);
    ASSERT(dlScheduler != nullptr);
    activeAllocations.clear();
    auto hcf = dynamic_cast<Hcf *>(callback);
    auto hcfMac = hcf != nullptr ? dynamic_cast<Ieee80211Mac *>(check_and_cast<cModule *>(hcf)->getParentModule()) : nullptr;
    auto notifyPlanningFailure = [&] {
        if (auto heHcf = dynamic_cast<HeHcf *>(callback)) {
            auto ac = scheduleContext.candidates.empty() ? AccessCategory::AC_BE :
                    scheduleContext.candidates.front().accessCategory;
            heHcf->handleDlMuPlanningFailure(ac);
        }
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

    // Standard-like QoS data header for the model container.  The real HE MU
    // PPDU has per-user PSDUs selected by TXVECTOR STA_ID/RU allocation
    // (26.5.1.2); this broadcast header is not a user payload.
    auto containerHdr = makeShared<Ieee80211DataHeader>();
    containerHdr->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    containerHdr->setType(ST_DATA_WITH_QOS);
    containerHdr->setChunkLength(b(288)); // minimal 802.11 QoS data header size
    if (auto heHcf = dynamic_cast<HeHcf *>(callback)) {
        auto originatorQosDataService = check_and_cast<OriginatorQosMacDataService *>(heHcf->getOriginatorMacDataService());
        ASSERT(originatorQosDataService != nullptr);
        originatorQosDataService->assignSequenceNumber(containerHdr);
    }

    // Calculate the NAV-protecting Duration field.  9.2.5 and 26.4.3 require
    // BAR/MU-BAR originators to account for the expected BlockAck response; the
    // value here covers either sequential BAR/BA pairs or the MU-BAR HE TB BA.
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
    std::vector<HeDlMuPackingPlanner::SelectedAllocation> selectedAllocations;
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
            warnDlMuIneligible(nullptr, alloc.staAddress, -1, alloc.ru.index, "no queued packet for scheduled receiver");
            skippedAllocations++;
            continue;
        }

        selectedPackets.push_back(staPacket);
        auto macHdr = staPacket->peekAtFront<Ieee80211MacHeader>();
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(macHdr);
        if (dataHeader == nullptr) {
            warnDlMuIneligible(staPacket, alloc.staAddress, -1, alloc.ru.index, "packet is not a data frame");
            skippedAllocations++;
            continue;
        }
        if (dataHeader->getType() != ST_DATA_WITH_QOS) {
            warnDlMuIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index, "packet is not QoS data");
            skippedAllocations++;
            continue;
        }
        if (dataHeader->getReceiverAddress() != alloc.staAddress) {
            warnDlMuIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index, "packet receiver does not match scheduler allocation");
            skippedAllocations++;
            continue;
        }
        if (!hasActiveOriginatorBlockAckAgreement(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid())) {
            warnDlMuIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(), alloc.ru.index,
                    getDlMuIneligibilityReason(originatorBAHandler, dataHeader->getReceiverAddress(), dataHeader->getTid()));
            skippedAllocations++;
            continue;
        }

        HeDlMuPackingPlanner::SelectedAllocation selectedAllocation;
        selectedAllocation.allocation = alloc;
        selectedAllocation.sourceQueue = sourceQueue;
        selectedAllocation.packet = staPacket;
        selectedAllocation.dataHeader = dataHeader;
        auto hcfMacForCapabilities = hcf != nullptr ? dynamic_cast<Ieee80211Mac *>(check_and_cast<cModule *>(hcf)->getParentModule()) : nullptr;
        auto negotiated = hcfMacForCapabilities != nullptr ? hcfMacForCapabilities->getMib()->findNegotiatedHeCapabilities(alloc.staAddress) : nullptr;
        selectedAllocation.multiTidAggregation = negotiated != nullptr &&
                negotiated->valid && negotiated->intersection.multiTidAggregationTx;
        if (hcf != nullptr) {
            auto hcfMac = check_and_cast<Ieee80211Mac *>(check_and_cast<cModule *>(hcf)->getParentModule());
            ASSERT(hcfMac != nullptr);
            auto aid = hcfMac->getMib()->getAssociationId(alloc.staAddress);
            if (aid <= 0) {
                warnDlMuIneligible(staPacket, dataHeader->getReceiverAddress(), dataHeader->getTid(),
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
        auto triggerDuration = responseMode->getDuration(getMuBarTriggerFrameLength(selectedAllocations.size()));
        simtime_t responseDuration = SIMTIME_ZERO;
        for (const auto& selectedAllocation : selectedAllocations)
            responseDuration = std::max(responseDuration,
                    estimateTriggeredBlockAckDuration(selectedAllocation.allocation.ru.toneSize,
                            scheduleContext.guardInterval, scheduleContext.coding));
        totalDuration = modeSet->getSifsTime() + triggerDuration +
                modeSet->getSifsTime() + responseDuration;
    }

    // Set the container Duration field before queue mutation so receivers that
    // can decode only the MAC header still see NAV protection for the pending
    // acknowledgement phase.
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
        if (remainingTxop <= totalDuration) {
            EV_WARN << "Building MU container packet: reserved MU acknowledgment phase ("
                    << totalDuration << ") leaves no payload time in the remaining TXOP ("
                    << remainingTxop << ")" << endl;
            delete container;
            notifyPlanningFailure();
            return nullptr;
        }
        auto txopPpduLimit = std::max(SIMTIME_ZERO, remainingTxop - totalDuration);
        packingDurationLimit = std::min(packingDurationLimit, txopPpduLimit);
        ppduDurationLimit = std::min(ppduDurationLimit, txopPpduLimit);
    }

    auto getAvailableSlots = [&] (const MacAddress& receiverAddress, Tid tid) {
        auto agreement = originatorBAHandler->getAgreement(receiverAddress, tid);
        int blockAckWindowLimit = agreement == nullptr ? 0 : agreement->getBufferSize();
        int occupiedSlots = ackHandler == nullptr ? 0 :
                ackHandler->getOccupiedBlockAckSequenceNumbers(receiverAddress, tid).size();
        return std::max(0, blockAckWindowLimit - occupiedSlots);
    };

    HeDlMuPackingPlanner::Parameters packingParameters;
    packingParameters.selectedAllocations = selectedAllocations;
    packingParameters.pendingQueue = pendingQueue;
    packingParameters.scheduleContext = scheduleContext;
    packingParameters.maxAmpduMpduCount = maxAmpduMpduCount;
    packingParameters.maxHeMuPsduLength = maxHeMuPsduLength;
    packingParameters.packingDurationLimit = packingDurationLimit;
    packingParameters.ppduDurationLimit = ppduDurationLimit;
    packingParameters.hasActiveBlockAckAgreement = [&] (const MacAddress& receiverAddress, Tid tid) {
        return hasActiveOriginatorBlockAckAgreement(originatorBAHandler, receiverAddress, tid);
    };
    packingParameters.getAvailableBlockAckSlots = getAvailableSlots;
    packingParameters.warnIneligible = warnDlMuIneligible;

    HeDlMuPackingPlanner packingPlanner;
    auto packingPlan = packingPlanner.plan(packingParameters);
    const auto& finalAllocations = packingPlan.allocations;
    EV_DEBUG << "Building MU container packet: " << finalAllocations.size()
             << " allocations survived final validation (" << packingPlan.rejectedFinalValidation << " rejected)\n";
    EV_DEBUG << "Building MU container packet: duration trim took " << packingPlan.durationTrimIterations
             << " iteration(s), final allocations = " << finalAllocations.size() << "\n";
    if (!packingPlan) {
        EV_WARN << "Building MU container packet: " << packingPlan.failureReason << endl;
        delete container;
        notifyPlanningFailure();
        return nullptr;
    }
    const auto& plannedPpdu = packingPlan.ppdu;
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
        auto triggerDuration = responseMode->getDuration(getMuBarTriggerFrameLength(finalAllocations.size()));
        simtime_t responseDuration = SIMTIME_ZERO;
        for (const auto& finalAllocation : finalAllocations)
            responseDuration = std::max(responseDuration,
                    estimateTriggeredBlockAckDuration(finalAllocation.allocation.ru.toneSize,
                            scheduleContext.guardInterval, scheduleContext.coding));
        totalDuration = modeSet->getSifsTime() + triggerDuration +
                modeSet->getSifsTime() + responseDuration;
    }
    auto finalContainerHdr = container->removeAtFront<Ieee80211DataHeader>();
    finalContainerHdr->setDurationField(totalDuration);
    container->insertAtFront(finalContainerHdr);

    // Build the final MU container packet and assign duration/sequence numbers.
    // Each selected STA becomes one per-user PSDU section.  Multiple users on
    // the same RU are represented as DL MU-MIMO with stream-start indices as in
    // the HE MU TXVECTOR user parameters (26.5.1 and 27.3.11.13).
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
                auto fcsMode = hcfMac != nullptr ? hcfMac->getFcsMode() : FCS_DECLARED_CORRECT;
                trailer->setFcsMode(fcsMode);
                if (fcsMode == FCS_COMPUTED)
                    trailer->setFcs(computeEthernetFcs(staPacket, fcsMode));
                staPacket->insertAtBack(trailer);
            }

            ackHandler->frameGotInProgress(dataOrMgmtHdrWritable);
            context->getInProgressFrames()->addInProgressFrame(staPacket);
        }

        auto payloadHeader = makeShared<Ieee80211HeMuRuPayloadHeader>();
        payloadHeader->setRuIndex(alloc.ru.index);
        payloadHeader->setRuToneSize(alloc.ru.toneSize);
        payloadHeader->setRuToneOffset(alloc.ru.toneOffset);
        payloadHeader->setStaId(selectedAllocation.associationId);
        payloadHeader->setMcs(alloc.mcs);
        payloadHeader->setNumberOfSpatialStreams(alloc.numberOfSpatialStreams);
        payloadHeader->setDcm(alloc.dcm);
        payloadHeader->setMpduLength(selectedAllocation.psduLength);
        payloadHeader->setStreamStartIndex(selectedAllocation.streamStartIndex);
        payloadHeader->setMuMimo(selectedAllocation.muMimo);
        if (selectedAllocation.muMimo)
            payloadHeader->setTotalNsts(selectedAllocation.totalNsts);
        container->insertAtBack(payloadHeader);
        for (size_t i = 0; i < staPackets.size(); ++i) {
            // 9.7.1 A-MPDU subframes use MPDU delimiters and 4-octet padding;
            // 26.6.2 applies that HE padding model to HE MU PPDUs.
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
    // The common request tag carries the HE MU PPDU common TXVECTOR fields
    // that are not literal MAC header fields: GI, coding, PE duration, and
    // preamble puncturing state (26.11 and 27.3.11.13).
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
