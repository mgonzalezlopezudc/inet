//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

namespace inet {

namespace  physicallayer {

namespace {

uint32_t packOfdmSignal(uint8_t rate, bool reserved, uint16_t length, bool parity, uint8_t tail)
{
    return (rate & 0xF) |
            (reserved ? 0x10 : 0) |
            ((length & 0xFFF) << 5) |
            (parity ? 0x20000 : 0) |
            ((tail & 0x3F) << 18);
}

void writeOfdmSignal(MemoryOutputStream& stream, uint8_t rate, bool reserved, uint16_t length, bool parity, uint8_t tail)
{
    auto signal = packOfdmSignal(rate, reserved, length, parity, tail);
    stream.writeByte(signal & 0xFF);
    stream.writeByte((signal >> 8) & 0xFF);
    stream.writeByte((signal >> 16) & 0xFF);
}

void readOfdmSignal(MemoryInputStream& stream, uint8_t& rate, bool& reserved, uint16_t& length, bool& parity, uint8_t& tail)
{
    uint32_t signal = stream.readByte();
    signal |= stream.readByte() << 8;
    signal |= stream.readByte() << 16;
    rate = signal & 0xF;
    reserved = (signal & 0x10) != 0;
    length = (signal >> 5) & 0xFFF;
    parity = (signal & 0x20000) != 0;
    tail = (signal >> 18) & 0x3F;
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

    // Packet-level HE signaling model. The serialized bytes cover the HE-SIG
    // fields currently consumed by INET's HE MU PHY/MAC path; runtime-only
    // metadata remains in the chunk object. This is not a complete bit-level
    // PPDU preamble encoder.

    const uint8_t ppduFormat = heMuPhyHeader->getPpduFormat();
    const bool extendedRangeSu = ppduFormat == HE_EXTENDED_RANGE_SU;

    uint32_t maxToneIndex = 0;
    for (unsigned int i = 0; i < numUsers; ++i) {
        const auto& u = heMuPhyHeader->getUsers(i);
        maxToneIndex = std::max(maxToneIndex, (uint32_t)(u.ruToneOffset + u.ruToneSize));
    }
    uint8_t bwField = 0;
    if (maxToneIndex > 996) bwField = 3;
    else if (maxToneIndex > 484) bwField = 2;
    else if (maxToneIndex > 242) bwField = 1;

    uint8_t numUsersField = (numUsers > 0) ? (numUsers - 1) & 0xF : 0;
    uint8_t giLtf = 2;
    if (heMuPhyHeader->getGuardInterval() == 0) giLtf = 1;
    else if (heMuPhyHeader->getGuardInterval() == 1) giLtf = 2;
    else if (heMuPhyHeader->getGuardInterval() == 2) giLtf = 3;

    auto writeHeSigA = [&]() {
        // --- HE-SIG-A (8 bytes = 64 bits) - IEEE Std 802.11-2024 Table 27-21 ---
        // HE-SIG-A1 (26 bits):
        stream.writeBit(ppduFormat == HE_TRIGGER_BASED_UPLINK); // B0: UL/DL
        stream.writeNBitsOfUint64Be(0, 3); // B1-B3: HE-SIG-B MCS
        stream.writeBit(false); // B4: HE-SIG-B DCM
        stream.writeNBitsOfUint64Be(heMuPhyHeader->getBssColor() & 0x3F, 6); // B5-B10: BSS Color
        stream.writeNBitsOfUint64Be(heMuPhyHeader->getSpatialReuse() & 0xF, 4); // B11-B14: Spatial Reuse
        stream.writeNBitsOfUint64Be(bwField, 3); // B15-B17: Bandwidth
        stream.writeNBitsOfUint64Be(numUsersField, 4); // B18-B21: Number of HE-SIG-B Symbols or MU-MIMO Users
        stream.writeBit(extendedRangeSu); // B22: HE-SIG-B Compression; also marks serialized ER SU
        stream.writeNBitsOfUint64Be(giLtf, 2); // B23-B24: GI+HE-LTF Size
        stream.writeBit(false); // B25: Doppler

        // HE-SIG-A2 (26 bits):
        stream.writeNBitsOfUint64Be(127, 7); // B0-B6: TXOP
        stream.writeBit(true); // B7: Reserved
        stream.writeNBitsOfUint64Be(0, 3); // B8-B10: Number of HE-LTF Symbols and Midamble Periodicity
        stream.writeBit(heMuPhyHeader->getCoding() & 1); // B11: LDPC Extra Symbol Segment
        stream.writeBit(false); // B12: STBC
        stream.writeNBitsOfUint64Be(0, 2); // B13-B14: Pre-FEC Padding Factor
        stream.writeBit(false); // B15: PE Disambiguity
        stream.writeNBitsOfUint64Be(0, 4); // B16-B19: CRC
        stream.writeNBitsOfUint64Be(0, 6); // B20-B25: Tail
        stream.writeNBitsOfUint64Be(0, 12); // Pad HE-SIG-A to 8 bytes
    };

    writeHeSigA();
    if (extendedRangeSu)
        writeHeSigA();

    // --- 2. HE-SIG-B (only if ppduFormat == 0, i.e. DL MU) ---
    if (ppduFormat == HE_MU_DOWNLINK) {
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
        auto codecResult = encodeHeSigBCommonField(rus, channelBw);
        if (!codecResult)
            throw cRuntimeError("Cannot serialize HE-SIG-B RU allocation: %s", codecResult.error.c_str());
        
        for (const auto& cc : codecResult.commonField.contentChannels) {
            for (uint8_t code : cc.ruAllocationSubfields) {
                stream.writeByte(code);
            }
        }
        if (channelBw > Hz(40e6)) {
            stream.writeBit(codecResult.commonField.contentChannels[0].hasCenterRu);
            stream.writeBit(codecResult.commonField.contentChannels[1].hasCenterRu);
        }

        std::vector<Ieee80211HeMuUserInfo> cc1Users;
        std::vector<Ieee80211HeMuUserInfo> cc2Users;

        std::map<int, std::vector<Ieee80211HeMuUserInfo>> usersByRuIndex;
        for (unsigned int i = 0; i < numUsers; ++i) {
            const auto& user = heMuPhyHeader->getUsers(i);
            usersByRuIndex[user.ruIndex].push_back(user);
        }

        auto subchannelRUs = getHeRuAllocationCatalog(Hz(0), channelBw);
        subchannelRUs.erase(std::remove_if(subchannelRUs.begin(), subchannelRUs.end(),
            [](const Ieee80211HeRu& ru) { return ru.toneSize != 242; }), subchannelRUs.end());
        std::sort(subchannelRUs.begin(), subchannelRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
            return a.toneOffset < b.toneOffset;
        });

        int K = subchannelRUs.size();
        for (int s = 0; s < K; ++s) {
            int c = s % 2;
            auto& ccUsers = (c == 0) ? cc1Users : cc2Users;

            std::vector<Ieee80211HeRu> localRUs;
            for (const auto& ru : codecResult.commonField.rus) {
                if (ru.toneSize > 242) {
                    if (ru.toneOffset <= subchannelRUs[s].toneOffset &&
                        ru.toneOffset + ru.toneSize >= subchannelRUs[s].toneOffset + 242) {
                        localRUs.push_back(ru);
                    }
                } else if (ru.toneOffset >= subchannelRUs[s].toneOffset &&
                           ru.toneOffset + ru.toneSize <= subchannelRUs[s].toneOffset + 242) {
                    if (std::find_if(localRUs.begin(), localRUs.end(), [&](const Ieee80211HeRu& r) {
                        return r.toneSize == ru.toneSize && r.toneOffset == ru.toneOffset;
                    }) == localRUs.end()) {
                        localRUs.push_back(ru);
                    }
                }
            }
            std::sort(localRUs.begin(), localRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
                return a.toneOffset < b.toneOffset;
            });

            for (const auto& ru : localRUs) {
                const auto& ruUsers = usersByRuIndex[ru.index];
                if (ru.toneSize > 242) {
                    int total = ruUsers.size();
                    int n1 = (total + 1) / 2;
                    if (c == 0) {
                        for (int i = 0; i < n1; ++i) ccUsers.push_back(ruUsers[i]);
                    } else {
                        for (int i = n1; i < total; ++i) ccUsers.push_back(ruUsers[i]);
                    }
                } else {
                    for (const auto& u : ruUsers) {
                        ccUsers.push_back(u);
                    }
                }
            }
        }

        for (const auto& ru : codecResult.commonField.rus) {
            if (ru.toneSize == 26) {
                if (ru.toneOffset == 485) {
                    for (const auto& u : usersByRuIndex[ru.index]) cc1Users.push_back(u);
                } else if (ru.toneOffset == 1481) {
                    for (const auto& u : usersByRuIndex[ru.index]) cc2Users.push_back(u);
                }
            }
        }

        auto writeUser = [&](const Ieee80211HeMuUserInfo& user) {
            stream.writeNBitsOfUint64Be(user.staId, 11);
            stream.writeNBitsOfUint64Be(user.mcs, 4);
            stream.writeBit(heMuPhyHeader->getCoding() & 1);
            uint8_t nssField = (user.numberOfSpatialStreams > 0) ? (user.numberOfSpatialStreams - 1) & 0x7 : 0;
            stream.writeNBitsOfUint64Be(nssField, 3);
            stream.writeBit(user.dcm);
        };

        for (const auto& u : cc1Users) writeUser(u);
        for (const auto& u : cc2Users) writeUser(u);
    }

    // Runtime-only fields such as triggerId, commonDuration, PSDU length and
    // per-user resolved duration remain in the packet-level chunk object; they
    // are not HE PHY signaling bits and are intentionally not serialized here.
}

const Ptr<Chunk> Ieee80211HeMuPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto heMuPhyHeader = makeShared<Ieee80211HeMuPhyHeader>();

    // --- 1. HE-SIG-A (8 bytes = 64 bits) ---
    auto uplink = stream.readBit();
    heMuPhyHeader->setPpduFormat(uplink ? HE_TRIGGER_BASED_UPLINK : HE_MU_DOWNLINK);
    stream.readNBitsToUint64Be(3); // HE-SIG-B MCS
    stream.readBit(); // HE-SIG-B DCM
    heMuPhyHeader->setBssColor(stream.readNBitsToUint64Be(6));
    heMuPhyHeader->setSpatialReuse(stream.readNBitsToUint64Be(4)); // Spatial Reuse
    auto bwField = stream.readNBitsToUint64Be(3);
    auto numUsersField = stream.readNBitsToUint64Be(4);
    auto compression = stream.readBit();
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

    auto ppduFormat = heMuPhyHeader->getPpduFormat();
    if (!uplink && compression && numUsersField == 0) {
        ppduFormat = HE_EXTENDED_RANGE_SU;
        heMuPhyHeader->setPpduFormat(ppduFormat);
        stream.readNBitsToUint64Be(64); // duplicated HE-SIG-A
    }

    // --- 2. HE-SIG-B (only if ppduFormat == 0) ---
    if (ppduFormat == HE_MU_DOWNLINK) {
        Hz channelBw = (bwField == 3) ? Hz(160e6) : ((bwField == 2) ? Hz(80e6) : ((bwField == 1) ? Hz(40e6) : Hz(20e6)));
        int numContentChannels = (channelBw > Hz(20e6)) ? 2 : 1;
        int N = (channelBw >= Hz(160e6)) ? 4 : (channelBw >= Hz(80e6)) ? 2 : 1;

        Ieee80211HeSigBCommonField commonField;
        commonField.contentChannels.resize(numContentChannels);
        for (int c = 0; c < numContentChannels; ++c) {
            for (int f = 0; f < N; ++f) {
                commonField.contentChannels[c].ruAllocationSubfields.push_back(stream.readByte());
            }
        }
        if (channelBw > Hz(40e6)) {
            commonField.contentChannels[0].hasCenterRu = stream.readBit();
            commonField.contentChannels[1].hasCenterRu = stream.readBit();
        }

        auto decoded = decodeHeSigBCommonField(commonField, Hz(0), channelBw);
        if (!decoded)
            throw cRuntimeError("Cannot deserialize HE-SIG-B RU allocation: %s", decoded.error.c_str());

        std::vector<std::pair<Ieee80211HeRu, int>> cc1Allocations;
        std::vector<std::pair<Ieee80211HeRu, int>> cc2Allocations;

        auto subchannelRUs = getHeRuAllocationCatalog(Hz(0), channelBw);
        subchannelRUs.erase(std::remove_if(subchannelRUs.begin(), subchannelRUs.end(),
            [](const Ieee80211HeRu& ru) { return ru.toneSize != 242; }), subchannelRUs.end());
        std::sort(subchannelRUs.begin(), subchannelRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
            return a.toneOffset < b.toneOffset;
        });

        std::map<int, int> wideRuTotalUsers;
        for (int s = 0; s < (int)subchannelRUs.size(); ++s) {
            int c = s % 2;
            int f = s / 2;
            uint8_t code = commonField.contentChannels[c].ruAllocationSubfields[f];
            if (code == 115) continue;
            std::vector<std::pair<int, int>> decodedRUs;
            std::vector<int> decodedUserCounts;
            decodeTable27_27(code, decodedRUs, decodedUserCounts);
            for (size_t i = 0; i < decodedRUs.size(); ++i) {
                if (decodedRUs[i].first > 242) {
                    auto it = std::find_if(decoded.commonField.rus.begin(), decoded.commonField.rus.end(), [&](const Ieee80211HeRu& r) {
                        return r.toneSize == decodedRUs[i].first && r.toneOffset <= subchannelRUs[s].toneOffset &&
                               r.toneOffset + r.toneSize >= subchannelRUs[s].toneOffset + 242;
                    });
                    if (it != decoded.commonField.rus.end()) {
                        wideRuTotalUsers[it->index] += decodedUserCounts[i];
                    }
                }
            }
        }

        for (int s = 0; s < (int)subchannelRUs.size(); ++s) {
            int c = s % 2;
            auto& ccAllocations = (c == 0) ? cc1Allocations : cc2Allocations;

            std::vector<Ieee80211HeRu> localRUs;
            for (const auto& ru : decoded.commonField.rus) {
                if (ru.toneSize > 242) {
                    if (ru.toneOffset <= subchannelRUs[s].toneOffset &&
                        ru.toneOffset + ru.toneSize >= subchannelRUs[s].toneOffset + 242) {
                        localRUs.push_back(ru);
                    }
                } else if (ru.toneOffset >= subchannelRUs[s].toneOffset &&
                           ru.toneOffset + ru.toneSize <= subchannelRUs[s].toneOffset + 242) {
                    if (std::find_if(localRUs.begin(), localRUs.end(), [&](const Ieee80211HeRu& r) {
                        return r.toneSize == ru.toneSize && r.toneOffset == ru.toneOffset;
                    }) == localRUs.end()) {
                        localRUs.push_back(ru);
                    }
                }
            }
            std::sort(localRUs.begin(), localRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
                return a.toneOffset < b.toneOffset;
            });

            for (const auto& ru : localRUs) {
                if (ru.toneSize > 242) {
                    int total = wideRuTotalUsers[ru.index];
                    int n1 = (total + 1) / 2;
                    int n2 = total / 2;
                    int count = (c == 0) ? n1 : n2;
                    if (count > 0) {
                        ccAllocations.push_back({ru, count});
                    }
                } else {
                    int count = std::count_if(decoded.commonField.rus.begin(), decoded.commonField.rus.end(), [&](const Ieee80211HeRu& r) {
                        return r.toneSize == ru.toneSize && r.toneOffset == ru.toneOffset;
                    });
                    ccAllocations.push_back({ru, count});
                }
            }
        }

        for (const auto& ru : decoded.commonField.rus) {
            if (ru.toneSize == 26) {
                if (ru.toneOffset == 485) {
                    cc1Allocations.push_back({ru, 1});
                } else if (ru.toneOffset == 1481) {
                    cc2Allocations.push_back({ru, 1});
                }
            }
        }

        int numUsersCC1 = 0;
        for (const auto& alloc : cc1Allocations) numUsersCC1 += alloc.second;
        int numUsersCC2 = 0;
        for (const auto& alloc : cc2Allocations) numUsersCC2 += alloc.second;
        int totalUsersToRead = numUsersCC1 + numUsersCC2;

        std::vector<Ieee80211HeMuUserInfo> cc1Users;
        std::vector<Ieee80211HeMuUserInfo> cc2Users;

        auto readUser = [&]() {
            Ieee80211HeMuUserInfo info;
            info.staId = stream.readNBitsToUint64Be(11);
            info.mcs = stream.readNBitsToUint64Be(4);
            auto userCoding = stream.readBit();
            if (userCoding != coding)
                throw cRuntimeError("HE-SIG-B user coding does not match HE-SIG-A coding");
            info.numberOfSpatialStreams = stream.readNBitsToUint64Be(3) + 1;
            info.dcm = stream.readBit();
            return info;
        };

        for (int i = 0; i < numUsersCC1; ++i) cc1Users.push_back(readUser());
        for (int i = 0; i < numUsersCC2; ++i) cc2Users.push_back(readUser());

        heMuPhyHeader->setUsersArraySize(totalUsersToRead);
        int userIdx = 0;

        int cc1Idx = 0;
        for (const auto& alloc : cc1Allocations) {
            for (int u = 0; u < alloc.second; ++u) {
                auto info = cc1Users[cc1Idx++];
                info.ruIndex = alloc.first.index;
                info.ruToneSize = alloc.first.toneSize;
                info.ruToneOffset = alloc.first.toneOffset;
                heMuPhyHeader->setUsers(userIdx++, info);
            }
        }
        int cc2Idx = 0;
        for (const auto& alloc : cc2Allocations) {
            for (int u = 0; u < alloc.second; ++u) {
                auto info = cc2Users[cc2Idx++];
                info.ruIndex = alloc.first.index;
                info.ruToneSize = alloc.first.toneSize;
                info.ruToneOffset = alloc.first.toneOffset;
                heMuPhyHeader->setUsers(userIdx++, info);
            }
        }
    }

    return heMuPhyHeader;
}

} // namespace physicallayer

} // namespace inet
