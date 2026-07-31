//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/recipient/RecipientQosMacDataService.h"

#include "inet/common/Simsignals.h"
#include "inet/linklayer/ieee80211/mac/aggregation/MpduDeaggregation.h"
#include "inet/linklayer/ieee80211/mac/aggregation/MsduDeaggregation.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/duplicateremoval/QosDuplicateRemoval.h"
#include "inet/linklayer/ieee80211/mac/fragmentation/BasicReassembly.h"
#include "inet/linklayer/ieee80211/mac/fragmentation/Defragmentation.h"

namespace inet {
namespace ieee80211 {

Define_Module(RecipientQosMacDataService);

// TODO refactor to avoid code duplication
void RecipientQosMacDataService::initialize()
{
    duplicateRemoval = new QoSDuplicateRemoval();
    basicReassembly = new BasicReassembly();
    aMsduDeaggregation = new MsduDeaggregation();
    aMpduDeaggregation = new MpduDeaggregation();
    blockAckReordering = new BlockAckReordering();
}

Packet *RecipientQosMacDataService::defragment(std::vector<Packet *> completeFragments)
{
    for (auto fragment : completeFragments) {
        auto packet = basicReassembly->addFragment(fragment);
        if (packet != nullptr) {
            if (packet != fragment)
                emit(packetDefragmentedSignal, packet);
            return packet;
        }
    }
    return nullptr;
}

Packet *RecipientQosMacDataService::defragment(Packet *mgmtFragment)
{
    auto packet = basicReassembly->addFragment(mgmtFragment);
    if (packet && packet->hasAtFront<Ieee80211DataOrMgmtHeader>()) {
        if (packet != mgmtFragment)
            emit(packetDefragmentedSignal, packet);
        return packet;
    }
    else
        return nullptr;
}

std::vector<Packet *> RecipientQosMacDataService::processReorderBuffer(
        const BlockAckReordering::ReorderBuffer& frames)
{
    std::vector<Packet *> defragmentedFrames;
    if (basicReassembly) { // FIXME defragmentation
        for (const auto& entry : frames) {
            auto frame = defragment(entry.second);
            if (frame != nullptr)
                defragmentedFrames.push_back(frame);
        }
    }
    else {
        for (const auto& entry : frames) {
            const auto& fragments = entry.second;
            if (fragments.size() == 1)
                defragmentedFrames.push_back(fragments.at(0));
            else ; // TODO drop?
        }
    }

    std::vector<Packet *> deaggregatedFrames;
    if (aMsduDeaggregation) {
        for (auto frame : defragmentedFrames) {
            if (frame->peekAtFront<Ieee80211DataHeader>()->getAMsduPresent()) {
                emit(packetDeaggregatedSignal, frame);
                auto subframes = aMsduDeaggregation->deaggregateFrame(frame);
                deaggregatedFrames.insert(deaggregatedFrames.end(),
                        subframes->begin(), subframes->end());
                delete subframes;
            }
            else
                deaggregatedFrames.push_back(frame);
        }
    }
    else
        deaggregatedFrames = defragmentedFrames;
    return deaggregatedFrames;
}

std::vector<Packet *> RecipientQosMacDataService::dataFrameReceived(Packet *dataPacket, const Ptr<const Ieee80211DataHeader>& dataHeader, IRecipientBlockAckAgreementHandler *blockAckAgreementHandler)
{
    Enter_Method("dataFrameReceived");
    take(dataPacket);
    // TODO A-MPDU Deaggregation, MPDU Header+FCS Validation, Address1 Filtering, Duplicate Removal, MPDU Decryption
    if (duplicateRemoval && duplicateRemoval->isDuplicate(dataHeader)) {
        EV_WARN << "Dropping duplicate packet " << *dataPacket << ".\n";
        PacketDropDetails details;
        details.setReason(DUPLICATE_DETECTED);
        emit(packetDroppedSignal, dataPacket, &details);
        delete dataPacket;
        return std::vector<Packet *>();
    }
    BlockAckReordering::ReorderBuffer frames;
    frames[dataHeader->getSequenceNumber().get()].push_back(dataPacket);
    if (blockAckReordering && blockAckAgreementHandler) {
        Tid tid = dataHeader->getTid();
        MacAddress originatorAddr = dataHeader->getTransmitterAddress();
        RecipientBlockAckAgreement *agreement = blockAckAgreementHandler->getAgreement(tid, originatorAddr);
        if (agreement)
            frames = blockAckReordering->processReceivedQoSFrame(agreement, dataPacket, dataHeader);
    }
    // TODO MSDU Integrity, Replay Detection, RX MSDU Rate Limiting
    return processReorderBuffer(frames);
}

std::vector<Packet *> RecipientQosMacDataService::managementFrameReceived(Packet *mgmtPacket, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader)
{
    Enter_Method("managementFrameReceived");
    take(mgmtPacket);
    // TODO MPDU Header+FCS Validation, Address1 Filtering, Duplicate Removal, MPDU Decryption
    if (duplicateRemoval && duplicateRemoval->isDuplicate(mgmtHeader))
        return std::vector<Packet *>();
    if (basicReassembly) { // FIXME defragmentation
        mgmtPacket = defragment(mgmtPacket);
    }
    if (auto delba = dynamicPtrCast<const Ieee80211Delba>(mgmtHeader))
        blockAckReordering->processReceivedDelba(delba);
    // TODO Defrag, MSDU Integrity, Replay Detection, RX MSDU Rate Limiting
    if (dynamicPtrCast<const Ieee80211ActionFrame>(mgmtHeader) &&
            !dynamicPtrCast<const Ieee80211TwtSetupFrame>(mgmtHeader) &&
            !dynamicPtrCast<const Ieee80211TwtTeardownFrame>(mgmtHeader) &&
            !dynamicPtrCast<const Ieee80211TwtInformationFrame>(mgmtHeader)) {
        delete mgmtPacket;
        return std::vector<Packet *>();
    }
    else
        return std::vector<Packet *>({ mgmtPacket });
}

std::vector<Packet *> RecipientQosMacDataService::controlFrameReceived(Packet *controlPacket, const Ptr<const Ieee80211MacHeader>& controlHeader, IRecipientBlockAckAgreementHandler *blockAckAgreementHandler)
{
    Enter_Method("controlFrameReceived");
    if (auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(controlHeader)) {
        if (blockAckReordering == nullptr || blockAckAgreementHandler == nullptr)
            return {};

        MacAddress originatorAddr = blockAckReq->getTransmitterAddress();
        std::vector<Packet *> deaggregatedFrames;
        if (auto multiTidReq =
                dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(blockAckReq)) {
            // Multi-TID BAR processing uses one independent Block Ack agreement
            // and reorder window per record. Process each result separately so
            // equal sequence numbers from different TIDs cannot be merged in
            // the sequence-number-keyed ReorderBuffer.
            for (unsigned int i = 0; i < multiTidReq->getRecordsArraySize(); ++i) {
                const auto& record = multiTidReq->getRecords(i);
                auto agreement = blockAckAgreementHandler->getAgreement(
                        record.tid, originatorAddr);
                if (agreement == nullptr)
                    continue;
                auto frames = blockAckReordering->processReceivedBlockAckReq(
                        agreement,
                        SequenceNumberCyclic(record.startingSequenceNumber));
                auto delivered = processReorderBuffer(frames);
                deaggregatedFrames.insert(deaggregatedFrames.end(),
                        delivered.begin(), delivered.end());
            }
        }
        else {
            Tid tid = -1;
            if (auto basicReq =
                    dynamicPtrCast<const Ieee80211BasicBlockAckReq>(blockAckReq))
                tid = basicReq->getTidInfo();
            else if (auto compressedReq =
                    dynamicPtrCast<const Ieee80211CompressedBlockAckReq>(blockAckReq))
                tid = compressedReq->getTidInfo();
            else
                return {};
            auto agreement = blockAckAgreementHandler->getAgreement(
                    tid, originatorAddr);
            if (agreement == nullptr)
                return {};
            auto frames = blockAckReordering->processReceivedBlockAckReq(
                    agreement, blockAckReq);
            deaggregatedFrames = processReorderBuffer(frames);
        }
        // TODO MSDU Integrity, Replay Detection, RX MSDU Rate Limiting
        return deaggregatedFrames;
    }
    return std::vector<Packet *>();
}

RecipientQosMacDataService::~RecipientQosMacDataService()
{
    delete duplicateRemoval;
    delete basicReassembly;
    delete aMsduDeaggregation;
    delete aMpduDeaggregation;
    delete blockAckReordering;
}

} /* namespace ieee80211 */
} /* namespace inet */
