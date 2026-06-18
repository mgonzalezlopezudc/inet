//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211OfdmSignalField.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

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

} // namespace

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
    if (heMuPhyHeader->getGuardInterval() > HE_GI_3_2_US)
        throw cRuntimeError("Invalid HE guard interval: %u", heMuPhyHeader->getGuardInterval());
    if (heMuPhyHeader->getCoding() != HE_CODING_BCC)
        throw cRuntimeError("HE LDPC serialization is not supported");
    stream.writeByte(heMuPhyHeader->getBssColor());
    stream.writeByte(heMuPhyHeader->getPpduFormat());
    stream.writeByte(heMuPhyHeader->getGuardInterval());
    stream.writeByte(heMuPhyHeader->getCoding());
    stream.writeUint32Be(heMuPhyHeader->getTriggerId());
    stream.writeUint32Be(heMuPhyHeader->getCommonDuration().inUnit(SIMTIME_NS));
    stream.writeByte(numUsers);
    for (unsigned int i = 0; i < numUsers; ++i) {
        const auto& user = heMuPhyHeader->getUsers(i);
        if (user.ruIndex < 0 || user.ruIndex > 255)
            throw cRuntimeError("Invalid HE MU RU index for serialization: %d", user.ruIndex);
        if (user.staId > 2047)
            throw cRuntimeError("Invalid HE MU STA-ID for serialization: %u", user.staId);
        if (user.ruToneSize != 0 && user.ruToneSize != 26 && user.ruToneSize != 52 && user.ruToneSize != 106 &&
                user.ruToneSize != 242 && user.ruToneSize != 484 && user.ruToneSize != 996 &&
                user.ruToneSize != 1992)
            throw cRuntimeError("Invalid HE MU RU tone size: %u", user.ruToneSize);
        if (user.mcs > 11)
            throw cRuntimeError("Invalid HE MU MCS: %u", user.mcs);
        if (user.numberOfSpatialStreams < 1 || user.numberOfSpatialStreams > 8)
            throw cRuntimeError("Invalid HE MU NSS: %u", user.numberOfSpatialStreams);
        if (user.dcm && !isHeDcmCombinationSupported(user.mcs, user.numberOfSpatialStreams))
            throw cRuntimeError("Invalid HE MU DCM combination");
        stream.writeByte(user.ruIndex);
        stream.writeUint16Be(user.ruToneSize);
        stream.writeUint16Be(user.ruToneOffset);
        stream.writeUint16Be(user.staId);
        stream.writeByte(user.mcs);
        stream.writeByte(user.numberOfSpatialStreams);
        stream.writeBit(user.dcm);
        stream.writeUint32Be(user.psduLength.get<B>());
        stream.writeUint32Be(user.duration.inUnit(SIMTIME_NS));
    }
}

const Ptr<Chunk> Ieee80211HeMuPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto heMuPhyHeader = makeShared<Ieee80211HeMuPhyHeader>();
    heMuPhyHeader->setBssColor(stream.readByte());
    heMuPhyHeader->setPpduFormat(stream.readByte());
    heMuPhyHeader->setGuardInterval(stream.readByte());
    heMuPhyHeader->setCoding(stream.readByte());
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
        info.psduLength = B(stream.readUint32Be());
        info.duration = SimTime(stream.readUint32Be(), SIMTIME_NS);
        heMuPhyHeader->setUsers(i, info);
    }
    return heMuPhyHeader;
}

} // namespace physicallayer

} // namespace inet
