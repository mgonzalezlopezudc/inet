//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckProcedure.h"

#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"

namespace inet {
namespace ieee80211 {

const Ptr<Ieee80211BlockAck> RecipientBlockAckProcedure::buildBlockAck(
        const Ptr<const Ieee80211BlockAckReq>& blockAckReq,
        IRecipientBlockAckAgreementHandler *blockAckAgreementHandler) const
{
    Tid tid = -1;
    if (auto basicBlockAckReq = dynamicPtrCast<const Ieee80211BasicBlockAckReq>(blockAckReq))
        tid = basicBlockAckReq->getTidInfo();
    else if (auto compressedBlockAckReq = dynamicPtrCast<const Ieee80211CompressedBlockAckReq>(blockAckReq))
        tid = compressedBlockAckReq->getTidInfo();
    else
        throw cRuntimeError("Unsupported Block Ack Request");
    auto agreement = blockAckAgreementHandler->getAgreement(
            tid, blockAckReq->getTransmitterAddress());
    return agreement == nullptr ? nullptr : buildBlockAck(blockAckReq, agreement);
}

//
// Upon successful reception of a frame of a type that requires an immediate BlockAck response, the receiving
// STA shall transmit a BlockAck frame after a SIFS period, without regard to the busy/idle state of the medium.
// The rules that specify the contents of this BlockAck frame are defined in 9.21.
//
void RecipientBlockAckProcedure::processReceivedBlockAckReq(Packet *blockAckPacketReq,
        const Ptr<const Ieee80211BlockAckReq>& blockAckReq,
        IRecipientQosAckPolicy *ackPolicy,
        IRecipientBlockAckAgreementHandler *blockAckAgreementHandler,
        IProcedureCallback *callback,
        MultiTidBlockAckResponseFormat multiTidResponseFormat,
        uint16_t responseAid)
{
    numReceivedBlockAckReq++;
    if (auto singleTidBlockAckReq = dynamicPtrCast<const Ieee80211BasicBlockAckReq>(blockAckReq)) {
        auto agreement = blockAckAgreementHandler->getAgreement(singleTidBlockAckReq->getTidInfo(), singleTidBlockAckReq->getTransmitterAddress());
        if (ackPolicy->isBlockAckNeeded(singleTidBlockAckReq, agreement)) {
            auto blockAck = buildBlockAck(singleTidBlockAckReq, agreement);
            auto duration = ackPolicy->computeBasicBlockAckDurationField(blockAckPacketReq, singleTidBlockAckReq);
            blockAck->setDurationField(duration);
            auto blockAckPacket = new Packet(dynamicPtrCast<const Ieee80211CompressedBlockAck>(blockAck) ? "CompressedBlockAck" : "BasicBlockAck", blockAck);
            EV_DEBUG << "Duration for " << blockAckPacket->getName() << " is set to " << duration << " s.\n";
            callback->transmitControlResponseFrame(blockAckPacket, blockAck, blockAckPacketReq, singleTidBlockAckReq);
        }
    }
    else if (auto compressedBlockAckReq = dynamicPtrCast<const Ieee80211CompressedBlockAckReq>(blockAckReq)) {
        auto agreement = blockAckAgreementHandler->getAgreement(compressedBlockAckReq->getTidInfo(), compressedBlockAckReq->getTransmitterAddress());
        if (ackPolicy->isBlockAckNeeded(compressedBlockAckReq, agreement)) {
            auto blockAck = buildBlockAck(compressedBlockAckReq, agreement);
            auto duration = ackPolicy->computeBasicBlockAckDurationField(blockAckPacketReq, compressedBlockAckReq);
            blockAck->setDurationField(duration);
            auto blockAckPacket = new Packet("CompressedBlockAck", blockAck);
            EV_DEBUG << "Duration for " << blockAckPacket->getName() << " is set to " << duration << " s.\n";
            callback->transmitControlResponseFrame(blockAckPacket, blockAck, blockAckPacketReq, compressedBlockAckReq);
        }
    }
    else if (auto multiTidBlockAckReq = dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(blockAckReq)) {
        if (multiTidResponseFormat == MultiTidBlockAckResponseFormat::NONE) {
            EV_WARN << "Ignoring a Multi-TID BlockAckReq because no enabled response format applies.\n";
            return;
        }
        if (ackPolicy->isBlockAckNeeded(multiTidBlockAckReq, nullptr)) {
            unsigned int numRecords = multiTidBlockAckReq->getRecordsArraySize();
            std::vector<uint64_t> bitmaps(numRecords, 0);
            for (unsigned int i = 0; i < numRecords; ++i) {
                const auto& reqRec = multiTidBlockAckReq->getRecords(i);
                auto agreement = blockAckAgreementHandler->getAgreement(reqRec.tid, multiTidBlockAckReq->getTransmitterAddress());
                if (agreement != nullptr) {
                    auto agreementSnapshot = agreement->getSnapshot();
                    SequenceNumberCyclic startingSequenceNumber(reqRec.startingSequenceNumber);
                    for (int j = 0; j < 64; ++j) {
                        bool ackState = agreementSnapshot.record.getAckState(startingSequenceNumber + j, 0);
                        if (ackState)
                            bitmaps[i] |= (1ULL << j);
                    }
                }
            }

            Ptr<Ieee80211BlockAck> blockAck;
            const char *packetName = nullptr;
            // IEEE Std 802.11-2024, 10.25.5 and 26.4.5: an HE Multi-TID
            // BlockAckReq solicits a Multi-STA BlockAck response.
            if (multiTidResponseFormat == MultiTidBlockAckResponseFormat::HE_MULTI_STA) {
                auto multiStaBlockAck = makeShared<Ieee80211MultiStaBlockAck>();
                multiStaBlockAck->setBlockAckPolicy(false);
                multiStaBlockAck->setRecordsArraySize(numRecords);
                for (unsigned int i = 0; i < numRecords; ++i) {
                    const auto& reqRec = multiTidBlockAckReq->getRecords(i);
                    Ieee80211MultiStaBlockAckRecord ackRec;
                    ackRec.aid = responseAid;
                    ackRec.tid = reqRec.tid;
                    ackRec.startingSequenceNumber = reqRec.startingSequenceNumber;
                    ackRec.bitmap = bitmaps[i];
                    ackRec.responseReceived = true;
                    multiStaBlockAck->setRecords(i, ackRec);
                }
                blockAck = multiStaBlockAck;
                packetName = "MultiStaBlockAck";
            }
            else {
                ASSERT(multiTidResponseFormat == MultiTidBlockAckResponseFormat::LEGACY_MULTI_TID);
                // This explicitly enabled INET extension models the historical
                // IEEE 802.11n Multi-TID BlockAck response; BA Type 3 is
                // reserved by IEEE Std 802.11-2024 Table 9-37.
                auto multiTidBlockAck = makeShared<Ieee80211MultiTidBlockAck>();
                multiTidBlockAck->setBlockAckPolicy(false);
                multiTidBlockAck->setRecordsArraySize(numRecords);
                for (unsigned int i = 0; i < numRecords; ++i) {
                    const auto& reqRec = multiTidBlockAckReq->getRecords(i);
                    Ieee80211MultiTidBlockAckRecord ackRec;
                    ackRec.tid = reqRec.tid;
                    ackRec.startingSequenceNumber = reqRec.startingSequenceNumber;
                    ackRec.bitmap = bitmaps[i];
                    multiTidBlockAck->setRecords(i, ackRec);
                }
                blockAck = multiTidBlockAck;
                packetName = "MultiTidBlockAck";
            }
            blockAck->setReceiverAddress(multiTidBlockAckReq->getTransmitterAddress());
            blockAck->setTransmitterAddress(blockAckReq->getReceiverAddress());
            blockAck->setChunkLength(B(18 + numRecords * 12));
            auto duration = ackPolicy->computeBasicBlockAckDurationField(blockAckPacketReq, multiTidBlockAckReq);
            blockAck->setDurationField(duration);
            auto blockAckPacket = new Packet(packetName, blockAck);
            EV_DEBUG << "Duration for " << blockAckPacket->getName() << " is set to " << duration << " s.\n";
            callback->transmitControlResponseFrame(blockAckPacket, blockAck, blockAckPacketReq, multiTidBlockAckReq);
        }
    }
    else
        throw cRuntimeError("Unsupported BlockAckReq");
}

bool RecipientBlockAckProcedure::processReceivedHtImplicitBlockAckRequest(
        Packet *ampduPacket,
        const std::vector<Ptr<const Ieee80211DataHeader>>& dataHeaders,
        IRecipientBlockAckAgreementHandler *blockAckAgreementHandler,
        IBlockAckAgreementHandlerCallback *agreementHandlerCallback,
        IProcedureCallback *procedureCallback)
{
    if (dataHeaders.empty())
        return false;
    const auto& firstHeader = dataHeaders.front();
    const auto originatorAddress = firstHeader->getTransmitterAddress();
    const auto recipientAddress = firstHeader->getReceiverAddress();
    const auto tid = firstHeader->getTid();
    if (originatorAddress.isUnspecified() ||
            recipientAddress.isMulticast() ||
            firstHeader->getAckPolicy() != NORMAL_ACK)
        return false;
    auto agreement = blockAckAgreementHandler->getAgreement(
            tid, originatorAddress);
    if (agreement == nullptr)
        return false;
    for (const auto& header : dataHeaders) {
        if (header->getType() != ST_DATA_WITH_QOS ||
                header->getAckPolicy() != NORMAL_ACK ||
                header->getTransmitterAddress() != originatorAddress ||
                header->getReceiverAddress() != recipientAddress ||
                header->getTid() != tid ||
                header->getFragmentNumber() != 0 ||
                header->getMoreFragments())
            return false;
    }
    for (const auto& header : dataHeaders)
        if (!blockAckAgreementHandler->
                    implicitBlockAckRequestFrameReceived(
                            header, agreementHandlerCallback))
            return false;

    // IEEE Std 802.11-2024, 10.25.6.3 and 10.25.6.5: anchor the
    // Compressed BlockAck bitmap at the recipient scoreboard's WinStartR,
    // not at the first MPDU that happened to decode successfully.
    auto agreementSnapshot = agreement->getSnapshot();
    auto startingSequenceNumber = agreementSnapshot.record.winStartR;
    std::vector<uint8_t> bytes(8, 0);
    BitVector bitmap(bytes);
    for (int i = 0; i < 64; i++)
        bitmap.setBit(i, agreementSnapshot.record.getAckState(
                startingSequenceNumber + i, 0));
    auto blockAck = makeShared<Ieee80211CompressedBlockAck>();
    blockAck->setReceiverAddress(originatorAddress);
    blockAck->setStartingSequenceNumber(startingSequenceNumber);
    blockAck->setTidInfo(tid);
    blockAck->setBlockAckBitmap(bitmap);
    auto blockAckPacket = new Packet("CompressedBlockAck", blockAck);
    // IEEE Std 802.11-2024 Table 9-13 and 10.25.6.5: Ack Policy 00
    // in a non-single-MPDU A-MPDU is an implicit BAR; the recipient sends
    // one Compressed BlockAck after SIFS without an on-air BAR.
    procedureCallback->transmitControlResponseFrame(blockAckPacket, blockAck,
            ampduPacket, firstHeader);
    return true;
}

void RecipientBlockAckProcedure::processTransmittedBlockAck(const Ptr<const Ieee80211BlockAck>& blockAck)
{
    numSentBlockAck++;
}

//
// The Basic BlockAck frame contains acknowledgments for the MPDUs of up to 64 previous MSDUs. In the
// Basic BlockAck frame, the STA acknowledges only the MPDUs starting from the starting sequence control
// until the MPDU with the highest sequence number that has been received, and the STA shall set bits in the
// Block Ack bitmap corresponding to all other MPDUs to 0.
//
const Ptr<Ieee80211BlockAck> RecipientBlockAckProcedure::buildBlockAck(const Ptr<const Ieee80211BlockAckReq>& blockAckReq, RecipientBlockAckAgreement *agreement) const
{
    if (auto basicBlockAckReq = dynamicPtrCast<const Ieee80211BasicBlockAckReq>(blockAckReq)) {
        ASSERT(agreement != nullptr);
        auto blockAck = makeShared<Ieee80211BasicBlockAck>();
        auto startingSequenceNumber = basicBlockAckReq->getStartingSequenceNumber();
        agreement->advanceWindow(startingSequenceNumber);
        auto agreementSnapshot = agreement->getSnapshot();
        for (int i = 0; i < 64; i++) {
            BitVector& bitmap = blockAck->getBlockAckBitmapForUpdate(i);
            for (FragmentNumber fragNum = 0; fragNum < 16; fragNum++) {
                bool ackState = agreementSnapshot.record.getAckState(startingSequenceNumber + i, fragNum);
                bitmap.setBit(fragNum, ackState);
            }
        }
        blockAck->setReceiverAddress(blockAckReq->getTransmitterAddress());
        blockAck->setCompressedBitmap(false);
        blockAck->setStartingSequenceNumber(basicBlockAckReq->getStartingSequenceNumber());
        blockAck->setTidInfo(basicBlockAckReq->getTidInfo());
        return blockAck;
    }
    else if (auto compressedBlockAckReq = dynamicPtrCast<const Ieee80211CompressedBlockAckReq>(blockAckReq)) {
        ASSERT(agreement != nullptr);
        auto blockAck = makeShared<Ieee80211CompressedBlockAck>();
        auto startingSequenceNumber = compressedBlockAckReq->getStartingSequenceNumber();
        agreement->advanceWindow(startingSequenceNumber);
        auto agreementSnapshot = agreement->getSnapshot();
        std::vector<uint8_t> bytes(8, 0);
        BitVector bitmap(bytes);
        for (int i = 0; i < 64; i++) {
            bool ackState = agreementSnapshot.record.getAckState(startingSequenceNumber + i, 0);
            bitmap.setBit(i, ackState);
        }
        blockAck->setReceiverAddress(blockAckReq->getTransmitterAddress());
        blockAck->setStartingSequenceNumber(compressedBlockAckReq->getStartingSequenceNumber());
        blockAck->setTidInfo(compressedBlockAckReq->getTidInfo());
        blockAck->setBlockAckBitmap(bitmap);
        return blockAck;
    }
    else
        throw cRuntimeError("Unsupported Block Ack Request");
}

} /* namespace ieee80211 */
} /* namespace inet */
