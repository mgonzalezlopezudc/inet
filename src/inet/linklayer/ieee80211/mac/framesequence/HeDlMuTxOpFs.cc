//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

#include <algorithm>
#include <map>

#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"

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
//   - The HE MU PPDU is represented as a single container Packet whose data is
//     the ordered concatenation of the per-user PSDUs. Transmitter-local tags
//     carry the TXVECTOR allocation metadata.
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
#include "inet/linklayer/ieee80211/mac/framesequence/Ieee80211HeMuContainerTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
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

Ieee80211HeTriggerResponseFinalizationResult finalizeTriggeredBlockAckResponse(
        const std::vector<Ieee80211HeUserPhyParameters>& users,
        Hz centerFrequency, Hz channelBandwidth)
{
    Ieee80211HeTriggerResponseFinalizationRequest request;
    request.users = users;
    request.centerFrequency = centerFrequency;
    request.channelBandwidth = channelBandwidth;
    // Table 27-32 makes 2x HE-LTF with 1.6 us GI the universally mandatory
    // HE-TB response pair; 1x/1.6 us is limited to full-bandwidth UL MU-MIMO.
    request.guardInterval = HE_GI_1_6_US;
    request.ltfType = HE_LTF_2X;
    return finalizeHeTriggerResponse(request);
}

Ieee80211HeUserPhyParameters makeTriggeredBlockAckResponseUser(
        const Ieee80211HeRu& ru, uint16_t staId, int numberOfSpatialStreams,
        int streamStartIndex, Ieee80211HeCoding coding)
{
    Ieee80211HeUserPhyParameters user;
    user.ru = ru;
    user.mcs = 0;
    user.numberOfSpatialStreams = numberOfSpatialStreams;
    user.streamStartIndex = streamStartIndex;
    user.staId = staId;
    user.coding = ru.toneSize >= 484 ? HE_CODING_LDPC : coding;
    user.psduLength = LENGTH_COMPRESSED_BLOCKACK;
    return user;
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
        if (negotiated != nullptr && negotiated->localTxPeerRx.multiTidAggregation) {
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
        bool multiTid = (negotiated != nullptr && negotiated->localTxPeerRx.multiTidAggregation);

        auto activeRecordCount = multiTid ? std::max<size_t>(1, collectStartingSequenceNumbersByTid(getActiveAllocation().packets).size()) : 0;
        auto blockAckDuration = responseMode->getDuration(multiTid ? b(B(18 + 12 * activeRecordCount)) : LENGTH_BASIC_BLOCKACK);
        auto remainingDuration = owner->modeSet->getSifsTime() + blockAckDuration;
        for (int nextIndex = allocationIndex + 1; nextIndex < (int)owner->activeAllocations.size(); nextIndex++) {
            auto nextNegotiated = mib != nullptr ? mib->findNegotiatedHeCapabilities(owner->activeAllocations.at(nextIndex).staAddress) : nullptr;
            bool nextMultiTid = (nextNegotiated != nullptr && nextNegotiated->localTxPeerRx.multiTidAggregation);
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
        bool multiTid = (negotiated != nullptr && negotiated->localTxPeerRx.multiTidAggregation);

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
        header->setTransmitterAddress(owner->getTransmitterAddress());
        header->setTriggerType(2); // MU-BAR Trigger
        const auto& scheduleContext = owner->dlPlan.getScheduleContext();
        header->setChannelBandwidthMhz(std::lround(scheduleContext.channelBandwidth.get() / 1e6));
        header->setGuardInterval(HE_GI_1_6_US);
        header->setLtfType(HE_LTF_2X);
        header->setUsersArraySize(owner->activeAllocations.size());
        std::vector<Ieee80211HeUserPhyParameters> responseUsers;
        responseUsers.reserve(owner->activeAllocations.size());
        std::map<int, int> responseStreamStartIndex;
        for (size_t i = 0; i < owner->activeAllocations.size(); ++i) {
            const auto& allocation = owner->activeAllocations[i];
            auto responseStart = responseStreamStartIndex[allocation.ru.index]++;
            auto responseUser = makeTriggeredBlockAckResponseUser(allocation.ru,
                    allocation.associationId, 1, responseStart,
                    scheduleContext.coding);
            Ieee80211HeTriggerUserInfo user;
            user.aid = allocation.associationId;
            user.ruIndex = allocation.ruIndex;
            user.ruToneSize = allocation.ru.toneSize;
            user.ruToneOffset = allocation.ru.toneOffset;
            user.mcs = 0;
            user.coding = responseUser.coding;
            user.numberOfSpatialStreams = 1;
            user.streamStartIndex = responseStart;
            user.muMimo = allocation.muMimo;
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
            responseUsers.push_back(responseUser);
        }
        auto finalization = finalizeTriggeredBlockAckResponse(responseUsers,
                scheduleContext.channelCenterFrequency,
                scheduleContext.channelBandwidth);
        if (!finalization)
            throw cRuntimeError("Cannot finalize MU-BAR Trigger response: %s",
                    finalization.error.c_str());
        header->setUlLength(finalization.ulLength);
        header->setNumberOfHeLtfSymbols(finalization.parameters.common.numberOfHeLtfSymbols);
        header->setPreFecPaddingFactor(finalization.parameters.common.preFecPaddingFactor);
        header->setLdpcExtraSymbolSegment(finalization.parameters.common.ldpcExtraSymbol);
        header->setPeDisambiguity(finalization.peDisambiguity);
        auto durationEnvelope = getIeee80211HeTriggerTxTimeUpperBound(
                finalization.ulLength);
        if (!durationEnvelope)
            throw cRuntimeError("Cannot decode finalized MU-BAR Trigger UL Length: %s",
                    durationEnvelope.error.c_str());
        header->setCommonDuration(durationEnvelope.txTime);
        header->setDurationField(owner->modeSet->getSifsTime() + finalization.commonDuration);
        header->setChunkLength(getMuBarTriggerHeaderLength(owner->activeAllocations.size()));
        auto packet = new Packet("HE-MU-BAR-Trigger", header);
        packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
        packet->addTag<physicallayer::Ieee80211HeTriggerCorrelationTag>()->
                setTriggerId(owner->ackTriggerId);
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
            auto blockAck = dynamicPtrCast<const Ieee80211CompressedBlockAck>(
                    packet->peekAtFront<Ieee80211MacHeader>());
            if (blockAck == nullptr) {
                EV_WARN << "HE DL MU-BAR FS: received non-compressed BlockAck frame in MU-BAR response window\n";
                continue;
            }
            auto expected = std::find_if(owner->activeAllocations.begin(),
                    owner->activeAllocations.end(), [&] (const auto& allocation) {
                        return allocation.staAddress == blockAck->getTransmitterAddress() &&
                                allocation.tid == blockAck->getTidInfo();
                    });
            auto rx = packet->findTag<Ieee80211HeTbRecipientContextInd>();
            if (expected == owner->activeAllocations.end() ||
                    rx == nullptr ||
                    rx->getTriggerId() != owner->ackTriggerId ||
                    rx->getRecipientParameters() == nullptr ||
                    rx->getRecipientParameters()->ru.index != expected->ruIndex ||
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

HeDlMuTxOpFs::HeDlMuTxOpFs(const HeDlMuPlan& dlPlan,
                             Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback,
                             int maxAmpduMpduCount,
                             int maxHeMuPsduLength,
                             simtime_t maxHeMuPpduDuration,
                             AckMethod ackMethod)
    : dlPlan(dlPlan),
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
    ASSERT(modeSet != nullptr);
    ASSERT(pendingQueue != nullptr);
    ASSERT(ackHandler != nullptr);
    ASSERT(callback != nullptr);
    ackTriggerId = allocateIeee80211HeTriggerId();
    if (maxAmpduMpduCount <= 0)
        throw cRuntimeError("maxAmpduMpduCount must be positive");
    if (maxHeMuPsduLength <= 0)
        throw cRuntimeError("maxHeMuPsduLength must be positive");
    if (maxHeMuPpduDuration <= SIMTIME_ZERO)
        throw cRuntimeError("maxHeMuPpduDuration must be positive");
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

MacAddress HeDlMuTxOpFs::getTransmitterAddress() const
{
    auto hcfModule = check_and_cast<cModule *>(callback);
    auto mac = check_and_cast<Ieee80211Mac *>(
            getContainingNicModule(hcfModule)->getSubmodule("mac"));
    ASSERT(mac != nullptr);
    return mac->getAddress();
}

Packet *HeDlMuTxOpFs::buildMuContainerPacket(FrameSequenceContext *context)
{
    // IEEE 802.11-2024 26.5.1 / 27.3.11.13: an AP schedules one or more
    // per-user PSDUs in an HE MU PPDU using OFDMA RUs and, optionally,
    // MU-MIMO spatial streams.  This method builds INET's container Packet for
    // that PPDU and annotates each user payload with the corresponding RU PHY
    // parameters.
    ASSERT(context != nullptr);
    const auto& scheduleContext = dlPlan.getScheduleContext();
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
    const auto& allocations = dlPlan.getAllocations();
    EV_INFO << "Validated scheduler plan contains " << allocations.size() << " allocations\n";

    // Assemble the HE MU PPDU container packet.
    auto container = new Packet("HE-MU-PPDU");

    // Header metadata is passed to the MAC transmit interface but is not
    // inserted into the packet: an HE MU PPDU has independent per-user PSDUs,
    // not a broadcast wrapper MPDU.
    auto containerHdr = makeShared<Ieee80211DataHeader>();
    containerHdr->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    containerHdr->setType(ST_DATA_WITH_QOS);
    containerHdr->setChunkLength(b(208)); // minimal 802.11 QoS data header size

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
                negotiated->localTxPeerRx.valid && negotiated->localTxPeerRx.multiTidAggregation;
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

    // The preliminary MU-BAR timing check runs before the packing planner,
    // which later assigns the same contiguous per-RU stream ranges to the
    // final allocations. Supply those ranges here as well so shared-RU
    // MU-MIMO users do not all appear to start at spatial stream zero.
    std::map<int, int> preliminaryStreamStartIndex;
    for (auto& selectedAllocation : selectedAllocations) {
        auto ruIndex = selectedAllocation.allocation.ru.index;
        selectedAllocation.streamStartIndex = preliminaryStreamStartIndex[ruIndex];
        preliminaryStreamStartIndex[ruIndex] +=
                selectedAllocation.allocation.numberOfSpatialStreams;
    }

    Ptr<Ieee80211BlockAckReq> dummyReq;
    if (ackMethod == AckMethod::MU_BAR_TRIGGER)
        dummyReq = makeShared<Ieee80211CompressedBlockAckReq>();
    else
        dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
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
        selectedAllocations[idx].phyMode = staMode;
    }
    if (ackMethod == AckMethod::MU_BAR_TRIGGER) {
        auto triggerDuration = responseMode->getDuration(getMuBarTriggerFrameLength(selectedAllocations.size()));
        std::vector<Ieee80211HeUserPhyParameters> responseUsers;
        responseUsers.reserve(selectedAllocations.size());
        std::map<int, int> responseStreamStartIndex;
        for (const auto& selectedAllocation : selectedAllocations) {
            auto responseStart =
                    responseStreamStartIndex[selectedAllocation.allocation.ru.index]++;
            responseUsers.push_back(makeTriggeredBlockAckResponseUser(
                    selectedAllocation.allocation.ru, selectedAllocation.associationId,
                    1, responseStart, scheduleContext.coding));
        }
        auto response = finalizeTriggeredBlockAckResponse(responseUsers,
                scheduleContext.channelCenterFrequency, scheduleContext.channelBandwidth);
        if (!response) {
            EV_WARN << "Cannot reserve MU-BAR response phase: " << response.error << endl;
            delete container;
            notifyPlanningFailure();
            return nullptr;
        }
        totalDuration = modeSet->getSifsTime() + triggerDuration +
                modeSet->getSifsTime() + response.commonDuration;
    }

    // Keep the NAV duration in transmitter-local metadata; each real MPDU also
    // receives the same duration below.
    containerHdr->setDurationField(totalDuration);

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
    const auto& finalAllocations = packingPlan.getAllocations();
    EV_DEBUG << "Building MU container packet: " << finalAllocations.size()
             << " allocations survived final validation (" << packingPlan.getRejectedFinalValidation() << " rejected)\n";
    EV_DEBUG << "Building MU container packet: duration trim took " << packingPlan.getDurationTrimIterations()
             << " iteration(s), final allocations = " << finalAllocations.size() << "\n";
    if (!packingPlan) {
        EV_WARN << "Building MU container packet: " << packingPlan.getFailureReason() << endl;
        delete container;
        notifyPlanningFailure();
        return nullptr;
    }
    const auto& plannedPpdu = packingPlan.getPpdu();
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
        std::vector<Ieee80211HeUserPhyParameters> responseUsers;
        responseUsers.reserve(finalAllocations.size());
        std::map<int, int> responseStreamStartIndex;
        for (const auto& finalAllocation : finalAllocations) {
            auto responseStart =
                    responseStreamStartIndex[finalAllocation.allocation.ru.index]++;
            responseUsers.push_back(makeTriggeredBlockAckResponseUser(
                    finalAllocation.allocation.ru, finalAllocation.associationId,
                    1, responseStart, scheduleContext.coding));
        }
        auto response = finalizeTriggeredBlockAckResponse(responseUsers,
                scheduleContext.channelCenterFrequency, scheduleContext.channelBandwidth);
        if (!response) {
            EV_WARN << "Cannot finalize MU-BAR response phase: " << response.error << endl;
            delete container;
            notifyPlanningFailure();
            return nullptr;
        }
        totalDuration = modeSet->getSifsTime() + triggerDuration +
                modeSet->getSifsTime() + response.commonDuration;
    }
    containerHdr->setDurationField(totalDuration);
    // Prepare the complete PPDU on private packet copies. Sequence numbers are
    // assigned against a cloned counter state, so every remaining fallible
    // operation precedes the queue/BA ownership commit.
    auto heHcf = dynamic_cast<HeHcf *>(callback);
    OriginatorQosMacDataService *originatorQosDataService = nullptr;
    std::unique_ptr<ISequenceNumberAssignment> preparedSequenceNumberState;
    if (heHcf != nullptr) {
        originatorQosDataService = check_and_cast<OriginatorQosMacDataService *>(
                heHcf->getOriginatorMacDataService());
        preparedSequenceNumberState = originatorQosDataService->cloneSequenceNumberState();
    }
    std::map<Packet *, std::unique_ptr<Packet>> preparedPackets;
    for (const auto& selectedAllocation : finalAllocations) {
        for (auto originalPacket : selectedAllocation.packets) {
            auto preparedPacket = std::unique_ptr<Packet>(originalPacket->dup());
            auto writableHeader = preparedPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
            if (preparedSequenceNumberState != nullptr && !writableHeader->getRetry())
                preparedSequenceNumberState->assignSequenceNumber(writableHeader);
            if (auto dataHeader = dynamicPtrCast<Ieee80211DataHeader>(writableHeader))
                dataHeader->setAckPolicy(BLOCK_ACK);
            writableHeader->setDurationField(totalDuration);
            preparedPacket->insertAtFront(writableHeader);
            RateSelection::setFrameMode(preparedPacket.get(), writableHeader, selectedAllocation.phyMode);

            if (preparedPacket->getDataLength() >= B(4) &&
                    dynamicPtrCast<const Ieee80211MacTrailer>(preparedPacket->peekAtBack(B(4))) != nullptr) {
                auto trailer = preparedPacket->removeAtBack<Ieee80211MacTrailer>(B(4));
                auto fcsMode = hcfMac != nullptr ? hcfMac->getFcsMode() : FCS_DECLARED_CORRECT;
                trailer->setFcsMode(fcsMode);
                if (fcsMode == FCS_COMPUTED)
                    trailer->setFcs(computeEthernetFcs(preparedPacket.get(), fcsMode));
                preparedPacket->insertAtBack(trailer);
            }
            // Header/trailer replacement may clear packet-level region tags
            // covering the changed chunks. Their regions remain valid because
            // DL preparation never changes the MPDU length.
            preparedPacket->getRegionTags() = originalPacket->getRegionTags();
            preparedPackets.emplace(originalPacket, std::move(preparedPacket));
        }
    }

    // Build the final MU container packet from prepared copies. Each selected
    // STA becomes one per-user PSDU section. Multiple users on the same RU are
    // represented as DL MU-MIMO with stream-start indices.
    for (const auto& selectedAllocation : finalAllocations) {
        const auto& alloc = selectedAllocation.allocation;
        Packet *firstPacket = selectedAllocation.packet;
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                preparedPackets.at(firstPacket)->peekAtFront<Ieee80211MacHeader>());
        ASSERT(dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS);
        ASSERT(hasActiveOriginatorBlockAckAgreement(originatorBAHandler,
                dataHeader->getReceiverAddress(), dataHeader->getTid()));

        const auto& staPackets = selectedAllocation.packets;

        auto psduStartOffset = container->getDataLength();
        for (size_t i = 0; i < staPackets.size(); ++i) {
            auto preparedPacket = preparedPackets.at(staPackets[i]).get();
            // 9.7.1 A-MPDU subframes use MPDU delimiters and 4-octet padding;
            // 26.6.2 applies that HE padding model to HE MU PPDUs.
            auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
            delimiter->setLength(preparedPacket->getByteLength());
            container->insertAtBack(delimiter);
            // Only the active MPDU data region is serialized into the PSDU;
            // popped historical content remains part of packet identity but
            // must never reappear on the wire.
            container->insertAtBack(preparedPacket->peekData());
            int padding = (4 - (B(4) + B(preparedPacket->getByteLength())).get<B>() % 4) % 4;
            if (i + 1 != staPackets.size() && padding != 0)
                container->insertAtBack(makeShared<ByteCountChunk>(B(padding)));
        }
        ASSERT(container->getDataLength() - psduStartOffset == selectedAllocation.psduLength);

        ActiveAllocation activeAlloc;
        activeAlloc.staAddress = alloc.staAddress;
        activeAlloc.associationId = selectedAllocation.associationId;
        activeAlloc.tid = dataHeader->getTid();
        activeAlloc.ruIndex = alloc.ru.index;
        activeAlloc.ru = alloc.ru;
        activeAlloc.mcs = alloc.mcs;
        activeAlloc.numberOfSpatialStreams = alloc.numberOfSpatialStreams;
        activeAlloc.streamStartIndex = selectedAllocation.streamStartIndex;
        activeAlloc.totalNsts = selectedAllocation.totalNsts;
        activeAlloc.muMimo = selectedAllocation.muMimo;
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

    // The scheduler plan is projected once into the immutable canonical
    // TXVECTOR/layout before ownership commits. The radio must not
    // reinterpret mutable per-user or common request tags.
    Ieee80211HeTxVectorRequest canonicalRequest;
    canonicalRequest.centerFrequency = scheduleContext.channelCenterFrequency;
    canonicalRequest.channelBandwidth = scheduleContext.channelBandwidth;
    canonicalRequest.ppduFormat = HE_MU_DOWNLINK;
    canonicalRequest.puncturedSubchannelMask = scheduleContext.puncturedSubchannelMask;
    canonicalRequest.bssColor = hcfMac == nullptr ? 0 : hcfMac->getMib()->heOperation.bssColor;
    if (scheduleContext.txopLimit > SIMTIME_ZERO)
        canonicalRequest.txopDuration = {false, static_cast<uint16_t>(std::min<int64_t>(
                8448, scheduleContext.txopLimit.inUnit(SIMTIME_US)))};
    canonicalRequest.guardInterval = scheduleContext.guardInterval;
    canonicalRequest.ltfType = scheduleContext.ltfType;
    canonicalRequest.packetExtensionDurationUs = scheduleContext.packetExtensionDurationUs;
    for (const auto& selectedAllocation : finalAllocations) {
        Ieee80211HeUserTxVectorRequest user;
        user.ru = selectedAllocation.allocation.ru;
        user.staId = selectedAllocation.associationId;
        user.mcs = selectedAllocation.allocation.mcs;
        user.numberOfSpatialStreams = selectedAllocation.allocation.numberOfSpatialStreams;
        user.streamStartIndex = selectedAllocation.streamStartIndex;
        user.dcm = selectedAllocation.allocation.dcm;
        user.coding = scheduleContext.coding;
        user.psduLength = selectedAllocation.psduLength;
        canonicalRequest.users.push_back(user);
    }
    auto canonicalResult = Ieee80211HeTxVectorFactory::create(canonicalRequest);
    if (!canonicalResult ||
            canonicalResult.getPpduLayout()->getDuration() != plannedPpdu.parameters.duration) {
        EV_WARN << "Canonical HE DL MU TXVECTOR disagrees with the validated packing plan\n";
        activeAllocations.clear();
        delete container;
        notifyPlanningFailure();
        return nullptr;
    }
    auto handoff = container->addTag<Ieee80211HeTxVectorReq>();
    handoff->setCanonicalPair(canonicalResult.getTxVector(), canonicalResult.getPpduLayout());
    container->addTag<Ieee80211HeMuContainerReq>()->setDurationField(totalDuration);

    if (activeAllocations.size() < 2)
        throw cRuntimeError("Validated HE DL MU plan lost allocations before commit");

    // Revalidate stable packet handles immediately before the only ownership
    // transition. Failure here leaves queues, packet contents, BA state, and
    // the live sequence-number counters untouched.
    for (const auto& selectedAllocation : finalAllocations) {
        auto sourceQueue = selectedAllocation.sourceQueue == nullptr ? pendingQueue : selectedAllocation.sourceQueue;
        for (auto packet : selectedAllocation.packets) {
            bool found = false;
            for (int i = 0; i < sourceQueue->getNumPackets(); ++i)
                found |= sourceQueue->getPacket(i) == packet;
            if (!found)
                throw cRuntimeError("HE DL MU selected packet changed queue membership before commit");
        }
    }

    // Exercise all fallible transaction hooks while every packet is still in
    // selected-uncommitted state. Aborting here is a complete rollback by
    // construction: queues, packets, BA state, and sequence counters have not
    // been mutated and no queue signal has been emitted.
    try {
        int packetIndex = 0;
        for (const auto& selectedAllocation : finalAllocations)
            for (auto originalPacket : selectedAllocation.packets) {
                (void)originalPacket;
                beforePacketCommit(packetIndex++);
            }
    }
    catch (const std::exception& error) {
        EV_WARN << "HE DL MU transaction aborted before commit: " << error.what() << endl;
        activeAllocations.clear();
        delete container;
        notifyPlanningFailure();
        return nullptr;
    }

    // Explicit commit boundary. The state transitions below are invariant
    // operations over revalidated handles and prepared immutable values. Per
    // Gate 3 policy, an internal failure after this point is loud; attempting
    // observer-visible queue reconstruction would not be a valid rollback.
    if (preparedSequenceNumberState != nullptr)
        originatorQosDataService->commitSequenceNumberState(*preparedSequenceNumberState);
    for (const auto& selectedAllocation : finalAllocations) {
        auto sourceQueue = selectedAllocation.sourceQueue == nullptr ? pendingQueue : selectedAllocation.sourceQueue;
        for (auto originalPacket : selectedAllocation.packets) {
            auto preparedPacket = preparedPackets.at(originalPacket).get();
            sourceQueue->removePacket(originalPacket);
            originalPacket->removeAll();
            originalPacket->insertAtBack(preparedPacket->peekAll());
            originalPacket->setFrontOffset(preparedPacket->getFrontOffset());
            originalPacket->setBackOffset(preparedPacket->getBackOffset());
            originalPacket->clearTags();
            originalPacket->copyTags(*preparedPacket);
            originalPacket->getRegionTags() = preparedPacket->getRegionTags();
            auto committedHeader = originalPacket->peekAtFront<Ieee80211DataOrMgmtHeader>();
            ackHandler->frameGotInProgress(committedHeader);
            context->getInProgressFrames()->addInProgressFrame(originalPacket);
        }
    }

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
