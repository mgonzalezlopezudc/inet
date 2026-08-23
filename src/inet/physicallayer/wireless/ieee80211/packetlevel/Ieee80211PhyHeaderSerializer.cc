//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211OfdmSignalField.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderCrc.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"

namespace inet {

namespace  physicallayer {

namespace {

void writeOfdmSignal(MemoryOutputStream& stream, uint8_t rate, bool reserved, uint16_t length, bool parity, uint8_t tail)
{
    auto signal = packIeee80211OfdmSignalField(rate, reserved, length, parity, tail);
    stream.writeByte(signal & 0xFF);
    stream.writeByte((signal >> 8) & 0xFF);
    stream.writeByte((signal >> 16) & 0xFF);
}

void readOfdmSignal(MemoryInputStream& stream, uint8_t& rate, bool& reserved, uint16_t& length, bool& parity, uint8_t& tail)
{
    uint32_t signal = stream.readByte();
    signal |= stream.readByte() << 8;
    signal |= stream.readByte() << 16;
    auto field = unpackIeee80211OfdmSignalField(signal);
    rate = field.rate;
    reserved = field.reserved;
    length = field.length;
    parity = field.parity;
    tail = field.tail;
}

void appendLe(std::vector<bool>& bits, uint64_t value, int width)
{
    if (width < 0 || width > 64 || (width != 64 && value >= (uint64_t(1) << width)))
        throw cRuntimeError("IEEE 802.11 PHY header field does not fit in %d bits", width);
    for (int i = 0; i < width; i++)
        bits.push_back((value >> i) & 1);
}

uint64_t readLe(MemoryInputStream& stream, std::vector<bool>& protectedBits, int width)
{
    uint64_t value = 0;
    for (int i = 0; i < width; i++) {
        bool bit = stream.readBit();
        protectedBits.push_back(bit);
        if (bit)
            value |= uint64_t(1) << i;
    }
    return value;
}

void writeProtectedBits(MemoryOutputStream& stream, const std::vector<bool>& bits)
{
    if (bits.size() != 34)
        throw cRuntimeError("IEEE 802.11 PHY header must contain 34 protected bits");
    stream.writeBits(bits);
    uint8_t crc = computeIeee80211PhyHeaderCrc(bits);
    // Clause 19.3.9.4.4 denotes the result as {B7,...,B0} and transmits
    // B7 first. The numeric result stores B7 in bit 7.
    for (int i = 7; i >= 0; i--)
        stream.writeBit((crc >> i) & 1);
    stream.writeBitRepeatedly(false, 6);
}

uint8_t readCrcAndTail(MemoryInputStream& stream, bool& tailIsZero)
{
    uint8_t crc = 0;
    for (int i = 7; i >= 0; i--)
        if (stream.readBit())
            crc |= 1 << i;
    tailIsZero = stream.readBitRepeatedly(false, 6);
    return crc;
}

} // namespace

Register_Serializer(Ieee80211FhssPhyHeader, Ieee80211FhssPhyHeaderSerializer);
Register_Serializer(Ieee80211IrPhyHeader, Ieee80211IrPhyHeaderSerializer);
Register_Serializer(Ieee80211DsssPhyHeader, Ieee80211DsssPhyHeaderSerializer);
Register_Serializer(Ieee80211HrDsssPhyHeader, Ieee80211HrDsssPhyHeaderSerializer);
Register_Serializer(Ieee80211OfdmPhyHeader, Ieee80211OfdmPhyHeaderSerializer);
Register_Serializer(Ieee80211ErpOfdmPhyHeader, Ieee80211ErpOfdmPhyHeaderSerializer);
Register_Serializer(Ieee80211HtPhyHeader, Ieee80211HtPhyHeaderSerializer);
Register_Serializer(Ieee80211VhtPhyHeader, Ieee80211VhtPhyHeaderSerializer);

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
    stream.writeByte(dsssPhyHeader->getSignal());
    stream.writeByte(dsssPhyHeader->getService());
    stream.writeUint16Le(dsssPhyHeader->getLengthField().get<B>());
    stream.writeUint16Le(dsssPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211DsssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto dsssPhyHeader = makeShared<Ieee80211DsssPhyHeader>();
    dsssPhyHeader->setSignal(stream.readByte());
    dsssPhyHeader->setService(stream.readByte());
    dsssPhyHeader->setLengthField(B(stream.readUint16Le()));
    dsssPhyHeader->setFcs(stream.readUint16Le());
    dsssPhyHeader->setFcsMode(FCS_COMPUTED);
    return dsssPhyHeader;
}

/**
 * HR/DSSS
 */
void Ieee80211HrDsssPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto hrDsssPhyHeader = dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(chunk);
    stream.writeByte(hrDsssPhyHeader->getSignal());
    stream.writeByte(hrDsssPhyHeader->getService());
    stream.writeUint16Le(hrDsssPhyHeader->getLengthField().get<B>());
    stream.writeUint16Le(hrDsssPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211HrDsssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto hrDsssPhyHeader = makeShared<Ieee80211HrDsssPhyHeader>();
    hrDsssPhyHeader->setSignal(stream.readByte());
    hrDsssPhyHeader->setService(stream.readByte());
    hrDsssPhyHeader->setLengthField(B(stream.readUint16Le()));
    hrDsssPhyHeader->setFcs(stream.readUint16Le());
    hrDsssPhyHeader->setFcsMode(FCS_COMPUTED);
    return hrDsssPhyHeader;
}

/**
 * OFDM
 */
void Ieee80211OfdmPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto ofdmPhyHeader = dynamicPtrCast<const Ieee80211OfdmPhyHeader>(chunk);
    writeOfdmSignal(stream, ofdmPhyHeader->getRate(), ofdmPhyHeader->getReserved(), ofdmPhyHeader->getLengthField().get<B>(), ofdmPhyHeader->getParity(), ofdmPhyHeader->getTail());
    stream.writeUint16Le(ofdmPhyHeader->getService());
}

const Ptr<Chunk> Ieee80211OfdmPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto ofdmPhyHeader = makeShared<Ieee80211OfdmPhyHeader>();
    uint8_t rate;
    bool reserved;
    uint16_t length;
    bool parity;
    uint8_t tail;
    readOfdmSignal(stream, rate, reserved, length, parity, tail);
    ofdmPhyHeader->setRate(rate);
    ofdmPhyHeader->setReserved(reserved);
    ofdmPhyHeader->setLengthField(B(length));
    ofdmPhyHeader->setParity(parity);
    ofdmPhyHeader->setTail(tail);
    ofdmPhyHeader->setService(stream.readUint16Le());
    return ofdmPhyHeader;
}

/**
 * ERP OFDM
 */
void Ieee80211ErpOfdmPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto erpOfdmPhyHeader = dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(chunk);
    writeOfdmSignal(stream, erpOfdmPhyHeader->getRate(), erpOfdmPhyHeader->getReserved(), erpOfdmPhyHeader->getLengthField().get<B>(), erpOfdmPhyHeader->getParity(), erpOfdmPhyHeader->getTail());
    stream.writeUint16Le(erpOfdmPhyHeader->getService());
}

const Ptr<Chunk> Ieee80211ErpOfdmPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto erpOfdmPhyHeader = makeShared<Ieee80211ErpOfdmPhyHeader>();
    uint8_t rate;
    bool reserved;
    uint16_t length;
    bool parity;
    uint8_t tail;
    readOfdmSignal(stream, rate, reserved, length, parity, tail);
    erpOfdmPhyHeader->setRate(rate);
    erpOfdmPhyHeader->setReserved(reserved);
    erpOfdmPhyHeader->setLengthField(B(length));
    erpOfdmPhyHeader->setParity(parity);
    erpOfdmPhyHeader->setTail(tail);
    erpOfdmPhyHeader->setService(stream.readUint16Le());
    return erpOfdmPhyHeader;
}

/**
 * HT
 */
void Ieee80211HtPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto htPhyHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(chunk);
    std::vector<bool> bits;
    appendLe(bits, htPhyHeader->getMcs(), 7);
    appendLe(bits, htPhyHeader->getChannelWidth40(), 1);
    auto length = htPhyHeader->getLengthField().get<B>();
    if (length < 0 || length > 65535)
        throw cRuntimeError("HT-SIG length is outside the 16-bit field");
    appendLe(bits, length, 16);
    appendLe(bits, htPhyHeader->getSmoothing(), 1);
    appendLe(bits, htPhyHeader->getNotSounding(), 1);
    appendLe(bits, htPhyHeader->getReserved(), 1);
    appendLe(bits, htPhyHeader->getAggregation(), 1);
    appendLe(bits, htPhyHeader->getStbc(), 2);
    appendLe(bits, htPhyHeader->getFecCoding(), 1);
    appendLe(bits, htPhyHeader->getShortGi(), 1);
    appendLe(bits, htPhyHeader->getNumberOfExtensionSpatialStreams(), 2);
    writeProtectedBits(stream, bits);
}

const Ptr<Chunk> Ieee80211HtPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto htPhyHeader = makeShared<Ieee80211HtPhyHeader>();
    std::vector<bool> bits;
    htPhyHeader->setMcs(readLe(stream, bits, 7));
    htPhyHeader->setChannelWidth40(readLe(stream, bits, 1));
    htPhyHeader->setLengthField(B(readLe(stream, bits, 16)));
    htPhyHeader->setSmoothing(readLe(stream, bits, 1));
    htPhyHeader->setNotSounding(readLe(stream, bits, 1));
    htPhyHeader->setReserved(readLe(stream, bits, 1));
    htPhyHeader->setAggregation(readLe(stream, bits, 1));
    htPhyHeader->setStbc(readLe(stream, bits, 2));
    htPhyHeader->setFecCoding(readLe(stream, bits, 1));
    htPhyHeader->setShortGi(readLe(stream, bits, 1));
    htPhyHeader->setNumberOfExtensionSpatialStreams(readLe(stream, bits, 2));
    bool tailIsZero;
    uint8_t crc = readCrcAndTail(stream, tailIsZero);
    htPhyHeader->setCrc(crc);
    if (crc != computeIeee80211PhyHeaderCrc(bits) || !htPhyHeader->getReserved())
        htPhyHeader->markIncorrect();
    if (!tailIsZero || htPhyHeader->getMcs() > 76 || htPhyHeader->getStbc() == 3)
        htPhyHeader->markImproperlyRepresented();
    return htPhyHeader;
}

/**
 * VHT
 */
void Ieee80211VhtPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto vhtPhyHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(chunk);
    std::vector<bool> bits;
    appendLe(bits, vhtPhyHeader->getBandwidth(), 2);
    appendLe(bits, vhtPhyHeader->getReserved1(), 1);
    appendLe(bits, vhtPhyHeader->getStbc(), 1);
    appendLe(bits, vhtPhyHeader->getGroupId(), 6);
    appendLe(bits, vhtPhyHeader->getNumberOfSpaceTimeStreams(), 3);
    appendLe(bits, vhtPhyHeader->getPartialAid(), 9);
    appendLe(bits, vhtPhyHeader->getTxopPsNotAllowed(), 1);
    appendLe(bits, vhtPhyHeader->getReserved2(), 1);
    appendLe(bits, vhtPhyHeader->getShortGi(), 1);
    appendLe(bits, vhtPhyHeader->getShortGiNsymDisambiguation(), 1);
    appendLe(bits, vhtPhyHeader->getCoding(), 1);
    appendLe(bits, vhtPhyHeader->getLdpcExtraOfdmSymbol(), 1);
    appendLe(bits, vhtPhyHeader->getMcs(), 4);
    appendLe(bits, vhtPhyHeader->getBeamformed(), 1);
    appendLe(bits, vhtPhyHeader->getReserved3(), 1);
    writeProtectedBits(stream, bits);

    auto sigBLayout = getVhtSuSigBLayout(vhtPhyHeader->getBandwidth());
    auto expectedChunkLength = b(48 + sigBLayout.getBitLength());
    if (vhtPhyHeader->getChunkLength() != expectedChunkLength)
        throw cRuntimeError("VHT PHY header chunk length %s does not match bandwidth-code %u layout %s",
                vhtPhyHeader->getChunkLength().str().c_str(), vhtPhyHeader->getBandwidth(), expectedChunkLength.str().c_str());
    auto encodedLength = encodeVhtSuSigBLength(vhtPhyHeader->getLengthField());
    if (encodedLength != vhtPhyHeader->getVhtSigBLength())
        throw cRuntimeError("VHT-SIG-B Length %u does not encode APEP length %s",
                vhtPhyHeader->getVhtSigBLength(), vhtPhyHeader->getLengthField().str().c_str());
    std::vector<bool> sigBBits;
    appendLe(sigBBits, encodedLength, sigBLayout.lengthFieldWidth);
    appendLe(sigBBits, vhtPhyHeader->getVhtSigBReserved(), sigBLayout.reservedFieldWidth);
    appendLe(sigBBits, vhtPhyHeader->getVhtSigBTail(), 6);
    stream.writeBits(sigBBits);
}

const Ptr<Chunk> Ieee80211VhtPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto vhtPhyHeader = makeShared<Ieee80211VhtPhyHeader>();
    std::vector<bool> bits;
    vhtPhyHeader->setBandwidth(readLe(stream, bits, 2));
    vhtPhyHeader->setReserved1(readLe(stream, bits, 1));
    vhtPhyHeader->setStbc(readLe(stream, bits, 1));
    vhtPhyHeader->setGroupId(readLe(stream, bits, 6));
    vhtPhyHeader->setNumberOfSpaceTimeStreams(readLe(stream, bits, 3));
    vhtPhyHeader->setPartialAid(readLe(stream, bits, 9));
    vhtPhyHeader->setTxopPsNotAllowed(readLe(stream, bits, 1));
    vhtPhyHeader->setReserved2(readLe(stream, bits, 1));
    vhtPhyHeader->setShortGi(readLe(stream, bits, 1));
    vhtPhyHeader->setShortGiNsymDisambiguation(readLe(stream, bits, 1));
    vhtPhyHeader->setCoding(readLe(stream, bits, 1));
    vhtPhyHeader->setLdpcExtraOfdmSymbol(readLe(stream, bits, 1));
    vhtPhyHeader->setMcs(readLe(stream, bits, 4));
    vhtPhyHeader->setBeamformed(readLe(stream, bits, 1));
    vhtPhyHeader->setReserved3(readLe(stream, bits, 1));
    bool tailIsZero;
    uint8_t crc = readCrcAndTail(stream, tailIsZero);
    vhtPhyHeader->setCrc(crc);
    if (crc != computeIeee80211PhyHeaderCrc(bits) || !vhtPhyHeader->getReserved1() ||
        !vhtPhyHeader->getReserved2() || !vhtPhyHeader->getReserved3() ||
        (!vhtPhyHeader->getShortGi() && vhtPhyHeader->getShortGiNsymDisambiguation()) ||
        (!vhtPhyHeader->getCoding() && vhtPhyHeader->getLdpcExtraOfdmSymbol()))
        vhtPhyHeader->markIncorrect();
    if (!tailIsZero || (vhtPhyHeader->getGroupId() != 0 && vhtPhyHeader->getGroupId() != 63) ||
        vhtPhyHeader->getMcs() > 9 || vhtPhyHeader->getStbc())
        vhtPhyHeader->markImproperlyRepresented();

    auto sigBLayout = getVhtSuSigBLayout(vhtPhyHeader->getBandwidth());
    std::vector<bool> sigBBits;
    auto encodedLength = readLe(stream, sigBBits, sigBLayout.lengthFieldWidth);
    auto reserved = readLe(stream, sigBBits, sigBLayout.reservedFieldWidth);
    auto tail = readLe(stream, sigBBits, 6);
    vhtPhyHeader->setVhtSigBLength(encodedLength);
    vhtPhyHeader->setVhtSigBReserved(reserved);
    vhtPhyHeader->setVhtSigBTail(tail);
    vhtPhyHeader->setLengthField(decodeVhtSuSigBLength(encodedLength));
    vhtPhyHeader->setChunkLength(b(48 + sigBLayout.getBitLength()));
    if (reserved != sigBLayout.getReservedValue())
        vhtPhyHeader->markIncorrect();
    if (tail != 0)
        vhtPhyHeader->markImproperlyRepresented();
    return vhtPhyHeader;
}

} // namespace physicallayer

} // namespace inet
