//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

namespace inet {

namespace  physicallayer {

Register_Serializer(Ieee80211FhssPhyHeader, Ieee80211FhssPhyHeaderSerializer);
Register_Serializer(Ieee80211IrPhyHeader, Ieee80211IrPhyHeaderSerializer);
Register_Serializer(Ieee80211DsssPhyHeader, Ieee80211DsssPhyHeaderSerializer);
Register_Serializer(Ieee80211HrDsssPhyHeader, Ieee80211HrDsssPhyHeaderSerializer);
Register_Serializer(Ieee80211OfdmPhyHeader, Ieee80211OfdmPhyHeaderSerializer);
Register_Serializer(Ieee80211ErpOfdmPhyHeader, Ieee80211ErpOfdmPhyHeaderSerializer);
Register_Serializer(Ieee80211HtPhyHeader, Ieee80211HtPhyHeaderSerializer);
Register_Serializer(Ieee80211VhtPhyHeader, Ieee80211VhtPhyHeaderSerializer);
Register_Serializer(Ieee80211HeMuPhyHeader, Ieee80211HeMuPhyHeaderSerializer);

/**
 * FHSS
 */
void Ieee80211FhssPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto fhssPhyHeader = dynamicPtrCast<const Ieee80211FhssPhyHeader>(chunk);
    stream.writeNBitsOfUint64Be(fhssPhyHeader->getPlw(), 12);
    stream.writeUint4(fhssPhyHeader->getPsf());
    stream.writeUint16Be(fhssPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211FhssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto fhssPhyHeader = makeShared<Ieee80211FhssPhyHeader>();
    fhssPhyHeader->setPlw(stream.readNBitsToUint64Be(12));
    fhssPhyHeader->setPsf(stream.readUint4());
    fhssPhyHeader->setFcs(stream.readUint16Be());
    fhssPhyHeader->setFcsMode(FCS_COMPUTED);
    return fhssPhyHeader;
}

/**
 * IR
 */
void Ieee80211IrPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto irPhyHeader = dynamicPtrCast<const Ieee80211IrPhyHeader>(chunk);
    stream.writeUint16Be(irPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211IrPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto irPhyHeader = makeShared<Ieee80211IrPhyHeader>();
    irPhyHeader->setFcs(stream.readUint16Be());
    irPhyHeader->setFcsMode(FCS_COMPUTED);
    return irPhyHeader;
}

/**
 * DSSS
 */
void Ieee80211DsssPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto dsssPhyHeader = dynamicPtrCast<const Ieee80211DsssPhyHeader>(chunk);
    stream.writeUint16Be(0);
    stream.writeByte(dsssPhyHeader->getSignal());
    stream.writeByte(dsssPhyHeader->getService());
    stream.writeUint16Be(dsssPhyHeader->getLengthField().get<B>());
}

const Ptr<Chunk> Ieee80211DsssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto dsssPhyHeader = makeShared<Ieee80211DsssPhyHeader>();
    stream.readUint16Be();
    dsssPhyHeader->setSignal(stream.readByte());
    dsssPhyHeader->setService(stream.readByte());
    dsssPhyHeader->setLengthField(B(stream.readUint16Be()));
    dsssPhyHeader->setFcsMode(FCS_COMPUTED);
    return dsssPhyHeader;
}

/**
 * HR/DSSS
 */
void Ieee80211HrDsssPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto hrDsssPhyHeader = dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(chunk);
    stream.writeUint16Be(0);
    stream.writeByte(hrDsssPhyHeader->getSignal());
    stream.writeByte(hrDsssPhyHeader->getService());
    stream.writeUint16Be(hrDsssPhyHeader->getLengthField().get<B>());
}

const Ptr<Chunk> Ieee80211HrDsssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto hrDsssPhyHeader = makeShared<Ieee80211HrDsssPhyHeader>();
    stream.readUint16Be();
    hrDsssPhyHeader->setSignal(stream.readByte());
    hrDsssPhyHeader->setService(stream.readByte());
    hrDsssPhyHeader->setLengthField(B(stream.readUint16Be()));
    hrDsssPhyHeader->setFcsMode(FCS_COMPUTED);
    return hrDsssPhyHeader;
}

/**
 * OFDM
 */
void Ieee80211OfdmPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto ofdmPhyHeader = dynamicPtrCast<const Ieee80211OfdmPhyHeader>(chunk);
    stream.writeUint4(ofdmPhyHeader->getRate());
    stream.writeBit(ofdmPhyHeader->getReserved());
    stream.writeNBitsOfUint64Be(ofdmPhyHeader->getLengthField().get<B>(), 12);
    stream.writeBit(ofdmPhyHeader->getParity());
    stream.writeNBitsOfUint64Be(ofdmPhyHeader->getTail(), 6);
    stream.writeUint16Be(ofdmPhyHeader->getService());
}

const Ptr<Chunk> Ieee80211OfdmPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto ofdmPhyHeader = makeShared<Ieee80211OfdmPhyHeader>();
    ofdmPhyHeader->setRate(stream.readUint4());
    ofdmPhyHeader->setReserved(stream.readBit());
    ofdmPhyHeader->setLengthField(B(stream.readNBitsToUint64Be(12)));
    ofdmPhyHeader->setParity(stream.readBit());
    ofdmPhyHeader->setTail(stream.readNBitsToUint64Be(6));
    ofdmPhyHeader->setService(stream.readUint16Be());
    return ofdmPhyHeader;
}

/**
 * ERP OFDM
 */
void Ieee80211ErpOfdmPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto erpOfdmPhyHeader = dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(chunk);
    stream.writeUint4(erpOfdmPhyHeader->getRate());
    stream.writeBit(erpOfdmPhyHeader->getReserved());
    stream.writeNBitsOfUint64Be(erpOfdmPhyHeader->getLengthField().get<B>(), 12);
    stream.writeBit(erpOfdmPhyHeader->getParity());
    stream.writeNBitsOfUint64Be(erpOfdmPhyHeader->getTail(), 6);
    stream.writeUint16Be(erpOfdmPhyHeader->getService());
}

const Ptr<Chunk> Ieee80211ErpOfdmPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto erpOfdmPhyHeader = makeShared<Ieee80211ErpOfdmPhyHeader>();
    erpOfdmPhyHeader->setRate(stream.readUint4());
    erpOfdmPhyHeader->setReserved(stream.readBit());
    erpOfdmPhyHeader->setLengthField(B(stream.readNBitsToUint64Be(12)));
    erpOfdmPhyHeader->setParity(stream.readBit());
    erpOfdmPhyHeader->setTail(stream.readNBitsToUint64Be(6));
    erpOfdmPhyHeader->setService(stream.readUint16Be());
    return erpOfdmPhyHeader;
}

/**
 * HT
 */
void Ieee80211HtPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto htPhyHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(chunk);
}

const Ptr<Chunk> Ieee80211HtPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto htPhyHeader = makeShared<Ieee80211HtPhyHeader>();
    return htPhyHeader;
}

/**
 * VHT
 */
void Ieee80211VhtPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto vhtPhyHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(chunk);
}

const Ptr<Chunk> Ieee80211VhtPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto vhtPhyHeader = makeShared<Ieee80211VhtPhyHeader>();
    return vhtPhyHeader;
}

/**
 * HE MU
 */
void Ieee80211HeMuPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto heMuPhyHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(chunk);
    unsigned int numUsers = heMuPhyHeader->getUsersArraySize();
    if (numUsers > 255)
        throw cRuntimeError("Too many HE MU users: %u", numUsers);

    // --- 1. HE-SIG-A (8 bytes = 64 bits) - IEEE Std 802.11-2024 Table 27-21 ---
    // HE-SIG-A1 (26 bits):
    // B0: UL/DL (0 for DL, 1 for UL)
    stream.writeBit(heMuPhyHeader->getPpduFormat() & 1);
    // B1-B3: HE-SIG-B MCS (3 bits)
    stream.writeNBitsOfUint64Be(0, 3);
    // B4: HE-SIG-B DCM (1 bit)
    stream.writeBit(false);
    // B5-B10: BSS Color (6 bits)
    stream.writeNBitsOfUint64Be(heMuPhyHeader->getBssColor() & 0x3F, 6);
    // B11-B14: Spatial Reuse (4 bits)
    stream.writeNBitsOfUint64Be(0, 4);
    // B15-B17: Bandwidth (3 bits)
    uint32_t maxToneIndex = 0;
    for (unsigned int i = 0; i < numUsers; ++i) {
        const auto& u = heMuPhyHeader->getUsers(i);
        maxToneIndex = std::max(maxToneIndex, (uint32_t)(u.ruToneOffset + u.ruToneSize));
    }
    uint8_t bwField = 0;
    if (maxToneIndex > 996) bwField = 3;
    else if (maxToneIndex > 484) bwField = 2;
    else if (maxToneIndex > 242) bwField = 1;
    stream.writeNBitsOfUint64Be(bwField, 3);

    // B18-B21: Number of HE-SIG-B Symbols or MU-MIMO Users (4 bits)
    uint8_t numUsersField = (numUsers > 0) ? (numUsers - 1) & 0xF : 0;
    stream.writeNBitsOfUint64Be(numUsersField, 4);

    // B22: HE-SIG-B Compression (1 bit)
    stream.writeBit(false);

    // B23-B24: GI+HE-LTF Size (2 bits) - maps guardInterval
    uint8_t giLtf = 2;
    if (heMuPhyHeader->getGuardInterval() == 0) giLtf = 1;
    else if (heMuPhyHeader->getGuardInterval() == 1) giLtf = 2;
    else if (heMuPhyHeader->getGuardInterval() == 2) giLtf = 3;
    stream.writeNBitsOfUint64Be(giLtf, 2);

    // B25: Doppler (1 bit)
    stream.writeBit(false);

    // HE-SIG-A2 (26 bits):
    // B0-B6: TXOP (7 bits)
    stream.writeNBitsOfUint64Be(127, 7);
    // B7: Reserved (1 bit) - set to 1
    stream.writeBit(true);
    // B8-B10: Number of HE-LTF Symbols and Midamble Periodicity (3 bits)
    stream.writeNBitsOfUint64Be(0, 3);
    // B11: LDPC Extra Symbol Segment (1 bit)
    stream.writeBit(heMuPhyHeader->getCoding() & 1);
    // B12: STBC (1 bit)
    stream.writeBit(false);
    // B13-B14: Pre-FEC Padding Factor (2 bits)
    stream.writeNBitsOfUint64Be(0, 2);
    // B15: PE Disambiguity (1 bit)
    stream.writeBit(false);
    // B16-B19: CRC (4 bits)
    stream.writeNBitsOfUint64Be(0, 4);
    // B20-B25: Tail (6 bits)
    stream.writeNBitsOfUint64Be(0, 6);

    // Pad HE-SIG-A to 8 bytes (12 bits)
    stream.writeNBitsOfUint64Be(0, 12);

    // --- 2. HE-SIG-B (only if ppduFormat == 0, i.e. DL MU) ---
    if (heMuPhyHeader->getPpduFormat() == 0) {
        Hz channelBw = (bwField == 3) ? Hz(160e6) : ((bwField == 2) ? Hz(80e6) : ((bwField == 1) ? Hz(40e6) : Hz(20e6)));
        std::vector<Ieee80211HeRu> rus;
        for (unsigned int i = 0; i < numUsers; ++i) {
            const auto& user = heMuPhyHeader->getUsers(i);
            Ieee80211HeRu ru;
            ru.index = user.ruIndex;
            ru.toneSize = user.ruToneSize;
            ru.toneOffset = user.ruToneOffset;
            rus.push_back(ru);
        }
        auto codecResult = encodeHeSigBRuAllocation(rus, channelBw);
        uint8_t numCodes = codecResult ? codecResult.allocation.allocationCodes.size() : 0;
        stream.writeByte(numCodes);
        for (uint8_t code : codecResult.allocation.allocationCodes) {
            stream.writeByte(code);
        }

        // User Specific Field (20 bits per user)
        for (unsigned int i = 0; i < numUsers; ++i) {
            const auto& user = heMuPhyHeader->getUsers(i);
            stream.writeNBitsOfUint64Be(user.staId, 11);
            stream.writeNBitsOfUint64Be(user.mcs, 4);
            stream.writeBit(heMuPhyHeader->getCoding() & 1);
            uint8_t nssField = (user.numberOfSpatialStreams > 0) ? (user.numberOfSpatialStreams - 1) & 0x7 : 0;
            stream.writeNBitsOfUint64Be(nssField, 3);
            stream.writeBit(user.dcm);
        }
    }

    // --- 3. Simulator Extension Block (internal simulator state metadata) ---
    stream.writeUint32Be(heMuPhyHeader->getTriggerId());
    stream.writeUint32Be(heMuPhyHeader->getCommonDuration().inUnit(SIMTIME_NS));
    stream.writeByte(numUsers);
    for (unsigned int i = 0; i < numUsers; ++i) {
        const auto& user = heMuPhyHeader->getUsers(i);
        stream.writeByte(user.ruIndex);
        stream.writeUint16Be(user.ruToneSize);
        stream.writeUint16Be(user.ruToneOffset);
        stream.writeUint16Be(user.staId);
        stream.writeByte(user.mcs);
        stream.writeByte(user.numberOfSpatialStreams);
        stream.writeBit(user.dcm);
        stream.writeByte(user.streamStartIndex);
        stream.writeUint16Be((uint16_t)std::lround(user.leakageSum * 10000.0));
        stream.writeUint32Be(user.psduLength.get<B>());
        stream.writeUint32Be(user.duration.inUnit(SIMTIME_NS));
    }
}

const Ptr<Chunk> Ieee80211HeMuPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto heMuPhyHeader = makeShared<Ieee80211HeMuPhyHeader>();

    // --- 1. HE-SIG-A (8 bytes = 64 bits) ---
    auto ppduFormat = stream.readBit();
    heMuPhyHeader->setPpduFormat(ppduFormat);
    stream.readNBitsToUint64Be(3); // HE-SIG-B MCS
    stream.readBit(); // HE-SIG-B DCM
    heMuPhyHeader->setBssColor(stream.readNBitsToUint64Be(6));
    stream.readNBitsToUint64Be(4); // Spatial Reuse
    auto bwField = stream.readNBitsToUint64Be(3);
    auto numUsersField = stream.readNBitsToUint64Be(4);
    stream.readBit(); // Compression
    auto giLtf = stream.readNBitsToUint64Be(2);
    uint8_t gi = 2;
    if (giLtf == 1) gi = 0;
    else if (giLtf == 2) gi = 1;
    else if (giLtf == 3) gi = 2;
    heMuPhyHeader->setGuardInterval(gi);
    stream.readBit(); // Doppler

    stream.readNBitsToUint64Be(7); // TXOP
    stream.readBit(); // Reserved B7
    stream.readNBitsToUint64Be(3); // Number of HE-LTF Symbols
    auto coding = stream.readBit();
    heMuPhyHeader->setCoding(coding);
    stream.readBit(); // STBC
    stream.readNBitsToUint64Be(2); // Pre-FEC Padding Factor
    stream.readBit(); // PE Disambiguity
    stream.readNBitsToUint64Be(4); // CRC
    stream.readNBitsToUint64Be(6); // Tail
    stream.readNBitsToUint64Be(12); // Padding

    // --- 2. HE-SIG-B (only if ppduFormat == 0) ---
    if (ppduFormat == 0) {
        uint8_t numCodes = stream.readByte();
        std::vector<uint8_t> codes;
        for (uint8_t c = 0; c < numCodes; ++c) {
            codes.push_back(stream.readByte());
        }
        int numUsers = numUsersField + 1;
        for (int i = 0; i < numUsers; ++i) {
            stream.readNBitsToUint64Be(11); // STA-ID
            stream.readNBitsToUint64Be(4); // MCS
            stream.readBit(); // Coding
            stream.readNBitsToUint64Be(3); // NSS
            stream.readBit(); // DCM
        }
    }

    // --- 3. Simulator Extension Block ---
    heMuPhyHeader->setTriggerId(stream.readUint32Be());
    heMuPhyHeader->setCommonDuration(SimTime(stream.readUint32Be(), SIMTIME_NS));
    unsigned int numUsers = stream.readByte();
    heMuPhyHeader->setUsersArraySize(numUsers);
    for (unsigned int i = 0; i < numUsers; ++i) {
        Ieee80211HeMuUserInfo info;
        info.ruIndex = stream.readByte();
        info.ruToneSize = stream.readUint16Be();
        info.ruToneOffset = stream.readUint16Be();
        info.staId = stream.readUint16Be();
        info.mcs = stream.readByte();
        info.numberOfSpatialStreams = stream.readByte();
        info.dcm = stream.readBit();
        info.streamStartIndex = stream.readByte();
        info.leakageSum = stream.readUint16Be() / 10000.0;
        info.psduLength = B(stream.readUint32Be());
        info.duration = SimTime(stream.readUint32Be(), SIMTIME_NS);
        heMuPhyHeader->setUsers(i, info);
    }

    return heMuPhyHeader;
}

} // namespace physicallayer

} // namespace inet
