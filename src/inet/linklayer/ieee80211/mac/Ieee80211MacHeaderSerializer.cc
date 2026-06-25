//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {

namespace {

void copyBasicFields(const Ptr<ieee80211::Ieee80211MacHeader> to, const Ptr<ieee80211::Ieee80211MacHeader> from)
{
    to->setType(from->getType());
    to->setOrder(from->getOrder());
    to->setProtectedFrame(from->getProtectedFrame());
    to->setMoreData(from->getMoreData());
    to->setPowerMgmt(from->getPowerMgmt());
    to->setRetry(from->getRetry());
    to->setMoreFragments(from->getMoreFragments());
    to->setFromDS(from->getFromDS());
    to->setToDS(from->getToDS());
}

void copyActionFrameFields(const Ptr<ieee80211::Ieee80211ActionFrame> to, const Ptr<ieee80211::Ieee80211ActionFrame> from)
{
    to->setDurationField(from->getDurationField());
    to->setReceiverAddress(from->getReceiverAddress());
    to->setTransmitterAddress(from->getTransmitterAddress());
    to->setAddress3(from->getAddress3());
    to->setFragmentNumber(from->getFragmentNumber());
    to->setSequenceNumber(from->getSequenceNumber());
    to->setCategory(from->getCategory());
}

void copyBlockAckReqFrameFields(const Ptr<ieee80211::Ieee80211BlockAckReq> to, const Ptr<ieee80211::Ieee80211BlockAckReq> from)
{
    to->setDurationField(from->getDurationField());
    to->setReceiverAddress(from->getReceiverAddress());
    to->setTransmitterAddress(from->getTransmitterAddress());
    to->setBarAckPolicy(from->getBarAckPolicy());
    to->setMultiTid(from->getMultiTid());
    to->setCompressedBitmap(from->getCompressedBitmap());
    to->setReserved(from->getReserved());
}

void copyBlockAckFrameFields(const Ptr<ieee80211::Ieee80211BlockAck> to, const Ptr<ieee80211::Ieee80211BlockAck> from)
{
    to->setDurationField(from->getDurationField());
    to->setReceiverAddress(from->getReceiverAddress());
    to->setTransmitterAddress(from->getTransmitterAddress());
    to->setBlockAckPolicy(from->getBlockAckPolicy());
    to->setMultiTid(from->getMultiTid());
    to->setCompressedBitmap(from->getCompressedBitmap());
    to->setReserved(from->getReserved());
}

} // namespace

namespace ieee80211 {

Register_Serializer(Ieee80211MacHeader, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211DataOrMgmtHeader, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211DataHeader, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211MgmtHeader, Ieee80211MacHeaderSerializer);

Register_Serializer(Ieee80211AckFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211RtsFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211CtsFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211TriggerFrame, Ieee80211MacHeaderSerializer);

Register_Serializer(Ieee80211BasicBlockAckReq, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211CompressedBlockAckReq, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211MultiTidBlockAckReq, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211BasicBlockAck, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211CompressedBlockAck, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211MultiTidBlockAck, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211MultiStaBlockAck, Ieee80211MacHeaderSerializer);

Register_Serializer(Ieee80211ActionFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211TwtSetupFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211TwtTeardownFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211TwtInformationFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211PsPollFrame, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211AddbaRequest, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211AddbaResponse, Ieee80211MacHeaderSerializer);
Register_Serializer(Ieee80211Delba, Ieee80211MacHeaderSerializer);

Register_Serializer(Ieee80211MacTrailer, Ieee80211MacTrailerSerializer);

Register_Serializer(Ieee80211MsduSubframeHeader, Ieee80211MsduSubframeHeaderSerializer);
Register_Serializer(Ieee80211MpduSubframeHeader, Ieee80211MpduSubframeHeaderSerializer);

void Ieee80211MsduSubframeHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto msduSubframe = dynamicPtrCast<const Ieee80211MsduSubframeHeader>(chunk);
    stream.writeMacAddress(msduSubframe->getDa());
    stream.writeMacAddress(msduSubframe->getSa());
    stream.writeUint16Be(msduSubframe->getLength());
}

const Ptr<Chunk> Ieee80211MsduSubframeHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto msduSubframe = makeShared<Ieee80211MsduSubframeHeader>();
    msduSubframe->setDa(stream.readMacAddress());
    msduSubframe->setSa(stream.readMacAddress());
    msduSubframe->setLength(stream.readUint16Be());
    return msduSubframe;
}

void Ieee80211MpduSubframeHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto mpduSubframe = dynamicPtrCast<const Ieee80211MpduSubframeHeader>(chunk);
    if (mpduSubframe->getReserved() > 1 || mpduSubframe->getLength() < 0 ||
            mpduSubframe->getLength() > 16383 || mpduSubframe->getSignature() != 0x4E)
        throw cRuntimeError("Invalid A-MPDU delimiter");
    auto computeDelimiterCrc = [] (uint16_t value) {
        uint8_t crc = 0xFF;
        for (int bit = 0; bit < 16; ++bit) {
            bool feedback = ((crc >> 7) & 1) ^ ((value >> bit) & 1);
            crc <<= 1;
            if (feedback)
                crc ^= 0x07;
        }
        return static_cast<uint8_t>(~crc);
    };
    uint16_t delimiterValue = (mpduSubframe->getEof() ? 1 : 0) |
            ((mpduSubframe->getReserved() & 1) << 1) |
            ((mpduSubframe->getLength() & 0x3FFF) << 2);
    stream.writeUint2(delimiterValue & 0x3);
    for (int bit = 13; bit >= 8; --bit)
        stream.writeBit((mpduSubframe->getLength() >> bit) & 1);
    stream.writeUint8(mpduSubframe->getLength() & 0xFF);
    stream.writeByte(computeDelimiterCrc(delimiterValue));
    stream.writeByte(mpduSubframe->getSignature());
}

const Ptr<Chunk> Ieee80211MpduSubframeHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto mpduSubframe = makeShared<Ieee80211MpduSubframeHeader>();
    auto computeDelimiterCrc = [] (uint16_t value) {
        uint8_t crc = 0xFF;
        for (int bit = 0; bit < 16; ++bit) {
            bool feedback = ((crc >> 7) & 1) ^ ((value >> bit) & 1);
            crc <<= 1;
            if (feedback)
                crc ^= 0x07;
        }
        return static_cast<uint8_t>(~crc);
    };
    uint8_t control = stream.readUint2();
    int length = 0;
    for (int bit = 13; bit >= 8; --bit)
        length |= stream.readBit() << bit;
    length |= stream.readUint8();
    uint16_t delimiterValue = control | (length << 2);
    uint8_t crc = stream.readByte();
    uint8_t signature = stream.readByte();
    mpduSubframe->setEof((control & 1) != 0);
    mpduSubframe->setReserved((control >> 1) & 1);
    mpduSubframe->setLength(length);
    mpduSubframe->setDelimiterCrc(crc);
    mpduSubframe->setSignature(signature);
    if (signature != 0x4E || crc != computeDelimiterCrc(delimiterValue))
        mpduSubframe->markIncorrect();
    return mpduSubframe;
}

void Ieee80211MacHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    B startPos = stream.getLength();
    auto macHeader = dynamicPtrCast<const Ieee80211MacHeader>(chunk);
    stream.writeUint4(macHeader->getSubType());
    stream.writeUint2(macHeader->getFrameType());
    stream.writeUint2(macHeader->getProtocolVersion());
    stream.writeBit(macHeader->getOrder());
    stream.writeBit(macHeader->getProtectedFrame());
    stream.writeBit(macHeader->getMoreData());
    stream.writeBit(macHeader->getPowerMgmt());
    stream.writeBit(macHeader->getRetry());
    stream.writeBit(macHeader->getMoreFragments());
    stream.writeBit(macHeader->getFromDS());
    stream.writeBit(macHeader->getToDS());
    Ieee80211FrameType type = macHeader->getType();
    switch (type) {
        case ST_ASSOCIATIONREQUEST:
        case ST_ASSOCIATIONRESPONSE:
        case ST_REASSOCIATIONREQUEST:
        case ST_REASSOCIATIONRESPONSE:
        case ST_PROBEREQUEST:
        case ST_PROBERESPONSE:
        case ST_BEACON:
        case ST_ATIM:
        case ST_DISASSOCIATION:
        case ST_AUTHENTICATION:
        case ST_DEAUTHENTICATION:
        case ST_ACTION:
        case ST_NOACKACTION: {
            auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(chunk);
            stream.writeUint16Le(mgmtHeader->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(mgmtHeader->getReceiverAddress());
            stream.writeMacAddress(mgmtHeader->getTransmitterAddress());
            stream.writeMacAddress(mgmtHeader->getAddress3());
            stream.writeUint4(mgmtHeader->getFragmentNumber());
            stream.writeNBitsOfUint64Be(mgmtHeader->getSequenceNumber().get(), 12);
            if (mgmtHeader->getOrder())
                stream.writeUint32Be(0);
            if (type == ST_ACTION) {
                auto actionFrame = dynamicPtrCast<const Ieee80211ActionFrame>(chunk);
                switch (actionFrame->getCategory()) {
                    case 3: {
                        stream.writeByte(actionFrame->getCategory());
                        switch (actionFrame->getBlockAckAction()) {
                            case 0: {
                                auto addbaRequest = dynamicPtrCast<const Ieee80211AddbaRequest>(chunk);
                                stream.writeByte(addbaRequest->getBlockAckAction());
                                stream.writeByte(addbaRequest->getDialogToken());
                                stream.writeBit(addbaRequest->getAMsduSupported());
                                stream.writeBit(addbaRequest->getBlockAckPolicy());
                                stream.writeUint4(addbaRequest->getTid());
                                stream.writeNBitsOfUint64Be(addbaRequest->getBufferSize(), 10);
                                stream.writeUint16Be(addbaRequest->getBlockAckTimeoutValue().inUnit(SIMTIME_US) / 1024);
                                stream.writeUint4(addbaRequest->get_fragmentNumber());
                                stream.writeNBitsOfUint64Be(addbaRequest->getStartingSequenceNumber().get(), 12);
                                ASSERT(stream.getLength() - startPos == addbaRequest->getChunkLength());
                                break;
                            }
                            case 1: {
                                auto addbaResponse = dynamicPtrCast<const Ieee80211AddbaResponse>(chunk);
                                stream.writeByte(addbaResponse->getBlockAckAction());
                                stream.writeByte(addbaResponse->getDialogToken());
                                stream.writeUint16Be(addbaResponse->getStatusCode());
                                stream.writeBit(addbaResponse->getAMsduSupported());
                                stream.writeBit(addbaResponse->getBlockAckPolicy());
                                stream.writeUint4(addbaResponse->getTid());
                                stream.writeNBitsOfUint64Be(addbaResponse->getBufferSize(), 10);
                                stream.writeUint16Be(addbaResponse->getBlockAckTimeoutValue().inUnit(SIMTIME_US) / 1024);
                                ASSERT(stream.getLength() - startPos == addbaResponse->getChunkLength());
                                break;
                            }
                            case 2: {
                                auto delba = dynamicPtrCast<const Ieee80211Delba>(chunk);
                                stream.writeByte(delba->getBlockAckAction());
                                stream.writeNBitsOfUint64Be(delba->getReserved(), 11);
                                stream.writeBit(delba->getInitiator());
                                stream.writeUint4(delba->getTid());
                                stream.writeUint16Be(delba->getReasonCode());
                                ASSERT(stream.getLength() - startPos == delba->getChunkLength());
                                break;
                            }
                            default:
                                throw cRuntimeError("Ieee80211MacHeaderSerializer: cannot serialize the Ieee80211ActionFrame frame, blockAckAction %d not supported.", actionFrame->getBlockAckAction());
                        }
                        break;
                    }
                    case 22: {
                        stream.writeByte(actionFrame->getCategory());
                        stream.writeByte(actionFrame->getS1gAction());
                        if (auto twtSetup = dynamicPtrCast<const Ieee80211TwtSetupFrame>(chunk)) {
                            stream.writeByte(twtSetup->getDialogToken());
                            uint8_t control = (twtSetup->getTwtRequest() ? 1 : 0) |
                                    ((twtSetup->getSetupCommand() & 0x7) << 1) |
                                    (twtSetup->getTrigger() ? 0x10 : 0) |
                                    (twtSetup->getImplicit() ? 0x20 : 0) |
                                    (twtSetup->getAnnounced() ? 0x40 : 0) |
                                    (twtSetup->getBroadcast() ? 0x80 : 0);
                            stream.writeByte(control);
                            stream.writeByte((twtSetup->getFlowId() & 0x7) | ((twtSetup->getBroadcastId() & 0x1f) << 3));
                            stream.writeUint64Le(twtSetup->getTargetWakeTime());
                            stream.writeByte(twtSetup->getWakeIntervalExponent());
                            stream.writeUint16Le(twtSetup->getWakeIntervalMantissa());
                            stream.writeByte(twtSetup->getNominalWakeDuration());
                            stream.writeByte(twtSetup->getPersistence());
                            ASSERT(stream.getLength() - startPos == twtSetup->getChunkLength());
                        }
                        else if (auto teardown = dynamicPtrCast<const Ieee80211TwtTeardownFrame>(chunk)) {
                            stream.writeByte((teardown->getFlowId() & 0x7) | ((teardown->getBroadcastId() & 0x1f) << 3) |
                                    (teardown->getBroadcast() ? 0x80 : 0));
                            ASSERT(stream.getLength() - startPos == teardown->getChunkLength());
                        }
                        else if (auto information = dynamicPtrCast<const Ieee80211TwtInformationFrame>(chunk)) {
                            stream.writeByte(information->getFlowId() & 0x7);
                            stream.writeByte(information->getNextWakeTimePresent() ? 1 : 0);
                            stream.writeUint64Le(information->getNextWakeTime());
                            ASSERT(stream.getLength() - startPos == information->getChunkLength());
                        }
                        else
                            throw cRuntimeError("Unsupported S1G action frame");
                        break;
                    }
                    default:
                        throw cRuntimeError("Ieee80211MacHeaderSerializer: cannot serialize the Ieee80211ActionFrame frame, category %d not supported.", actionFrame->getCategory());
                }
                break;
            }
            else
                ASSERT(stream.getLength() - startPos == mgmtHeader->getChunkLength());
            break;
        }
        case ST_RTS: {
            auto rtsFrame = dynamicPtrCast<const Ieee80211RtsFrame>(chunk);
            stream.writeUint16Le(rtsFrame->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(rtsFrame->getReceiverAddress());
            stream.writeMacAddress(rtsFrame->getTransmitterAddress());
            ASSERT(stream.getLength() - startPos == rtsFrame->getChunkLength());
            break;
        }
        case ST_CTS: {
            auto ctsFrame = dynamicPtrCast<const Ieee80211CtsFrame>(chunk);
            stream.writeUint16Le(ctsFrame->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(ctsFrame->getReceiverAddress());
            ASSERT(stream.getLength() - startPos == ctsFrame->getChunkLength());
            break;
        }
        case ST_ACK: {
            auto ackFrame = dynamicPtrCast<const Ieee80211AckFrame>(chunk);
            stream.writeUint16Le(ackFrame->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(ackFrame->getReceiverAddress());
            ASSERT(stream.getLength() - startPos == ackFrame->getChunkLength());
            break;
        }
        case ST_TRIGGER: {
            auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(chunk);
            if (trigger->getUsersArraySize() > 255)
                throw cRuntimeError("Too many Trigger frame users");

            // Duration and MAC addresses (standard MAC header fields)
            stream.writeUint16Le(trigger->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(trigger->getReceiverAddress());
            stream.writeMacAddress(trigger->getTransmitterAddress());

            // --- Common Info Field (8 octets = 64 bits) - IEEE Std 802.11-2024 9.3.1.22.1 ---
            // B0–B3: Trigger Type (4 bits)
            stream.writeNBitsOfUint64Be(trigger->getTriggerType(), 4);

            // B4–B15: UL Length (12 bits) - L-SIG LENGTH field of the response, mapped to commonDuration in us
            uint32_t ulLength = static_cast<uint32_t>(trigger->getCommonDuration().inUnit(SIMTIME_US));
            stream.writeNBitsOfUint64Be(ulLength & 0xFFF, 12);

            // B16: More TF (1 bit) - 0
            stream.writeBit(false);

            // B17: CS Required (1 bit) - 1
            stream.writeBit(true);

            // B18-B19: UL BW (2 bits) - 0: 20MHz, 1: 40MHz, 2: 80MHz, 3: 160MHz
            uint32_t maxToneIndex = 0;
            for (unsigned int i = 0; i < trigger->getUsersArraySize(); ++i) {
                const auto& u = trigger->getUsers(i);
                maxToneIndex = std::max(maxToneIndex, (uint32_t)(u.ruToneOffset + u.ruToneSize));
            }
            uint8_t ulBw = 0;
            if (maxToneIndex > 996)
                ulBw = 3;
            else if (maxToneIndex > 484)
                ulBw = 2;
            else if (maxToneIndex > 242)
                ulBw = 1;
            stream.writeNBitsOfUint64Be(ulBw, 2);

            // B20–B21: GI And HE-LTF Type (2 bits)
            uint8_t giAndHeLtf = (trigger->getGuardInterval() <= 2) ? trigger->getGuardInterval() : 2;
            stream.writeNBitsOfUint64Be(giAndHeLtf, 2);

            // B22: MU-MIMO HE-LTF Mode (1 bit)
            stream.writeBit(false);

            // B23–B25: Number Of HE-LTF Symbols And Midamble Periodicity (3 bits)
            stream.writeNBitsOfUint64Be(0, 3);

            // B26: UL STBC (1 bit)
            stream.writeBit(false);

            // B27: LDPC Extra Symbol Segment (1 bit)
            stream.writeBit(trigger->getCoding() & 1);

            // Standard Trigger Common Info tail. triggerId is simulator correlation
            // metadata and is carried by HE MU PHY tags/headers, not by these MAC bits.
            stream.writeNBitsOfUint64Be(0, 6); // AP Tx Power
            stream.writeNBitsOfUint64Be(0, 2); // Pre-FEC padding factor
            stream.writeBit(false); // PE Disambiguity
            stream.writeNBitsOfUint64Be(0, 16); // UL Spatial Reuse
            stream.writeBit(false); // Doppler
            stream.writeNBitsOfUint64Be(0, 9); // UL HE-SIG-A2 Reserved
            stream.writeBit(false); // Reserved bit B63

            // --- User Info List ---
            for (unsigned int i = 0; i < trigger->getUsersArraySize(); i++) {
                const auto& user = trigger->getUsers(i);

                // B0–B11: AID12 (12 bits)
                stream.writeNBitsOfUint64Be(user.aid, 12);

                // B12–B19: RU Allocation (8 bits) - mapped using Table 9-53
                Hz bw = (ulBw == 3) ? Hz(160e6) : ((ulBw == 2) ? Hz(80e6) : ((ulBw == 1) ? Hz(40e6) : Hz(20e6)));
                uint8_t ruAllocation = 0;
                uint8_t b0 = 0;
                uint8_t b7_b1 = 0;
                if (ulBw == 3) {
                    if (user.ruToneSize == 1992) {
                        b7_b1 = 68;
                        b0 = 0;
                    } else if (user.ruToneOffset >= 996) {
                        b0 = 1;
                        auto catalog = physicallayer::getHeRuAllocationCatalog(Hz(0), Hz(80e6));
                        std::vector<physicallayer::Ieee80211HeRu> filtered;
                        for (const auto& r : catalog) {
                            if (r.toneSize == user.ruToneSize)
                                filtered.push_back(r);
                        }
                        std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) { return a.toneOffset < b.toneOffset; });
                        int idx = -1;
                        int targetOffset = user.ruToneOffset - 996;
                        for (size_t k = 0; k < filtered.size(); ++k) {
                            if (filtered[k].toneOffset == targetOffset) {
                                idx = k;
                                break;
                            }
                        }
                        if (idx != -1) {
                            if (user.ruToneSize == 26) b7_b1 = idx;
                            else if (user.ruToneSize == 52) b7_b1 = 37 + idx;
                            else if (user.ruToneSize == 106) b7_b1 = 53 + idx;
                            else if (user.ruToneSize == 242) b7_b1 = 61 + idx;
                            else if (user.ruToneSize == 484) b7_b1 = 65 + idx;
                            else if (user.ruToneSize == 996) b7_b1 = 67;
                        }
                    } else {
                        b0 = 0;
                        auto catalog = physicallayer::getHeRuAllocationCatalog(Hz(0), Hz(80e6));
                        std::vector<physicallayer::Ieee80211HeRu> filtered;
                        for (const auto& r : catalog) {
                            if (r.toneSize == user.ruToneSize)
                                filtered.push_back(r);
                        }
                        std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) { return a.toneOffset < b.toneOffset; });
                        int idx = -1;
                        for (size_t k = 0; k < filtered.size(); ++k) {
                            if (filtered[k].toneOffset == user.ruToneOffset) {
                                idx = k;
                                break;
                            }
                        }
                        if (idx != -1) {
                            if (user.ruToneSize == 26) b7_b1 = idx;
                            else if (user.ruToneSize == 52) b7_b1 = 37 + idx;
                            else if (user.ruToneSize == 106) b7_b1 = 53 + idx;
                            else if (user.ruToneSize == 242) b7_b1 = 61 + idx;
                            else if (user.ruToneSize == 484) b7_b1 = 65 + idx;
                            else if (user.ruToneSize == 996) b7_b1 = 67;
                        }
                    }
                } else {
                    b0 = 0;
                    auto catalog = physicallayer::getHeRuAllocationCatalog(Hz(0), bw);
                    std::vector<physicallayer::Ieee80211HeRu> filtered;
                    for (const auto& r : catalog) {
                        if (r.toneSize == user.ruToneSize)
                            filtered.push_back(r);
                    }
                    std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) { return a.toneOffset < b.toneOffset; });
                    int idx = -1;
                    for (size_t k = 0; k < filtered.size(); ++k) {
                        if (filtered[k].toneOffset == user.ruToneOffset) {
                            idx = k;
                            break;
                        }
                    }
                    if (idx != -1) {
                        if (user.ruToneSize == 26) b7_b1 = idx;
                        else if (user.ruToneSize == 52) b7_b1 = 37 + idx;
                        else if (user.ruToneSize == 106) b7_b1 = 53 + idx;
                        else if (user.ruToneSize == 242) b7_b1 = 61 + idx;
                        else if (user.ruToneSize == 484) b7_b1 = 65 + idx;
                        else if (user.ruToneSize == 996) b7_b1 = 67;
                    }
                }
                ruAllocation = (b7_b1 << 1) | b0;
                stream.writeNBitsOfUint64Be(ruAllocation, 8);

                // B20: UL FEC Coding Type (1 bit)
                stream.writeBit(trigger->getCoding() & 1);

                // B21–B24: UL HE-MCS (4 bits)
                stream.writeNBitsOfUint64Be(user.mcs, 4);

                // B25: UL DCM (1 bit) - 0
                stream.writeBit(false);

                // B26–B31: SS Allocation (6 bits) - starting stream = 0 (i.e. SS=1), number of SS = 0 (i.e. NSS=1)
                stream.writeNBitsOfUint64Be(0, 6);

                // B32–B38: UL Target Receive Power (7 bits) - PTARGET mapped to user.targetRssiDbm + 110
                int fval = user.targetRssiDbm + 110;
                if (fval < 0) fval = 0;
                if (fval > 127) fval = 127;
                stream.writeNBitsOfUint64Be(fval, 7);

                // B39: Reserved (1 bit) - 0
                stream.writeBit(false);

                // --- Trigger Dependent User Info ---
                if (trigger->getTriggerType() == 0) { // Basic Trigger: 1 octet (8 bits)
                    // TID Aggregation Limit (B2-B4) packs user.tid
                    uint8_t tidAggregationLimit = user.tid & 0x7;
                    uint8_t basicUserInfo = (tidAggregationLimit << 2);
                    stream.writeByte(basicUserInfo);
                }
            }
            ASSERT(stream.getLength() - startPos == trigger->getChunkLength());
            break;
        }
        case ST_BLOCKACK_REQ: {
            auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(chunk);
            stream.writeUint16Le(blockAckReq->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(blockAckReq->getReceiverAddress());
            stream.writeMacAddress(blockAckReq->getTransmitterAddress());
            stream.writeBit(blockAckReq->getBarAckPolicy());
            bool multiTid = blockAckReq->getMultiTid();
            bool compressedBitmap = blockAckReq->getCompressedBitmap();
            stream.writeBit(multiTid);
            stream.writeBit(compressedBitmap);
            stream.writeNBitsOfUint64Be(blockAckReq->getReserved(), 9);
            if (!multiTid && !compressedBitmap) {
                auto basicBlockAckReq = dynamicPtrCast<const Ieee80211BasicBlockAckReq>(chunk);
                stream.writeUint4(basicBlockAckReq->getTidInfo());
                stream.writeUint32Be(basicBlockAckReq->getFragmentNumber());
                stream.writeUint64Be(0);
                stream.writeUint64Be(basicBlockAckReq->getStartingSequenceNumber().get());
                ASSERT(stream.getLength() - startPos == basicBlockAckReq->getChunkLength());
            }
            else if (!multiTid && compressedBitmap) {
                auto compressedBlockAckReq = dynamicPtrCast<const Ieee80211CompressedBlockAckReq>(chunk);
                stream.writeUint4(compressedBlockAckReq->getTidInfo());
                stream.writeUint32Be(compressedBlockAckReq->getFragmentNumber());
                stream.writeUint64Be(0);
                stream.writeUint64Be(compressedBlockAckReq->getStartingSequenceNumber().get());
                ASSERT(stream.getLength() - startPos == compressedBlockAckReq->getChunkLength());
            }
            else if (multiTid && compressedBitmap) {
                auto multiTidReq = dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(chunk);
                if (multiTidReq->getRecordsArraySize() == 0 || multiTidReq->getRecordsArraySize() > 16)
                    throw cRuntimeError("Multi-TID BlockAckReq must contain 1..16 records");
                stream.writeUint4(multiTidReq->getRecordsArraySize() - 1);
                for (unsigned int i = 0; i < multiTidReq->getRecordsArraySize(); ++i) {
                    const auto& rec = multiTidReq->getRecords(i);
                    stream.writeNBitsOfUint64Be(0, 12);
                    stream.writeUint4(rec.tid & 0xF);
                    stream.writeNBitsOfUint64Be(0, 4);
                    stream.writeNBitsOfUint64Be(rec.startingSequenceNumber & 0xFFF, 12);
                }
                ASSERT(stream.getLength() - startPos == multiTidReq->getChunkLength());
            }
            else
                throw cRuntimeError("Ieee80211MacHeaderSerializer: cannot serialize the frame, multiTid = 1 && compressedBitmap = 0 is reserved.");
            break;
        }
        case ST_BLOCKACK: {
            auto blockAck = dynamicPtrCast<const Ieee80211BlockAck>(chunk);
            stream.writeUint16Le(blockAck->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(blockAck->getReceiverAddress());
            stream.writeMacAddress(blockAck->getTransmitterAddress());
            stream.writeBit(blockAck->getBlockAckPolicy());
            bool multiTid = blockAck->getMultiTid();
            bool compressedBitmap = blockAck->getCompressedBitmap();
            stream.writeBit(multiTid);
            stream.writeBit(compressedBitmap);
            stream.writeNBitsOfUint64Be(blockAck->getReserved(), 9);
            if (!multiTid && !compressedBitmap) {
                auto basicBlockAck = dynamicPtrCast<const Ieee80211BasicBlockAck>(chunk);
                stream.writeUint4(basicBlockAck->getTidInfo());
                stream.writeUint16Be(basicBlockAck->getStartingSequenceNumber().get());
                for (size_t i = 0; i < 64; ++i) {
                    stream.writeByte(basicBlockAck->getBlockAckBitmap(i).getBytes()[0]);
                    stream.writeByte(basicBlockAck->getBlockAckBitmap(i).getBytes()[1]);
                }
                ASSERT(stream.getLength() - startPos == basicBlockAck->getChunkLength());
            }
            else if (!multiTid && compressedBitmap) {
                auto compressedBlockAck = dynamicPtrCast<const Ieee80211CompressedBlockAck>(chunk);
                stream.writeUint4(compressedBlockAck->getTidInfo());
                stream.writeUint16Be(compressedBlockAck->getStartingSequenceNumber().get());
                for (size_t i = 0; i < 8; ++i) {
                    stream.writeByte(compressedBlockAck->getBlockAckBitmap().getBytes()[i]);
                }
                ASSERT(stream.getLength() - startPos == compressedBlockAck->getChunkLength());
            }
            else if (multiTid && compressedBitmap) {
                auto multiTidAck = dynamicPtrCast<const Ieee80211MultiTidBlockAck>(chunk);
                if (multiTidAck->getRecordsArraySize() == 0 || multiTidAck->getRecordsArraySize() > 16)
                    throw cRuntimeError("Multi-TID BlockAck must contain 1..16 records");
                stream.writeUint4(multiTidAck->getRecordsArraySize() - 1);
                for (unsigned int i = 0; i < multiTidAck->getRecordsArraySize(); ++i) {
                    const auto& rec = multiTidAck->getRecords(i);
                    stream.writeNBitsOfUint64Be(0, 12);
                    stream.writeUint4(rec.tid & 0xF);
                    stream.writeNBitsOfUint64Be(0, 4);
                    stream.writeNBitsOfUint64Be(rec.startingSequenceNumber & 0xFFF, 12);
                    stream.writeUint64Be(rec.bitmap);
                }
                ASSERT(stream.getLength() - startPos == multiTidAck->getChunkLength());
            }
            else if (multiTid && !compressedBitmap) {
                auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(chunk);
                if (multiStaBlockAck == nullptr)
                    throw cRuntimeError("Unsupported multi-STA Block Ack representation");
                if (multiStaBlockAck->getRecordsArraySize() > 255)
                    throw cRuntimeError("Too many Multi-STA Block Ack records");
                stream.writeUint4(0);
                stream.writeByte(multiStaBlockAck->getRecordsArraySize());
                for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); i++) {
                    const auto& record = multiStaBlockAck->getRecords(i);
                    stream.writeUint16Be(record.aid);
                    stream.writeByte(record.tid);
                    stream.writeUint16Be(record.startingSequenceNumber);
                    stream.writeUint64Be(record.bitmap);
                    stream.writeBit(record.responseReceived);
                    stream.writeNBitsOfUint64Be(0, 7);
                }
                ASSERT(stream.getLength() - startPos == multiStaBlockAck->getChunkLength());
            }
            else {
                throw cRuntimeError("Ieee80211MacHeaderSerializer: cannot serialize the frame, multiTid = 1 && compressedBitmap = 0 is reserved.");
            }
            break;

        }
        case ST_QOS_NULL:
        case ST_DATA_WITH_QOS:
        case ST_DATA: {
            auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(chunk);
            stream.writeUint16Le(dataHeader->getDurationField().inUnit(SIMTIME_US));
            stream.writeMacAddress(dataHeader->getReceiverAddress());
            stream.writeMacAddress(dataHeader->getTransmitterAddress());
            stream.writeMacAddress(dataHeader->getAddress3());
            stream.writeUint4(dataHeader->getFragmentNumber());
            stream.writeNBitsOfUint64Be(dataHeader->getSequenceNumber().get(), 12);
            if (dataHeader->getFromDS() && dataHeader->getToDS())
                stream.writeMacAddress(dataHeader->getAddress4());
            if (type == ST_DATA_WITH_QOS || type == ST_QOS_NULL) {
                stream.writeUint4(dataHeader->getTid());
                stream.writeBit(true);
                stream.writeUint2(dataHeader->getAckPolicy());
                stream.writeBit(dataHeader->getAMsduPresent());
                stream.writeByte(0);
                if (dataHeader->getBufferStatusPresent()) {
                    stream.writeUint4(3);
                    stream.writeUint4(dataHeader->getBufferStatusTid());
                    stream.writeUint2(dataHeader->getBufferStatusAc());
                    stream.writeNBitsOfUint64Be(std::min<uint32_t>(dataHeader->getBufferStatusQueueSize(), 0x3fffff), 22);
                }
            }
            ASSERT(stream.getLength() - startPos == dataHeader->getChunkLength());
            break;
        }
        case ST_PSPOLL:
        {
            auto psPoll = dynamicPtrCast<const Ieee80211PsPollFrame>(chunk);
            if (psPoll == nullptr)
                throw cRuntimeError("Ieee80211Serializer: PS-Poll must use Ieee80211PsPollFrame");
            stream.writeUint16Le(psPoll->getAID());
            stream.writeMacAddress(psPoll->getReceiverAddress());
            stream.writeMacAddress(psPoll->getTransmitterAddress());
            ASSERT(stream.getLength() - startPos == psPoll->getChunkLength());
            break;
        }
        case ST_LBMS_REQUEST:
        case ST_LBMS_REPORT: {
            break;
        }
        default:
            throw cRuntimeError("Ieee80211Serializer: cannot serialize the frame, type %d not supported.", type);
    }
}

const Ptr<Chunk> Ieee80211MacHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto macHeader = makeShared<Ieee80211MacHeader>();
    uint8_t subType = stream.readUint4();
    uint8_t frameType = stream.readUint2();
    uint8_t protocolVersion = stream.readUint2();
    macHeader->setType(protocolVersion, frameType, subType);
    bool order = stream.readBit();
    macHeader->setOrder(order);
    macHeader->setProtectedFrame(stream.readBit());
    macHeader->setMoreData(stream.readBit());
    macHeader->setPowerMgmt(stream.readBit());
    macHeader->setRetry(stream.readBit());
    macHeader->setMoreFragments(stream.readBit());
    macHeader->setFromDS(stream.readBit());
    macHeader->setToDS(stream.readBit());
    Ieee80211FrameType type = macHeader->getType();
    switch (type) {
        case ST_ASSOCIATIONREQUEST:
        case ST_ASSOCIATIONRESPONSE:
        case ST_REASSOCIATIONREQUEST:
        case ST_REASSOCIATIONRESPONSE:
        case ST_PROBEREQUEST:
        case ST_PROBERESPONSE:
        case ST_BEACON:
        case ST_ATIM:
        case ST_DISASSOCIATION:
        case ST_AUTHENTICATION:
        case ST_DEAUTHENTICATION:
        case ST_NOACKACTION: {
            auto mgmtHeader = makeShared<Ieee80211MgmtHeader>();
            copyBasicFields(mgmtHeader, macHeader);
            mgmtHeader->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            mgmtHeader->setReceiverAddress(stream.readMacAddress());
            mgmtHeader->setTransmitterAddress(stream.readMacAddress());
            mgmtHeader->setAddress3(stream.readMacAddress());
            mgmtHeader->setFragmentNumber(stream.readUint4());
            mgmtHeader->setSequenceNumber(SequenceNumberCyclic(stream.readNBitsToUint64Be(12)));
            if (order)
                stream.readUint32Be();
            return mgmtHeader;
        }
        case ST_ACTION: {
            auto actionFrame = makeShared<Ieee80211ActionFrame>();
            copyBasicFields(actionFrame, macHeader);
            actionFrame->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            actionFrame->setReceiverAddress(stream.readMacAddress());
            actionFrame->setTransmitterAddress(stream.readMacAddress());
            actionFrame->setAddress3(stream.readMacAddress());
            actionFrame->setFragmentNumber(stream.readUint4());
            actionFrame->setSequenceNumber(SequenceNumberCyclic(stream.readNBitsToUint64Be(12)));
            if (order)
                stream.readUint32Be();
            actionFrame->setCategory(stream.readByte());
            switch (actionFrame->getCategory()) {
                case 3: {
                    uint8_t blockAckAction = stream.readByte();
                    switch (blockAckAction) {
                        case 0: {
                            auto addbaRequest = makeShared<Ieee80211AddbaRequest>();
                            copyBasicFields(addbaRequest, macHeader);
                            copyActionFrameFields(addbaRequest, actionFrame);
                            addbaRequest->setBlockAckAction(blockAckAction);
                            addbaRequest->setDialogToken(stream.readByte());
                            addbaRequest->setAMsduSupported(stream.readBit());
                            addbaRequest->setBlockAckPolicy(stream.readBit());
                            addbaRequest->setTid(stream.readUint4());
                            addbaRequest->setBufferSize(stream.readNBitsToUint64Be(10));
                            addbaRequest->setBlockAckTimeoutValue(SimTime(stream.readUint16Be() * 1024, SIMTIME_US));
                            addbaRequest->set_fragmentNumber(stream.readUint4());
                            addbaRequest->setStartingSequenceNumber(SequenceNumberCyclic(stream.readNBitsToUint64Be(12)));
                            return addbaRequest;
                        }
                        case 1: {
                            auto addbaResponse = makeShared<Ieee80211AddbaResponse>();
                            copyBasicFields(addbaResponse, macHeader);
                            copyActionFrameFields(addbaResponse, actionFrame);
                            addbaResponse->setBlockAckAction(blockAckAction);
                            addbaResponse->setDialogToken(stream.readByte());
                            addbaResponse->setStatusCode(stream.readUint16Be());
                            addbaResponse->setAMsduSupported(stream.readBit());
                            addbaResponse->setBlockAckPolicy(stream.readBit());
                            addbaResponse->setTid(stream.readUint4());
                            addbaResponse->setBufferSize(stream.readNBitsToUint64Be(10));
                            addbaResponse->setBlockAckTimeoutValue(SimTime(stream.readUint16Be() * 1024, SIMTIME_US));
                            return addbaResponse;
                        }
                        case 2: {
                            auto delba = makeShared<Ieee80211Delba>();
                            copyBasicFields(delba, macHeader);
                            delba->setBlockAckAction(blockAckAction);
                            delba->setReserved(stream.readNBitsToUint64Be(11));
                            delba->setInitiator(stream.readBit());
                            delba->setTid(stream.readUint4());
                            delba->setReasonCode(stream.readUint16Be());
                            return delba;
                        }
                        default:
                            actionFrame->markIncorrect();
                            return actionFrame;
                    }
                    break;
                }
                case 22: {
                    uint8_t s1gAction = stream.readByte();
                    if (s1gAction == 6) {
                        auto frame = makeShared<Ieee80211TwtSetupFrame>();
                        copyBasicFields(frame, macHeader);
                        copyActionFrameFields(frame, actionFrame);
                        frame->setS1gAction(s1gAction);
                        frame->setDialogToken(stream.readByte());
                        uint8_t control = stream.readByte();
                        uint8_t flow = stream.readByte();
                        frame->setTwtRequest(control & 1);
                        frame->setSetupCommand((control >> 1) & 0x7);
                        frame->setTrigger(control & 0x10);
                        frame->setImplicit(control & 0x20);
                        frame->setAnnounced(control & 0x40);
                        frame->setBroadcast(control & 0x80);
                        frame->setFlowId(flow & 0x7);
                        frame->setBroadcastId((flow >> 3) & 0x1f);
                        frame->setTargetWakeTime(stream.readUint64Le());
                        frame->setWakeIntervalExponent(stream.readByte());
                        frame->setWakeIntervalMantissa(stream.readUint16Le());
                        frame->setNominalWakeDuration(stream.readByte());
                        frame->setPersistence(stream.readByte());
                        return frame;
                    }
                    else if (s1gAction == 7) {
                        auto frame = makeShared<Ieee80211TwtTeardownFrame>();
                        copyBasicFields(frame, macHeader);
                        copyActionFrameFields(frame, actionFrame);
                        frame->setS1gAction(s1gAction);
                        uint8_t flow = stream.readByte();
                        frame->setFlowId(flow & 0x7);
                        frame->setBroadcastId((flow >> 3) & 0x1f);
                        frame->setBroadcast(flow & 0x80);
                        return frame;
                    }
                    else if (s1gAction == 11) {
                        auto frame = makeShared<Ieee80211TwtInformationFrame>();
                        copyBasicFields(frame, macHeader);
                        copyActionFrameFields(frame, actionFrame);
                        frame->setS1gAction(s1gAction);
                        frame->setFlowId(stream.readByte() & 0x7);
                        frame->setNextWakeTimePresent(stream.readByte() != 0);
                        frame->setNextWakeTime(stream.readUint64Le());
                        return frame;
                    }
                    actionFrame->markIncorrect();
                    return actionFrame;
                }
                default: {
                    actionFrame->markIncorrect();
                    return actionFrame;
                }
            }
        }
        case ST_RTS: {
            auto rtsFrame = makeShared<Ieee80211RtsFrame>();
            copyBasicFields(rtsFrame, macHeader);
            rtsFrame->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            rtsFrame->setReceiverAddress(stream.readMacAddress());
            rtsFrame->setTransmitterAddress(stream.readMacAddress());
            return rtsFrame;
        }
        case ST_CTS: {
            auto ctsFrame = makeShared<Ieee80211CtsFrame>();
            copyBasicFields(ctsFrame, macHeader);
            ctsFrame->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            ctsFrame->setReceiverAddress(stream.readMacAddress());
            return ctsFrame;
        }
        case ST_ACK: {
            auto ackFrame = makeShared<Ieee80211AckFrame>();
            copyBasicFields(ackFrame, macHeader);
            ackFrame->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            ackFrame->setReceiverAddress(stream.readMacAddress());
            return ackFrame;
        }
        case ST_TRIGGER: {
            auto trigger = makeShared<Ieee80211TriggerFrame>();
            copyBasicFields(trigger, macHeader);
            trigger->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            trigger->setReceiverAddress(stream.readMacAddress());
            trigger->setTransmitterAddress(stream.readMacAddress());

            // --- Common Info Field (8 octets = 64 bits) ---
            auto triggerType = stream.readNBitsToUint64Be(4);
            trigger->setTriggerType(triggerType);

            auto ulLength = stream.readNBitsToUint64Be(12);
            trigger->setCommonDuration(SimTime(ulLength, SIMTIME_US));

            stream.readBit(); // More TF
            stream.readBit(); // CS Required

            auto ulBw = stream.readNBitsToUint64Be(2);

            auto giAndHeLtf = stream.readNBitsToUint64Be(2);
            trigger->setGuardInterval(giAndHeLtf);

            stream.readBit(); // MU-MIMO HE-LTF Mode
            stream.readNBitsToUint64Be(3); // Number Of HE-LTF Symbols And Midamble Periodicity
            stream.readBit(); // UL STBC
            auto coding = stream.readBit();
            trigger->setCoding(coding);

            stream.readNBitsToUint64Be(6); // AP Tx Power
            stream.readNBitsToUint64Be(2); // Pre-FEC padding factor
            stream.readBit(); // PE Disambiguity
            stream.readNBitsToUint64Be(16); // UL Spatial Reuse
            stream.readBit(); // Doppler
            stream.readNBitsToUint64Be(9); // UL HE-SIG-A2 Reserved
            stream.readBit(); // Reserved bit B63
            trigger->setTriggerId(0);

            // Determine number of users from remaining chunk length
            int remainingBytes = trigger->getChunkLength().get<B>() - 24;
            int userInfoSize = (triggerType == 4) ? 5 : 6;
            int count = 0;
            if (remainingBytes > 0 && userInfoSize > 0) {
                count = remainingBytes / userInfoSize;
            }
            trigger->setUsersArraySize(count);

            for (unsigned int i = 0; i < (unsigned int)count; i++) {
                Ieee80211HeTriggerUserInfo user;
                user.aid = stream.readNBitsToUint64Be(12);

                auto ruAllocation = stream.readNBitsToUint64Be(8);
                uint8_t b0 = ruAllocation & 1;
                uint8_t b7_b1 = (ruAllocation >> 1) & 0x7F;

                Hz bw = (ulBw == 3) ? Hz(160e6) : ((ulBw == 2) ? Hz(80e6) : ((ulBw == 1) ? Hz(40e6) : Hz(20e6)));
                Hz eff_bandwidth = bw;
                if (bw == Hz(160e6)) {
                    if (b7_b1 == 68) {
                        user.ruToneSize = 1992;
                        user.ruToneOffset = 0;
                    } else {
                        eff_bandwidth = Hz(80e6);
                    }
                }

                if (bw != Hz(160e6) || b7_b1 != 68) {
                    int toneSize = 0;
                    int idx = 0;
                    if (b7_b1 >= 0 && b7_b1 <= 36) {
                        toneSize = 26;
                        idx = b7_b1;
                    } else if (b7_b1 >= 37 && b7_b1 <= 52) {
                        toneSize = 52;
                        idx = b7_b1 - 37;
                    } else if (b7_b1 >= 53 && b7_b1 <= 60) {
                        toneSize = 106;
                        idx = b7_b1 - 53;
                    } else if (b7_b1 >= 61 && b7_b1 <= 64) {
                        toneSize = 242;
                        idx = b7_b1 - 61;
                    } else if (b7_b1 >= 65 && b7_b1 <= 66) {
                        toneSize = 484;
                        idx = b7_b1 - 65;
                    } else if (b7_b1 == 67) {
                        toneSize = 996;
                        idx = 0;
                    } else if (b7_b1 == 68) {
                        toneSize = 1992;
                        idx = 0;
                    }

                    auto catalog = physicallayer::getHeRuAllocationCatalog(Hz(0), eff_bandwidth);
                    std::vector<physicallayer::Ieee80211HeRu> filtered;
                    for (const auto& r : catalog) {
                        if (r.toneSize == toneSize)
                            filtered.push_back(r);
                    }
                    std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) { return a.toneOffset < b.toneOffset; });
                    if (idx >= 0 && idx < (int)filtered.size()) {
                        user.ruToneSize = toneSize;
                        user.ruToneOffset = filtered[idx].toneOffset;
                        if (bw == Hz(160e6) && b0 == 1) {
                            user.ruToneOffset += 996;
                        }
                    }
                }

                auto catalog = physicallayer::getHeRuAllocationCatalog(Hz(0), bw);
                for (const auto& r : catalog) {
                    if (r.toneSize == user.ruToneSize && r.toneOffset == user.ruToneOffset) {
                        user.ruIndex = r.index;
                        break;
                    }
                }

                stream.readBit(); // UL FEC Coding Type
                user.mcs = stream.readNBitsToUint64Be(4);
                stream.readBit(); // UL DCM
                stream.readNBitsToUint64Be(6); // SS Allocation

                auto targetRssiVal = stream.readNBitsToUint64Be(7);
                user.targetRssiDbm = static_cast<int8_t>(targetRssiVal - 110);
                user.randomAccess = (user.aid == 0 || user.aid == 2045);

                stream.readBit(); // Reserved B39

                if (triggerType == 0) { // Basic Trigger
                    auto basicUserInfo = stream.readByte();
                    user.tid = (basicUserInfo >> 2) & 0x7;
                } else {
                    user.tid = 0;
                }

                trigger->setUsers(i, user);
            }
            return trigger;
        }
        case ST_BLOCKACK_REQ: {
            auto blockAckReq = makeShared<Ieee80211BlockAckReq>();
            copyBasicFields(blockAckReq, macHeader);
            blockAckReq->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            blockAckReq->setReceiverAddress(stream.readMacAddress());
            blockAckReq->setTransmitterAddress(stream.readMacAddress());
            blockAckReq->setBarAckPolicy(stream.readBit());
            bool multiTid = stream.readBit();
            bool compressedBitmap = stream.readBit();
            blockAckReq->setMultiTid(multiTid);
            blockAckReq->setCompressedBitmap(compressedBitmap);
            blockAckReq->setReserved(stream.readNBitsToUint64Be(9));
            auto tidInfo = stream.readUint4();
            if (!multiTid && !compressedBitmap) {
                auto basicBlockAckReq = makeShared<Ieee80211BasicBlockAckReq>();
                copyBasicFields(basicBlockAckReq, macHeader);
                copyBlockAckReqFrameFields(basicBlockAckReq, blockAckReq);
                basicBlockAckReq->setTidInfo(tidInfo);
                basicBlockAckReq->setFragmentNumber(stream.readUint32Be());
                stream.readUint64Be();
                basicBlockAckReq->setStartingSequenceNumber(SequenceNumberCyclic(stream.readUint64Be()));
                return basicBlockAckReq;
            }
            else if (!multiTid && compressedBitmap) {
                auto compressedBlockAckReq = makeShared<Ieee80211CompressedBlockAckReq>();
                copyBasicFields(compressedBlockAckReq, macHeader);
                copyBlockAckReqFrameFields(compressedBlockAckReq, blockAckReq);
                compressedBlockAckReq->setTidInfo(tidInfo);
                compressedBlockAckReq->setFragmentNumber(stream.readUint32Be());
                stream.readUint64Be();
                compressedBlockAckReq->setStartingSequenceNumber(SequenceNumberCyclic(stream.readUint64Be()));
                return compressedBlockAckReq;
            }
            else if (multiTid && compressedBitmap) {
                auto multiTidReq = makeShared<Ieee80211MultiTidBlockAckReq>();
                copyBasicFields(multiTidReq, macHeader);
                copyBlockAckReqFrameFields(multiTidReq, blockAckReq);
                auto count = tidInfo + 1;
                multiTidReq->setRecordsArraySize(count);
                for (unsigned int i = 0; i < count; ++i) {
                    Ieee80211MultiTidBlockAckReqRecord rec;
                    stream.readNBitsToUint64Be(12);
                    rec.tid = stream.readUint4();
                    stream.readNBitsToUint64Be(4);
                    rec.startingSequenceNumber = stream.readNBitsToUint64Be(12);
                    multiTidReq->setRecords(i, rec);
                }
                return multiTidReq;
            }
            else
                blockAckReq->markIncorrect();
            return blockAckReq;
        }
        case ST_BLOCKACK: {
            auto blockAck = makeShared<Ieee80211BlockAck>();
            copyBasicFields(blockAck, macHeader);
            blockAck->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            blockAck->setReceiverAddress(stream.readMacAddress());
            blockAck->setTransmitterAddress(stream.readMacAddress());
            blockAck->setBlockAckPolicy(stream.readBit());
            bool multiTid = stream.readBit();
            bool compressedBitmap = stream.readBit();
            blockAck->setMultiTid(multiTid);
            blockAck->setCompressedBitmap(compressedBitmap);
            blockAck->setReserved(stream.readNBitsToUint64Be(9));
            auto tidInfo = stream.readUint4();
            if (!multiTid && !compressedBitmap) {
                auto basicBlockAck = makeShared<Ieee80211BasicBlockAck>();
                copyBasicFields(basicBlockAck, macHeader);
                copyBlockAckFrameFields(basicBlockAck, blockAck);
                basicBlockAck->setTidInfo(tidInfo);
                basicBlockAck->setStartingSequenceNumber(SequenceNumberCyclic(stream.readUint16Be()));
                for (size_t i = 0; i < 64; ++i) {
                    std::vector<uint8_t> bytes;
                    bytes.push_back(stream.readByte());
                    bytes.push_back(stream.readByte());
                    BitVector *blockAckBitmap = new BitVector(bytes);
                    basicBlockAck->setBlockAckBitmap(i, *blockAckBitmap);
                }
                return basicBlockAck;
            }
            else if (!multiTid && compressedBitmap) {
                auto compressedBlockAck = makeShared<Ieee80211CompressedBlockAck>();
                copyBasicFields(compressedBlockAck, macHeader);
                copyBlockAckFrameFields(compressedBlockAck, blockAck);

                compressedBlockAck->setTidInfo(tidInfo);
                compressedBlockAck->setStartingSequenceNumber(SequenceNumberCyclic(stream.readUint16Be()));
                std::vector<uint8_t> bytes;
                for (size_t i = 0; i < 8; ++i) {
                    bytes.push_back(stream.readByte());
                }
                compressedBlockAck->setBlockAckBitmap(*(new BitVector(bytes)));
                return compressedBlockAck;
            }
            else if (multiTid && !compressedBitmap) {
                auto multiStaBlockAck = makeShared<Ieee80211MultiStaBlockAck>();
                copyBasicFields(multiStaBlockAck, macHeader);
                copyBlockAckFrameFields(multiStaBlockAck, blockAck);
                auto count = stream.readByte();
                multiStaBlockAck->setRecordsArraySize(count);
                for (unsigned int i = 0; i < count; i++) {
                    Ieee80211MultiStaBlockAckRecord record;
                    record.aid = stream.readUint16Be();
                    record.tid = stream.readByte();
                    record.startingSequenceNumber = stream.readUint16Be();
                    record.bitmap = stream.readUint64Be();
                    record.responseReceived = stream.readBit();
                    stream.readNBitsToUint64Be(7);
                    multiStaBlockAck->setRecords(i, record);
                }
                return multiStaBlockAck;
            }
            else if (multiTid && compressedBitmap) {
                auto multiTidAck = makeShared<Ieee80211MultiTidBlockAck>();
                copyBasicFields(multiTidAck, macHeader);
                copyBlockAckFrameFields(multiTidAck, blockAck);
                auto count = tidInfo + 1;
                multiTidAck->setRecordsArraySize(count);
                for (unsigned int i = 0; i < count; ++i) {
                    Ieee80211MultiTidBlockAckRecord rec;
                    stream.readNBitsToUint64Be(12);
                    rec.tid = stream.readUint4();
                    stream.readNBitsToUint64Be(4);
                    rec.startingSequenceNumber = stream.readNBitsToUint64Be(12);
                    rec.bitmap = stream.readUint64Be();
                    multiTidAck->setRecords(i, rec);
                }
                return multiTidAck;
            }
            else {
                blockAck->markIncorrect();
                return blockAck;
            }
            return blockAck;

        }
        case ST_QOS_NULL:
        case ST_DATA_WITH_QOS:
        case ST_DATA: {
            auto dataHeader = makeShared<Ieee80211DataHeader>();
            copyBasicFields(dataHeader, macHeader);
            dataHeader->setDurationField(SimTime(stream.readUint16Le(), SIMTIME_US));
            dataHeader->setReceiverAddress(stream.readMacAddress());
            dataHeader->setTransmitterAddress(stream.readMacAddress());
            dataHeader->setAddress3(stream.readMacAddress());
            dataHeader->setFragmentNumber(stream.readUint4());
            dataHeader->setSequenceNumber(SequenceNumberCyclic(stream.readNBitsToUint64Be(12)));
            if (dataHeader->getFromDS() && dataHeader->getToDS())
                dataHeader->setAddress4(stream.readMacAddress());
            if (type == ST_DATA_WITH_QOS || type == ST_QOS_NULL) {
                dataHeader->setTid(stream.readUint4());
                stream.readBit();
                dataHeader->setAckPolicy(static_cast<AckPolicy>(stream.readUint2()));
                dataHeader->setAMsduPresent(stream.readBit());
                stream.readByte();
                if (order) {
                    auto controlId = stream.readUint4();
                    if (controlId == 3) {
                        dataHeader->setBufferStatusPresent(true);
                        dataHeader->setBufferStatusTid(stream.readUint4());
                        dataHeader->setBufferStatusAc(stream.readUint2());
                        dataHeader->setBufferStatusQueueSize(stream.readNBitsToUint64Be(22));
                    }
                    else {
                        stream.readNBitsToUint64Be(28);
                        dataHeader->markIncorrect();
                    }
                }
            }
            return dataHeader;
        }
        case ST_PSPOLL:
        {
            auto psPoll = makeShared<Ieee80211PsPollFrame>();
            copyBasicFields(psPoll, macHeader);
            psPoll->setAID(stream.readUint16Le() & 0x3fff);
            psPoll->setReceiverAddress(stream.readMacAddress());
            psPoll->setTransmitterAddress(stream.readMacAddress());
            return psPoll;
        }
        case ST_LBMS_REQUEST:
        case ST_LBMS_REPORT: {
            return macHeader;
        }
        default: {
            macHeader->markIncorrect();
            return macHeader;
        }
    }
}

void Ieee80211MacTrailerSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& macTrailer = dynamicPtrCast<const Ieee80211MacTrailer>(chunk);
    auto fcsMode = macTrailer->getFcsMode();
    if (fcsMode != FCS_COMPUTED)
        throw cRuntimeError("Cannot serialize Ieee80211FcsTrailer without properly computed FCS, try changing the value of the fcsMode parameter (e.g. in the Ieee80211Mac module)");
    stream.writeUint32Be(macTrailer->getFcs());
}

const Ptr<Chunk> Ieee80211MacTrailerSerializer::deserialize(MemoryInputStream& stream) const
{
    auto macTrailer = makeShared<Ieee80211MacTrailer>();
    auto fcs = stream.readUint32Be();
    macTrailer->setFcs(fcs);
    macTrailer->setFcsMode(FCS_COMPUTED);
    return macTrailer;
}

} // namespace ieee80211

} // namespace inet
