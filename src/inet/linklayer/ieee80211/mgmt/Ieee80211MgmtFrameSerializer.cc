//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211HeMgmtElements.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211HtVhtMgmtElements.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"

namespace inet {

namespace ieee80211 {

Register_Serializer(Ieee80211AssociationRequestFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211AssociationResponseFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211AuthenticationFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211BeaconFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211DeauthenticationFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211DisassociationFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211ProbeRequestFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211ProbeResponseFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211ReassociationRequestFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211ReassociationResponseFrame, Ieee80211MgmtFrameSerializer);
Register_Serializer(Ieee80211HeNdpAnnouncement, Ieee80211HeSoundingMgmtFrameSerializer);
Register_Serializer(Ieee80211HeCompressedBeamformingFeedback, Ieee80211HeSoundingMgmtFrameSerializer);
Register_Serializer(Ieee80211VhtCompressedBeamformingFeedback, Ieee80211VhtActionFrameBodySerializer);
Register_Serializer(Ieee80211VhtGroupIdManagement, Ieee80211VhtActionFrameBodySerializer);
Register_Serializer(Ieee80211HtCsiFeedback, Ieee80211HtActionFrameBodySerializer);
Register_Serializer(Ieee80211HtNoncompressedBeamformingFeedback, Ieee80211HtActionFrameBodySerializer);
Register_Serializer(Ieee80211HtCompressedBeamformingFeedback, Ieee80211HtActionFrameBodySerializer);

static const uint8_t ELEMENT_ID_EXTENSION = 255;
static const uint8_t ELEMENT_ID_EXTENSION_HE_CAPABILITIES = 35;
static const uint8_t ELEMENT_ID_EXTENSION_HE_OPERATION = 36;
static const uint8_t ELEMENT_ID_EXTENSION_HE_6GHZ_BAND_CAPABILITIES = 59;
static const uint8_t ELEMENT_ID_EXTENSION_EHT_OPERATION = 106;
static const uint8_t ELEMENT_ID_EXTENSION_MULTI_LINK = 107;
static const uint8_t ELEMENT_ID_EXTENSION_EHT_CAPABILITIES = 108;
static const uint8_t ELEMENT_ID_EXTENSION_TID_TO_LINK_MAPPING = 111;
static const uint8_t ELEMENT_ID_TWT = 216;
static const uint8_t ELEMENT_ID_HT_CAPABILITIES = 45;
static const uint8_t ELEMENT_ID_HT_OPERATION = 61;
static const uint8_t ELEMENT_ID_VHT_CAPABILITIES = 191;
static const uint8_t ELEMENT_ID_VHT_OPERATION = 192;

static uint64_t getBits(uint64_t value, int offset, int length)
{
    return (value >> offset) & ((uint64_t(1) << length) - 1);
}

static void setBits(uint64_t& value, int offset, int length, uint64_t field)
{
    uint64_t mask = ((uint64_t(1) << length) - 1) << offset;
    value = (value & ~mask) | ((field << offset) & mask);
}

static bool getBit(const std::vector<uint8_t>& bytes, int bit)
{
    return (bytes[bit / 8] & (1 << (bit % 8))) != 0;
}

static int getBits(const std::vector<uint8_t>& bytes, int offset, int length)
{
    int value = 0;
    for (int i = 0; i < length; ++i)
        if (getBit(bytes, offset + i))
            value |= 1 << i;
    return value;
}

static void setBits(std::vector<uint8_t>& bytes, int offset, int length, int field)
{
    for (int i = 0; i < length; ++i) {
        if ((field & (1 << i)) != 0)
            bytes[(offset + i) / 8] |= 1 << ((offset + i) % 8);
        else
            bytes[(offset + i) / 8] &= ~(1 << ((offset + i) % 8));
    }
}

static int encodeHeMcsMapEntry(int maxMcs)
{
    if (maxMcs < 0)
        return 3;
    else if (maxMcs <= 7)
        return 0;
    else if (maxMcs <= 9)
        return 1;
    else
        return 2;
}

static int decodeHeMcsMapEntry(int encoded)
{
    switch (encoded) {
        case 0: return 7;
        case 1: return 9;
        case 2: return 11;
        case 3: return -1;
        default: throw cRuntimeError("Invalid HE-MCS map entry");
    }
}

static uint16_t encodeHeMcsMap(const Ieee80211HeMcsNssMapElement& map)
{
    uint16_t encoded = 0;
    for (int i = 0; i < 8; ++i)
        encoded |= encodeHeMcsMapEntry(map.maxMcsForNss[i]) << (2 * i);
    return encoded;
}

static void decodeHeMcsMap(uint16_t encoded, Ieee80211HeMcsNssMapElement& map)
{
    for (int i = 0; i < 8; ++i)
        map.maxMcsForNss[i] = decodeHeMcsMapEntry((encoded >> (2 * i)) & 0x3);
}

static int encodeDcmConstellation(int maxDcmConstellation)
{
    if (maxDcmConstellation <= 0)
        return 0;
    else if (maxDcmConstellation <= 1)
        return 1;
    else if (maxDcmConstellation <= 2)
        return 2;
    else
        return 3;
}

static int decodeDcmConstellation(int encoded)
{
    switch (encoded) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        default: throw cRuntimeError("Invalid HE DCM constellation");
    }
}

static int encodeMaxMpduLength(int maxMpduLength)
{
    if (maxMpduLength <= 3895)
        return 0;
    else if (maxMpduLength <= 7991)
        return 1;
    else
        return 2;
}

static int decodeMaxMpduLength(int encoded)
{
    switch (encoded) {
        case 0: return 3895;
        case 1: return 7991;
        case 2: return 11454;
        default: return 11454;
    }
}

static int encodeDcmMaxRu(const Ieee80211HeCapabilitiesElement& capabilities)
{
    if (capabilities.ru1992Tone)
        return 3;
    else if (capabilities.ru996Tone)
        return 2;
    else if (capabilities.ru484Tone)
        return 1;
    else
        return 0;
}

static void skipBytes(MemoryInputStream& stream, int length)
{
    for (int i = 0; i < length; ++i)
        stream.readByte();
}

static int encodeVhtMcs(int maxMcs)
{
    return maxMcs < 0 ? 3 : maxMcs <= 7 ? 0 : maxMcs == 8 ? 1 : 2;
}

static int decodeVhtMcs(int encoded)
{
    static const int values[] = {7, 8, 9, -1};
    return values[encoded & 3];
}

static void writeHtCapabilitiesElement(MemoryOutputStream& stream, const Ieee80211HtCapabilitiesElement& capabilities)
{
    // IEEE Std 802.11-2024, 9.4.2.54 and Tables 9-224/9-225.
    stream.writeByte(ELEMENT_ID_HT_CAPABILITIES);
    stream.writeByte(26);
    uint16_t information = (capabilities.ldpc ? 1 : 0) |
            (capabilities.supportedChannelWidth40MHz ? 1 << 1 : 0) |
            (capabilities.shortGi20 ? 1 << 5 : 0) |
            (capabilities.shortGi40 ? 1 << 6 : 0);
    stream.writeUint16Le(information);
    stream.writeByte(capabilities.maxAmpduLengthExponent & 3);
    std::vector<uint8_t> mcs(16);
    for (int nss = 0; nss < 4; nss++)
        for (int mcsIndex = 0; mcsIndex <= capabilities.rxMaxMcsForNss[nss] && mcsIndex < 8; mcsIndex++)
            setBits(mcs, nss * 8 + mcsIndex, 1, 1);
    bool unequal = false;
    int txMaxNss = 0;
    for (int nss = 0; nss < 4; nss++) {
        unequal |= capabilities.txMaxMcsForNss[nss] != capabilities.rxMaxMcsForNss[nss];
        if (capabilities.txMaxMcsForNss[nss] >= 0) txMaxNss = nss + 1;
    }
    if (txMaxNss == 0)
        throw cRuntimeError("HT Capabilities requires at least one transmit MCS spatial stream");
    if (unequal) {
        for (int nss = 0; nss < 4; nss++) {
            int expected = nss < txMaxNss ? capabilities.rxMaxMcsForNss[nss] : -1;
            if (capabilities.txMaxMcsForNss[nss] != expected)
                throw cRuntimeError("HT transmit MCS map is not representable by Tx Max NSS with equal modulation");
        }
    }
    setBits(mcs, 96, 1, 1);
    setBits(mcs, 97, 1, unequal);
    setBits(mcs, 98, 2, std::max(0, txMaxNss - 1));
    for (auto byte : mcs) stream.writeByte(byte);
    if (capabilities.mcsFeedback == 1 || capabilities.mcsFeedback < 0 || capabilities.mcsFeedback > 3)
        throw cRuntimeError("Invalid reserved HT MCS Feedback capability value: %d", capabilities.mcsFeedback);
    // IEEE Std 802.11-2024, Figure 9-459 and Table 9-227.
    uint16_t extendedCapabilities = (capabilities.mcsFeedback & 3) << 8 |
            (capabilities.htcSupport ? 1 << 10 : 0);
    stream.writeUint16Le(extendedCapabilities);
    if (capabilities.explicitCsiFeedback < 0 || capabilities.explicitCsiFeedback > 3 ||
            capabilities.explicitNoncompressedFeedback < 0 || capabilities.explicitNoncompressedFeedback > 3 ||
            capabilities.explicitCompressedFeedback < 0 || capabilities.explicitCompressedFeedback > 3)
        throw cRuntimeError("Invalid HT explicit-feedback capability value");
    // IEEE Std 802.11-2024, Figure 9-460 and Table 9-228.
    uint32_t transmitBeamformingCapabilities =
            (capabilities.receiveNdp ? 1u << 3 : 0) |
            (capabilities.transmitNdp ? 1u << 4 : 0) |
            ((capabilities.explicitCsiFeedback & 3u) << 11) |
            ((capabilities.explicitNoncompressedFeedback & 3u) << 13) |
            ((capabilities.explicitCompressedFeedback & 3u) << 15);
    stream.writeUint32Le(transmitBeamformingCapabilities);
    stream.writeByte(0);
}

static void writeHtOperationElement(MemoryOutputStream& stream, const Ieee80211HtOperationElement& operation)
{
    stream.writeByte(ELEMENT_ID_HT_OPERATION);
    stream.writeByte(22);
    stream.writeByte(operation.primaryChannel);
    uint64_t information = (operation.secondaryChannelOffset & 3) |
            (operation.staChannelWidth40MHz ? uint64_t(1) << 2 : 0) |
            (uint64_t(operation.protectionMode & 3) << 8);
    for (int i = 0; i < 5; i++) stream.writeByte((information >> (8 * i)) & 0xff);
    std::vector<uint8_t> basic(16);
    for (int nss = 0; nss < 4; nss++)
        for (int mcs = 0; mcs <= operation.basicMaxMcsForNss[nss] && mcs < 8; mcs++)
            setBits(basic, nss * 8 + mcs, 1, 1);
    for (auto byte : basic) stream.writeByte(byte);
}

static void writeVhtCapabilitiesElement(MemoryOutputStream& stream, const Ieee80211VhtCapabilitiesElement& capabilities)
{
    // IEEE Std 802.11-2024, 9.4.2.156 and Tables 9-313/9-315.
    stream.writeByte(ELEMENT_ID_VHT_CAPABILITIES);
    stream.writeByte(12);
    uint32_t information = (uint32_t(capabilities.supportedChannelWidthSet & 3) << 2) |
            (capabilities.rxLdpc ? 1 << 4 : 0) | (capabilities.shortGi80 ? 1 << 5 : 0) |
            (capabilities.shortGi160 ? 1 << 6 : 0) |
            (capabilities.suBeamformer ? 1 << 11 : 0) |
            (capabilities.suBeamformee ? 1 << 12 : 0) |
            (uint32_t((capabilities.beamformeeSts - 1) & 7) << 13) |
            (uint32_t((capabilities.soundingDimensions - 1) & 7) << 16) |
            (capabilities.muBeamformer ? 1 << 19 : 0) |
            (capabilities.muBeamformee ? 1 << 20 : 0) |
            (uint32_t(capabilities.maxAmpduLengthExponent & 7) << 23);
    stream.writeUint32Le(information);
    uint16_t rxMap = 0, txMap = 0;
    for (int i = 0; i < 8; i++) {
        rxMap |= encodeVhtMcs(capabilities.rxMaxMcsForNss[i]) << (2 * i);
        txMap |= encodeVhtMcs(capabilities.txMaxMcsForNss[i]) << (2 * i);
    }
    stream.writeUint16Le(rxMap);
    uint16_t rxHighestAndNsts = (capabilities.rxHighestLongGiDataRateMbps & 0x1fff) |
            ((capabilities.maxNstsTotal == 0 ? 0 : capabilities.maxNstsTotal - 1) & 7) << 13;
    stream.writeUint16Le(rxHighestAndNsts);
    stream.writeUint16Le(txMap);
    uint16_t txHighestAndExtendedNss = (capabilities.txHighestLongGiDataRateMbps & 0x1fff) |
            (capabilities.extendedNssBwCapable ? 1 << 13 : 0);
    stream.writeUint16Le(txHighestAndExtendedNss);
}

static void writeVhtOperationElement(MemoryOutputStream& stream, const Ieee80211VhtOperationElement& operation)
{
    stream.writeByte(ELEMENT_ID_VHT_OPERATION);
    stream.writeByte(5);
    stream.writeByte(operation.channelWidth);
    stream.writeByte(operation.centerFrequencySegment0);
    stream.writeByte(operation.centerFrequencySegment1);
    uint16_t basic = 0;
    for (int i = 0; i < 8; i++) basic |= encodeVhtMcs(operation.basicMaxMcsForNss[i]) << (2 * i);
    stream.writeUint16Le(basic);
}

static void writeHeCapabilitiesElement(MemoryOutputStream& stream, const Ieee80211HeCapabilitiesElement& capabilities)
{
    int mcsNssLength = capabilities.supportedChannelWidth160MHz || capabilities.supportedChannelWidth80Plus80MHz ? 8 : 4;
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 6 + 11 + mcsNssLength);
    stream.writeByte(ELEMENT_ID_EXTENSION_HE_CAPABILITIES);

    uint64_t macCapabilities = 0;
    setBits(macCapabilities, 1, 1, capabilities.twtRequester ? 1 : 0);
    setBits(macCapabilities, 2, 1, capabilities.twtResponder ? 1 : 0);
    setBits(macCapabilities, 3, 2, std::clamp((int)capabilities.dynamicFragmentationLevel, 0, 3));
    setBits(macCapabilities, 12, 3, capabilities.multiTidAggregationRx ? 1 : 0);
    setBits(macCapabilities, 17, 1, capabilities.heTbBlockAckTx ? 1 : 0);
    setBits(macCapabilities, 18, 1, capabilities.broadcastTwt ? 1 : 0);
    setBits(macCapabilities, 19, 1, capabilities.ulOfdma ? 1 : 0);
    setBits(macCapabilities, 25, 1, capabilities.omControl ? 1 : 0);
    setBits(macCapabilities, 26, 1, capabilities.ulOfdma ? 1 : 0);
    setBits(macCapabilities, 27, 2, std::clamp((int)capabilities.maxAmpduLengthExponent - 3, 0, 3));
    setBits(macCapabilities, 35, 1, capabilities.multiTidAggregationTx ? 1 : 0);
    // two-NAV is mandatory behavior rather than an HE capability bit. Keep it
    // in the packet-level extension's high model bits so association still
    // negotiates it without overloading a normative subfield.
    setBits(macCapabilities, 42, 1, capabilities.twoNav ? 1 : 0);
    setBits(macCapabilities, 43, 1, capabilities.erBss ? 1 : 0);
    setBits(macCapabilities, 44, 1, capabilities.ndpFeedbackReport ? 1 : 0);
    for (int i = 0; i < 6; ++i)
        stream.writeByte((macCapabilities >> (8 * i)) & 0xff);

    std::vector<uint8_t> phyCapabilities(11, 0);
    int supportedChannelWidthSet = 0;
    if (capabilities.supportedChannelWidth40MHz || capabilities.supportedChannelWidth80MHz)
        supportedChannelWidthSet |= 1 << 1;
    if (capabilities.supportedChannelWidth160MHz)
        supportedChannelWidthSet |= 1 << 2;
    if (capabilities.supportedChannelWidth80Plus80MHz)
        supportedChannelWidthSet |= 1 << 3;
    setBits(phyCapabilities, 1, 7, supportedChannelWidthSet);
    setBits(phyCapabilities, 13, 1, capabilities.ldpc ? 1 : 0);
    setBits(phyCapabilities, 14, 1, capabilities.preamblePuncturing ? 1 : 0);
    setBits(phyCapabilities, 22, 1, capabilities.fullBandwidthUlMuMimo ? 1 : 0);
    setBits(phyCapabilities, 23, 1, capabilities.partialBandwidthUlMuMimo ? 1 : 0);
    int dcmConstellation = capabilities.dcm ? encodeDcmConstellation(capabilities.maxDcmConstellation) : 0;
    int dcmNss = capabilities.dcm && capabilities.maxDcmNss > 1 ? 1 : 0;
    setBits(phyCapabilities, 24, 2, dcmConstellation);
    setBits(phyCapabilities, 26, 1, dcmNss);
    setBits(phyCapabilities, 27, 2, dcmConstellation);
    setBits(phyCapabilities, 29, 1, dcmNss);
    setBits(phyCapabilities, 70, 2, encodeDcmMaxRu(capabilities));
    setBits(phyCapabilities, 75, 1, capabilities.dlMuMimoBeamformer ? 1 : 0);
    setBits(phyCapabilities, 76, 1, capabilities.dlMuMimoBeamformee ? 1 : 0);
    setBits(phyCapabilities, 77, 3, capabilities.soundingDimensions);
    setBits(phyCapabilities, 80, 3, capabilities.beamformeeSts20Mhz);
    setBits(phyCapabilities, 83, 3, capabilities.beamformeeStsAbove20Mhz);
    setBits(phyCapabilities, 86, 2, capabilities.feedbackMode);
    for (auto byte : phyCapabilities)
        stream.writeByte(byte);

    stream.writeUint16Le(encodeHeMcsMap(capabilities.rxMcsNss));
    stream.writeUint16Le(encodeHeMcsMap(capabilities.txMcsNss));
    if (mcsNssLength >= 8) {
        stream.writeUint16Le(encodeHeMcsMap(capabilities.rxMcsNss));
        stream.writeUint16Le(encodeHeMcsMap(capabilities.txMcsNss));
    }
}

static void writeHeOperationElement(MemoryOutputStream& stream, const Ieee80211HeOperationElement& operation)
{
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 3 + 1 + 2);
    stream.writeByte(ELEMENT_ID_EXTENSION_HE_OPERATION);
    uint32_t operationParameters = 0;
    if (operation.defaultPeDurationPresent)
        operationParameters |= std::clamp((int)operation.defaultPeDurationUs / 4, 0, 4);
    // IEEE 802.11-2024, Figure 9-905: ER SU Disable is B16 of the
    // 24-bit HE Operation Parameters field.
    if (operation.erSuDisable)
        operationParameters |= 1 << 16;
    stream.writeByte(operationParameters & 0xff);
    stream.writeByte((operationParameters >> 8) & 0xff);
    stream.writeByte((operationParameters >> 16) & 0xff);
    stream.writeByte(operation.bssColor & 0x3f);
    stream.writeUint16Le(operation.basicHeMcsNss);
}

static void writeHe6GhzBandCapabilitiesElement(MemoryOutputStream& stream, const Ieee80211He6GhzBandCapabilitiesElement& capabilities)
{
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 2);
    stream.writeByte(ELEMENT_ID_EXTENSION_HE_6GHZ_BAND_CAPABILITIES);
    uint16_t capabilitiesInformation = 0;
    capabilitiesInformation |= (capabilities.minimumMpduStartSpacing & 0x7);
    capabilitiesInformation |= (std::clamp((int)capabilities.maxAmpduLengthExponent, 0, 7) & 0x7) << 3;
    capabilitiesInformation |= (encodeMaxMpduLength(capabilities.maxMpduLength) & 0x3) << 6;
    stream.writeUint16Le(capabilitiesInformation);
}

static uint8_t encodeTwtWakeDuration(simtime_t duration)
{
    return std::clamp((int)std::ceil(duration.inUnit(SIMTIME_US) / 256.0), 1, 255);
}

static void writeBroadcastTwtElement(MemoryOutputStream& stream, const Ptr<const Ieee80211MgmtFrame>& frame)
{
    if (!frame->getBroadcastTwtPresent())
        return;
    int count = frame->getBroadcastTwtSchedulesArraySize();
    if (count == 0 || count > 16)
        throw cRuntimeError("Broadcast TWT element requires 1..16 parameter sets");
    stream.writeByte(ELEMENT_ID_TWT);
    stream.writeByte(1 + count * 15);
    stream.writeByte(0x08 | ((count - 1) << 4)); // Broadcast=1, last parameter-set index
    for (int i = 0; i < count; ++i) {
        const auto& schedule = frame->getBroadcastTwtSchedules(i);
        if (schedule.broadcastId < 0 || schedule.broadcastId > 31 || schedule.wakeInterval <= SIMTIME_ZERO || schedule.wakeDuration <= SIMTIME_ZERO)
            throw cRuntimeError("Invalid broadcast TWT parameter set");
        uint64_t targetWakeTime = schedule.targetWakeTime.inUnit(SIMTIME_US);
        uint64_t intervalUs = schedule.wakeInterval.inUnit(SIMTIME_US);
        uint8_t exponent = 0;
        while (intervalUs > 0xffff && exponent < 31) {
            intervalUs = (intervalUs + 1) / 2;
            exponent++;
        }
        stream.writeUint64Le(targetWakeTime);
        stream.writeByte(encodeTwtWakeDuration(schedule.wakeDuration));
        stream.writeUint16Le(intervalUs);
        uint16_t requestType = (schedule.triggerEnabled ? 1 : 0) | (schedule.implicit ? 2 : 0) | ((uint16_t)exponent << 7);
        stream.writeUint16Le(requestType);
        uint16_t broadcastInfo = (schedule.broadcastId & 0x1f) | ((schedule.persistence & 0xff) << 8);
        stream.writeUint16Le(broadcastInfo);
    }
}

static void writeEhtCapabilitiesElement(MemoryOutputStream& stream, const Ieee80211EhtCapabilitiesElement& capabilities)
{
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 14); // Payload size approximated
    stream.writeByte(ELEMENT_ID_EXTENSION_EHT_CAPABILITIES);
    // EHT MAC capabilities (approximated)
    uint64_t macCap = 0;
    setBits(macCap, 0, 1, capabilities.support4096Qam ? 1 : 0);
    setBits(macCap, 1, 1, capabilities.mlo ? 1 : 0);
    setBits(macCap, 2, 1, capabilities.str ? 1 : 0);
    setBits(macCap, 3, 1, capabilities.emlsr ? 1 : 0);
    setBits(macCap, 4, 1, capabilities.emlmr ? 1 : 0);
    setBits(macCap, 5, 2, capabilities.maxAmpduLengthExponent);
    stream.writeUint16Le(macCap);
    
    // EHT PHY capabilities (approximated)
    uint64_t phyCap = 0;
    setBits(phyCap, 0, 1, capabilities.supportedChannelWidth320MHz ? 1 : 0);
    setBits(phyCap, 1, 1, capabilities.preamblePuncturing ? 1 : 0);
    setBits(phyCap, 2, 1, capabilities.dlOfdma ? 1 : 0);
    setBits(phyCap, 3, 1, capabilities.ulOfdma ? 1 : 0);
    setBits(phyCap, 4, 1, capabilities.dlMuMimo ? 1 : 0);
    setBits(phyCap, 5, 1, capabilities.ulMuMimo ? 1 : 0);
    setBits(phyCap, 6, 1, capabilities.ldpc ? 1 : 0);
    setBits(phyCap, 7, 1, capabilities.ehtDup6GHz ? 1 : 0);
    stream.writeUint32Le(phyCap);
    
    // EHT MCS NSS (approximated)
    stream.writeUint32Le(0); // rx
    stream.writeUint32Le(0); // tx
}

static void writeEhtOperationElement(MemoryOutputStream& stream, const Ieee80211EhtOperationElement& operation)
{
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 6);
    stream.writeByte(ELEMENT_ID_EXTENSION_EHT_OPERATION);
    stream.writeUint16Le(operation.operatingChannelWidthMHz);
    stream.writeUint16Le(operation.disabledSubchannelBitmap);
    stream.writeByte(operation.basicEhtMcsNss);
    stream.writeByte(operation.mcs15Disabled ? 1 : 0);
}

static void writeMultiLinkElement(MemoryOutputStream& stream, const Ieee80211MultiLinkElement& ml)
{
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 6 + 1 + 2);
    stream.writeByte(ELEMENT_ID_EXTENSION_MULTI_LINK);
    stream.writeMacAddress(ml.mldMacAddress);
    stream.writeByte(ml.isApMld ? 1 : 0);
    stream.writeUint16Le(ml.linkId);
}

static void writeTidToLinkMappingElement(MemoryOutputStream& stream, const Ieee80211TidToLinkMappingElement& mapping)
{
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 8 * 2);
    stream.writeByte(ELEMENT_ID_EXTENSION_TID_TO_LINK_MAPPING);
    for (int i = 0; i < 8; i++)
        stream.writeUint16Le(mapping.mapping[i]);
}

static void writeHeMgmtElements(MemoryOutputStream& stream, const Ptr<const Ieee80211MgmtFrame>& frame)
{
    if (frame->getHtCapabilitiesPresent()) writeHtCapabilitiesElement(stream, frame->getHtCapabilities());
    if (frame->getHtOperationPresent()) writeHtOperationElement(stream, frame->getHtOperation());
    if (frame->getVhtCapabilitiesPresent()) writeVhtCapabilitiesElement(stream, frame->getVhtCapabilities());
    if (frame->getVhtOperationPresent()) writeVhtOperationElement(stream, frame->getVhtOperation());
    if (frame->getHeCapabilitiesPresent())
        writeHeCapabilitiesElement(stream, frame->getHeCapabilities());
    if (frame->getHeOperationPresent())
        writeHeOperationElement(stream, frame->getHeOperation());
    if (frame->getHe6GhzBandCapabilitiesPresent())
        writeHe6GhzBandCapabilitiesElement(stream, frame->getHe6GhzBandCapabilities());
        
    if (frame->getEhtCapabilitiesPresent())
        writeEhtCapabilitiesElement(stream, frame->getEhtCapabilities());
    if (frame->getEhtOperationPresent())
        writeEhtOperationElement(stream, frame->getEhtOperation());
    if (frame->getMultiLinkElementPresent())
        writeMultiLinkElement(stream, frame->getMultiLinkElement());
    if (frame->getTidToLinkMappingPresent())
        writeTidToLinkMappingElement(stream, frame->getTidToLinkMapping());
        
    writeBroadcastTwtElement(stream, frame);
}

static void readHtCapabilitiesElement(MemoryInputStream& stream, int length, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (length != 26) throw cRuntimeError("Malformed HT Capabilities element: length is %d", length);
    Ieee80211HtCapabilitiesElement capabilities;
    uint16_t information = stream.readUint16Le();
    capabilities.ldpc = information & 1;
    capabilities.supportedChannelWidth40MHz = information & (1 << 1);
    capabilities.shortGi20 = information & (1 << 5);
    capabilities.shortGi40 = information & (1 << 6);
    capabilities.maxAmpduLengthExponent = stream.readByte() & 3;
    std::vector<uint8_t> mcs(16);
    for (auto& byte : mcs) byte = stream.readByte();
    bool unequal = getBits(mcs, 97, 1);
    int txMaxNss = getBits(mcs, 98, 2) + 1;
    for (int nss = 0; nss < 4; nss++) {
        capabilities.rxMaxMcsForNss[nss] = -1;
        for (int index = 0; index < 8; index++) if (getBits(mcs, nss * 8 + index, 1)) capabilities.rxMaxMcsForNss[nss] = index;
        capabilities.txMaxMcsForNss[nss] = !unequal || nss < txMaxNss ? capabilities.rxMaxMcsForNss[nss] : -1;
    }
    uint16_t extendedCapabilities = stream.readUint16Le();
    capabilities.mcsFeedback = (extendedCapabilities >> 8) & 3;
    if (capabilities.mcsFeedback == 1)
        throw cRuntimeError("Malformed HT Capabilities element: reserved MCS Feedback value 1");
    capabilities.htcSupport = extendedCapabilities & (1 << 10);
    uint32_t transmitBeamformingCapabilities = stream.readUint32Le();
    capabilities.receiveNdp = transmitBeamformingCapabilities & (1u << 3);
    capabilities.transmitNdp = transmitBeamformingCapabilities & (1u << 4);
    capabilities.explicitCsiFeedback = (transmitBeamformingCapabilities >> 11) & 3;
    capabilities.explicitNoncompressedFeedback = (transmitBeamformingCapabilities >> 13) & 3;
    capabilities.explicitCompressedFeedback = (transmitBeamformingCapabilities >> 15) & 3;
    skipBytes(stream, 1);
    frame->setHtCapabilitiesPresent(true);
    frame->setHtCapabilities(capabilities);
}

static void readHtOperationElement(MemoryInputStream& stream, int length, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (length != 22) throw cRuntimeError("Malformed HT Operation element: length is %d", length);
    Ieee80211HtOperationElement operation;
    operation.primaryChannel = stream.readByte();
    uint64_t information = 0;
    for (int i = 0; i < 5; i++) information |= uint64_t(stream.readByte()) << (8 * i);
    operation.secondaryChannelOffset = information & 3;
    operation.staChannelWidth40MHz = information & (1 << 2);
    operation.protectionMode = (information >> 8) & 3;
    std::vector<uint8_t> basic(16);
    for (auto& byte : basic) byte = stream.readByte();
    for (int nss = 0; nss < 4; nss++) {
        operation.basicMaxMcsForNss[nss] = -1;
        for (int index = 0; index < 8; index++) if (getBits(basic, nss * 8 + index, 1)) operation.basicMaxMcsForNss[nss] = index;
    }
    frame->setHtOperationPresent(true);
    frame->setHtOperation(operation);
}

static void readVhtCapabilitiesElement(MemoryInputStream& stream, int length, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (length != 12) throw cRuntimeError("Malformed VHT Capabilities element: length is %d", length);
    Ieee80211VhtCapabilitiesElement capabilities;
    uint32_t information = stream.readUint32Le();
    capabilities.supportedChannelWidthSet = (information >> 2) & 3;
    capabilities.rxLdpc = information & (1 << 4);
    capabilities.shortGi80 = information & (1 << 5);
    capabilities.shortGi160 = information & (1 << 6);
    capabilities.suBeamformer = information & (1 << 11);
    capabilities.suBeamformee = information & (1 << 12);
    capabilities.beamformeeSts = ((information >> 13) & 7) + 1;
    capabilities.soundingDimensions = ((information >> 16) & 7) + 1;
    capabilities.muBeamformer = information & (1 << 19);
    capabilities.muBeamformee = information & (1 << 20);
    capabilities.maxAmpduLengthExponent = (information >> 23) & 7;
    uint16_t rxMap = stream.readUint16Le();
    uint16_t rxHighestAndNsts = stream.readUint16Le();
    uint16_t txMap = stream.readUint16Le();
    uint16_t txHighestAndExtendedNss = stream.readUint16Le();
    capabilities.rxHighestLongGiDataRateMbps = rxHighestAndNsts & 0x1fff;
    auto encodedMaxNstsTotal = (rxHighestAndNsts >> 13) & 7;
    capabilities.maxNstsTotal = encodedMaxNstsTotal == 0 ? 0 : encodedMaxNstsTotal + 1;
    capabilities.txHighestLongGiDataRateMbps = txHighestAndExtendedNss & 0x1fff;
    capabilities.extendedNssBwCapable = (txHighestAndExtendedNss & (1 << 13)) != 0;
    for (int i = 0; i < 8; i++) {
        capabilities.rxMaxMcsForNss[i] = decodeVhtMcs(rxMap >> (2 * i));
        capabilities.txMaxMcsForNss[i] = decodeVhtMcs(txMap >> (2 * i));
    }
    frame->setVhtCapabilitiesPresent(true);
    frame->setVhtCapabilities(capabilities);
}

static void readVhtOperationElement(MemoryInputStream& stream, int length, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (length != 5) throw cRuntimeError("Malformed VHT Operation element: length is %d", length);
    Ieee80211VhtOperationElement operation;
    operation.channelWidth = stream.readByte();
    operation.centerFrequencySegment0 = stream.readByte();
    operation.centerFrequencySegment1 = stream.readByte();
    uint16_t basic = stream.readUint16Le();
    for (int i = 0; i < 8; i++) operation.basicMaxMcsForNss[i] = decodeVhtMcs(basic >> (2 * i));
    frame->setVhtOperationPresent(true);
    frame->setVhtOperation(operation);
}

static void readHeCapabilitiesElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (payloadLength < 6 + 11 + 4)
        throw cRuntimeError("Malformed HE Capabilities element: length is %d", payloadLength);
    Ieee80211HeCapabilitiesElement capabilities;
    uint64_t macCapabilities = 0;
    for (int i = 0; i < 6; ++i)
        macCapabilities |= (uint64_t)stream.readByte() << (8 * i);
    std::vector<uint8_t> phyCapabilities(11);
    for (auto& byte : phyCapabilities)
        byte = stream.readByte();
    int mcsNssLength = payloadLength - 6 - 11;
    decodeHeMcsMap(stream.readUint16Le(), capabilities.rxMcsNss);
    decodeHeMcsMap(stream.readUint16Le(), capabilities.txMcsNss);
    if (mcsNssLength > 4)
        skipBytes(stream, mcsNssLength - 4);

    int supportedChannelWidthSet = getBits(phyCapabilities, 1, 7);
    capabilities.supportedChannelWidth40MHz = (supportedChannelWidthSet & (1 << 1)) != 0;
    capabilities.twtRequester = getBits(macCapabilities, 1, 1) != 0;
    capabilities.twtResponder = getBits(macCapabilities, 2, 1) != 0;
    capabilities.broadcastTwt = getBits(macCapabilities, 18, 1) != 0;
    capabilities.dynamicFragmentationLevel = getBits(macCapabilities, 3, 2);
    capabilities.omControl = getBits(macCapabilities, 25, 1) != 0;
    capabilities.twoNav = getBits(macCapabilities, 42, 1) != 0;
    capabilities.erBss = getBits(macCapabilities, 43, 1) != 0;
    capabilities.ndpFeedbackReport = getBits(macCapabilities, 44, 1) != 0;
    capabilities.supportedChannelWidth80MHz = (supportedChannelWidthSet & (1 << 1)) != 0;
    capabilities.supportedChannelWidth160MHz = (supportedChannelWidthSet & (1 << 2)) != 0;
    capabilities.supportedChannelWidth80Plus80MHz = (supportedChannelWidthSet & (1 << 3)) != 0;
    capabilities.dlOfdma = true;
    capabilities.ulOfdma = getBits(macCapabilities, 26, 1) != 0 || getBits(macCapabilities, 19, 1) != 0;
    capabilities.ldpc = getBit(phyCapabilities, 13);
    capabilities.preamblePuncturing = getBit(phyCapabilities, 14);
    capabilities.fullBandwidthUlMuMimo = getBit(phyCapabilities, 22);
    capabilities.partialBandwidthUlMuMimo = getBit(phyCapabilities, 23);
    int dcmConstellation = getBits(phyCapabilities, 27, 2);
    capabilities.dcm = dcmConstellation != 0;
    capabilities.maxDcmConstellation = decodeDcmConstellation(dcmConstellation);
    capabilities.maxDcmNss = getBits(phyCapabilities, 29, 1) != 0 ? 2 : 1;
    capabilities.multiTidAggregationRx = getBits(macCapabilities, 12, 3) != 0;
    capabilities.multiTidAggregationTx = getBits(macCapabilities, 35, 1) != 0;
    capabilities.muBarTriggerRx = true;
    capabilities.heTbBlockAckTx = getBits(macCapabilities, 17, 1) != 0;
    capabilities.maxAmpduLengthExponent = 3 + getBits(macCapabilities, 27, 2);
    capabilities.maxMpduLength = 11454;
    capabilities.maxBlockAckBufferSize = 64;
    capabilities.ru26Tone = true;
    capabilities.ru52Tone = true;
    capabilities.ru106Tone = true;
    capabilities.ru242Tone = true;
    int dcmMaxRu = getBits(phyCapabilities, 70, 2);
    capabilities.ru484Tone = dcmMaxRu >= 1;
    capabilities.ru996Tone = dcmMaxRu >= 2;
    capabilities.ru1992Tone = dcmMaxRu >= 3;
    capabilities.dlMuMimoBeamformer = getBit(phyCapabilities, 75);
    capabilities.dlMuMimoBeamformee = getBit(phyCapabilities, 76);
    capabilities.soundingDimensions = getBits(phyCapabilities, 77, 3);
    capabilities.beamformeeSts20Mhz = getBits(phyCapabilities, 80, 3);
    capabilities.beamformeeStsAbove20Mhz = getBits(phyCapabilities, 83, 3);
    capabilities.feedbackMode = getBits(phyCapabilities, 86, 2);
    frame->setHeCapabilitiesPresent(true);
    frame->setHeCapabilities(capabilities);
}

static void readHeOperationElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (payloadLength < 3 + 1 + 2)
        throw cRuntimeError("Malformed HE Operation element: length is %d", payloadLength);
    Ieee80211HeOperationElement operation;
    uint32_t operationParameters = stream.readByte();
    operationParameters |= (uint32_t)stream.readByte() << 8;
    operationParameters |= (uint32_t)stream.readByte() << 16;
    int defaultPeDuration = operationParameters & 0x7;
    operation.defaultPeDurationPresent = defaultPeDuration != 0;
    operation.defaultPeDurationUs = defaultPeDuration * 4;
    operation.erSuDisable = (operationParameters & (1 << 16)) != 0;
    operation.bssColor = stream.readByte() & 0x3f;
    operation.basicHeMcsNss = stream.readUint16Le();
    operation.operatingChannelWidthMHz = 20;
    if (payloadLength > 6)
        skipBytes(stream, payloadLength - 6);
    frame->setHeOperationPresent(true);
    frame->setHeOperation(operation);
}

static void readHe6GhzBandCapabilitiesElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (payloadLength != 2)
        throw cRuntimeError("Malformed HE 6 GHz Band Capabilities element: length is %d", payloadLength);
    uint16_t capabilitiesInformation = stream.readUint16Le();
    Ieee80211He6GhzBandCapabilitiesElement capabilities;
    capabilities.minimumMpduStartSpacing = capabilitiesInformation & 0x7;
    capabilities.maxAmpduLengthExponent = (capabilitiesInformation >> 3) & 0x7;
    capabilities.maxMpduLength = decodeMaxMpduLength((capabilitiesInformation >> 6) & 0x3);
    frame->setHe6GhzBandCapabilitiesPresent(true);
    frame->setHe6GhzBandCapabilities(capabilities);
}

static void readBroadcastTwtElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    if (payloadLength < 16 || (payloadLength - 1) % 15 != 0)
        throw cRuntimeError("Malformed Broadcast TWT element: length is %d", payloadLength);
    uint8_t control = stream.readByte();
    if ((control & 0x08) == 0)
        throw cRuntimeError("Received non-broadcast TWT element in Beacon");
    int count = (payloadLength - 1) / 15;
    if ((control >> 4) != count - 1)
        throw cRuntimeError("Malformed Broadcast TWT element parameter-set count");
    frame->setBroadcastTwtPresent(true);
    frame->setBroadcastTwtSchedulesArraySize(count);
    for (int i = 0; i < count; ++i) {
        auto& schedule = frame->getBroadcastTwtSchedulesForUpdate(i);
        schedule.targetWakeTime = SimTime((int64_t)stream.readUint64Le(), SIMTIME_US);
        schedule.wakeDuration = SimTime((int64_t)stream.readByte() * 256, SIMTIME_US);
        uint64_t mantissa = stream.readUint16Le();
        uint16_t requestType = stream.readUint16Le();
        schedule.triggerEnabled = requestType & 1;
        schedule.implicit = requestType & 2;
        schedule.announced = false;
        schedule.wakeInterval = SimTime((int64_t)mantissa * (uint64_t(1) << ((requestType >> 7) & 0x1f)), SIMTIME_US);
        uint16_t broadcastInfo = stream.readUint16Le();
        schedule.broadcastId = broadcastInfo & 0x1f;
        schedule.persistence = (broadcastInfo >> 8) & 0xff;
    }
}

static void readEhtCapabilitiesElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    // Minimal read for EHT Capabilities
    uint16_t macCap = stream.readUint16Le();
    uint32_t phyCap = stream.readUint32Le();
    stream.readUint32Le();
    stream.readUint32Le();
    // Assuming 14 bytes read, skip rest
    int readLen = 14;
    for (int i = 0; i < payloadLength - readLen; i++) stream.readByte();
    
    Ieee80211EhtCapabilitiesElement cap;
    cap.support4096Qam = getBits(macCap, 0, 1);
    cap.mlo = getBits(macCap, 1, 1);
    cap.str = getBits(macCap, 2, 1);
    cap.emlsr = getBits(macCap, 3, 1);
    cap.emlmr = getBits(macCap, 4, 1);
    cap.maxAmpduLengthExponent = getBits(macCap, 5, 2);
    cap.supportedChannelWidth320MHz = getBits(phyCap, 0, 1);
    cap.preamblePuncturing = getBits(phyCap, 1, 1);
    cap.dlOfdma = getBits(phyCap, 2, 1);
    cap.ulOfdma = getBits(phyCap, 3, 1);
    cap.dlMuMimo = getBits(phyCap, 4, 1);
    cap.ulMuMimo = getBits(phyCap, 5, 1);
    cap.ldpc = getBits(phyCap, 6, 1);
    cap.ehtDup6GHz = getBits(phyCap, 7, 1);
    
    frame->setEhtCapabilitiesPresent(true);
    frame->setEhtCapabilities(cap);
}

static void readEhtOperationElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    Ieee80211EhtOperationElement op;
    op.operatingChannelWidthMHz = stream.readUint16Le();
    op.disabledSubchannelBitmap = stream.readUint16Le();
    op.basicEhtMcsNss = stream.readByte();
    op.mcs15Disabled = stream.readByte() != 0;
    for (int i = 0; i < payloadLength - 6; i++) stream.readByte();
    
    frame->setEhtOperationPresent(true);
    frame->setEhtOperation(op);
}

static void readMultiLinkElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    Ieee80211MultiLinkElement ml;
    ml.mldMacAddress = stream.readMacAddress();
    ml.isApMld = stream.readByte() != 0;
    ml.linkId = stream.readUint16Le();
    for (int i = 0; i < payloadLength - 9; i++) stream.readByte();
    
    frame->setMultiLinkElementPresent(true);
    frame->setMultiLinkElement(ml);
}

static void readTidToLinkMappingElement(MemoryInputStream& stream, int payloadLength, const Ptr<Ieee80211MgmtFrame>& frame)
{
    Ieee80211TidToLinkMappingElement mapping;
    for (int i = 0; i < 8; i++) {
        mapping.mapping[i] = stream.readUint16Le();
    }
    for (int i = 0; i < payloadLength - 16; i++) stream.readByte();
    
    frame->setTidToLinkMappingPresent(true);
    frame->setTidToLinkMapping(mapping);
}

static void deserialize(MemoryInputStream& stream, const Ptr<Ieee80211MgmtFrame>& frame)
{
    while (stream.getRemainingLength() >= B(2)) {
        int elementId = stream.readByte();
        int length = stream.readByte();
        if (stream.getRemainingLength() < B(length))
            throw cRuntimeError("Malformed IEEE 802.11 management element: id=%d length=%d remaining=%" PRId64,
                    elementId, length, stream.getRemainingLength().get<B>());
        if (elementId == ELEMENT_ID_TWT) {
            readBroadcastTwtElement(stream, length, frame);
        }
        else if (elementId == ELEMENT_ID_EXTENSION && length >= 1) {
            int extensionId = stream.readByte();
            int payloadLength = length - 1;
            switch (extensionId) {
                case ELEMENT_ID_EXTENSION_HE_CAPABILITIES:
                    readHeCapabilitiesElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_HE_OPERATION:
                    readHeOperationElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_HE_6GHZ_BAND_CAPABILITIES:
                    readHe6GhzBandCapabilitiesElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_EHT_CAPABILITIES:
                    readEhtCapabilitiesElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_EHT_OPERATION:
                    readEhtOperationElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_MULTI_LINK:
                    readMultiLinkElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_TID_TO_LINK_MAPPING:
                    readTidToLinkMappingElement(stream, payloadLength, frame);
                    break;
                default:
                    skipBytes(stream, payloadLength);
                    break;
            }
        }
        else
            skipBytes(stream, length);
    }
}

static void readHeMgmtElements(MemoryInputStream& stream, const Ptr<Ieee80211MgmtFrame>& frame)
{
    while (stream.getRemainingLength() >= B(2)) {
        int elementId = stream.readByte();
        int length = stream.readByte();
        if (stream.getRemainingLength() < B(length))
            throw cRuntimeError("Malformed IEEE 802.11 management element: id=%d length=%d remaining=%" PRId64,
                    elementId, length, stream.getRemainingLength().get<B>());
        if (elementId == 50) {
            if (length == 0)
                throw cRuntimeError("Malformed Extended Supported Rates element length: 0");
            Ieee80211ExtendedSupportedRatesElement rates;
            rates.numRates = length;
            for (int i = 0; i < rates.numRates; ++i) {
                auto encoded = stream.readByte();
                rates.rates[i].rate = encoded & 0x7f;
                if (rates.rates[i].rate == 0)
                    throw cRuntimeError("Malformed Extended Supported Rates rate code: %d", encoded);
                rates.rates[i].basic = (encoded & 0x80) != 0;
            }
            if (auto value = dynamicPtrCast<Ieee80211ProbeRequestFrame>(frame)) value->setExtendedSupportedRates(rates);
            else if (auto value = dynamicPtrCast<Ieee80211AssociationRequestFrame>(frame)) value->setExtendedSupportedRates(rates);
            else if (auto value = dynamicPtrCast<Ieee80211AssociationResponseFrame>(frame)) value->setExtendedSupportedRates(rates);
            else if (auto value = dynamicPtrCast<Ieee80211BeaconFrame>(frame)) value->setExtendedSupportedRates(rates);
        }
        else if (elementId == ELEMENT_ID_HT_CAPABILITIES) readHtCapabilitiesElement(stream, length, frame);
        else if (elementId == ELEMENT_ID_HT_OPERATION) readHtOperationElement(stream, length, frame);
        else if (elementId == ELEMENT_ID_VHT_CAPABILITIES) readVhtCapabilitiesElement(stream, length, frame);
        else if (elementId == ELEMENT_ID_VHT_OPERATION) readVhtOperationElement(stream, length, frame);
        else if (elementId == ELEMENT_ID_TWT) {
            readBroadcastTwtElement(stream, length, frame);
        }
        else if (elementId == ELEMENT_ID_EXTENSION && length >= 1) {
            int extensionId = stream.readByte();
            int payloadLength = length - 1;
            switch (extensionId) {
                case ELEMENT_ID_EXTENSION_HE_CAPABILITIES:
                    readHeCapabilitiesElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_HE_OPERATION:
                    readHeOperationElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_HE_6GHZ_BAND_CAPABILITIES:
                    readHe6GhzBandCapabilitiesElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_EHT_CAPABILITIES:
                    readEhtCapabilitiesElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_EHT_OPERATION:
                    readEhtOperationElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_MULTI_LINK:
                    readMultiLinkElement(stream, payloadLength, frame);
                    break;
                case ELEMENT_ID_EXTENSION_TID_TO_LINK_MAPPING:
                    readTidToLinkMappingElement(stream, payloadLength, frame);
                    break;
                default:
                    skipBytes(stream, payloadLength);
                    break;
            }
        }
        else
            skipBytes(stream, length);
    }
}

static void writeLegacyRates(MemoryOutputStream& stream,
        const Ieee80211SupportedRatesElement& supportedRates,
        const Ieee80211ExtendedSupportedRatesElement& extendedSupportedRates)
{
    if (supportedRates.numRates < 1 || supportedRates.numRates > 8)
        throw cRuntimeError("Supported Rates element length must be between 1 and 8, got %d",
                supportedRates.numRates);
    if (extendedSupportedRates.numRates < 0 || extendedSupportedRates.numRates > 255)
        throw cRuntimeError("Extended Supported Rates element length must be between 0 and 255, got %d",
                extendedSupportedRates.numRates);
    auto validateRate = [](const Ieee80211LegacyRate& rate) {
        if (rate.rate < 1 || rate.rate > 127)
            throw cRuntimeError("Legacy rate code must be between 1 and 127, got %d",
                    rate.rate);
    };
    stream.writeByte(1);
    stream.writeByte(supportedRates.numRates);
    for (int i = 0; i < supportedRates.numRates; ++i) {
        validateRate(supportedRates.rates[i]);
        stream.writeByte(supportedRates.rates[i].rate |
                (supportedRates.rates[i].basic ? 0x80 : 0));
    }
    if (extendedSupportedRates.numRates != 0) {
        stream.writeByte(50);
        stream.writeByte(extendedSupportedRates.numRates);
        for (int i = 0; i < extendedSupportedRates.numRates; ++i) {
            validateRate(extendedSupportedRates.rates[i]);
            stream.writeByte(extendedSupportedRates.rates[i].rate |
                    (extendedSupportedRates.rates[i].basic ? 0x80 : 0));
        }
    }
}

static Ieee80211SupportedRatesElement readSupportedRates(MemoryInputStream& stream)
{
    if (stream.readByte() != 1)
        throw cRuntimeError("Expected Supported Rates element");
    Ieee80211SupportedRatesElement rates;
    rates.numRates = stream.readByte();
    if (rates.numRates < 1 || rates.numRates > 8)
        throw cRuntimeError("Malformed Supported Rates element length: %d", rates.numRates);
    for (int i = 0; i < rates.numRates; ++i) {
        auto encoded = stream.readByte();
        rates.rates[i].rate = encoded & 0x7f;
        if (rates.rates[i].rate == 0)
            throw cRuntimeError("Malformed Supported Rates rate code: %d", encoded);
        rates.rates[i].basic = (encoded & 0x80) != 0;
    }
    return rates;
}

void Ieee80211MgmtFrameSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    if (auto authenticationFrame = dynamicPtrCast<const Ieee80211AuthenticationFrame>(chunk)) {
//        type = ST_AUTHENTICATION;
        // 1    Authentication algorithm number
        stream.writeUint16Be(0);
        // 2    Authentication transaction sequence number
        stream.writeUint16Be(authenticationFrame->getSequenceNumber());
        // 3    Status code                                 The status code information is reserved in certain Authentication frames as defined in Table 7-17.
        stream.writeUint16Be(authenticationFrame->getStatusCode());
        // 4    Challenge text                              The challenge text information is present only in certain Authentication frames as defined in Table 7-17.
        // Last Vendor Specific                             One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto deauthenticationFrame = dynamicPtrCast<const Ieee80211DeauthenticationFrame>(chunk)) {
//        type = ST_DEAUTHENTICATION;
        stream.writeUint16Be(deauthenticationFrame->getReasonCode());
    }
    else if (auto disassociationFrame = dynamicPtrCast<const Ieee80211DisassociationFrame>(chunk)) {
//        type = ST_DISASSOCIATION;
        stream.writeUint16Be(disassociationFrame->getReasonCode());
    }
    else if (auto probeRequestFrame = dynamicPtrCast<const Ieee80211ProbeRequestFrame>(chunk)) {
//        type = ST_PROBEREQUEST;
        // 1    SSID
        const char *SSID = probeRequestFrame->getSSID();
        unsigned int length = strlen(SSID);
        stream.writeByte(0); // FIXME dummy, what is it?
        stream.writeByte(length);
        stream.writeBytes((uint8_t *)SSID, B(length));
        // 2    Supported rates
        writeLegacyRates(stream, probeRequestFrame->getSupportedRates(),
                probeRequestFrame->getExtendedSupportedRates());
        writeHeMgmtElements(stream, probeRequestFrame);
        // 3    Request information         May be included if dot11MultiDomainCapabilityEnabled is true.
        // 4    Extended Supported Rates    The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // Last Vendor Specific             One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto associationRequestFrame = dynamicPtrCast<const Ieee80211AssociationRequestFrame>(chunk);
            associationRequestFrame != nullptr &&
            dynamicPtrCast<const Ieee80211ReassociationRequestFrame>(chunk) == nullptr) {
//        type = ST_ASSOCIATIONREQUEST;
        // 1    Capability
        stream.writeUint16Be(0); // FIXME
        // 2    Listen interval
        stream.writeUint16Be(0); // FIXME
        // 3    SSID
        const char *SSID = associationRequestFrame->getSSID();
        unsigned int length = strlen(SSID);
        stream.writeByte(0); // FIXME dummy, what is it?
        stream.writeByte(length);
        stream.writeBytes((uint8_t *)SSID, B(length));
        // 4    Supported rates
        writeLegacyRates(stream, associationRequestFrame->getSupportedRates(),
                associationRequestFrame->getExtendedSupportedRates());
        writeHeMgmtElements(stream, associationRequestFrame);
        // 5    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 6    Power Capability           The Power Capability element shall be present if dot11SpectrumManagementRequired is true.
        // 7    Supported Channel          The Supported Channels element shall be present if dot11SpectrumManagementRequired is true.
        // 8    RSN                        The RSN information element is only present within Association Request frames generated by STAs that have dot11RSNAEnabled set to TRUE.
        // 9    QoS Capability             The QoS Capability element is present when dot11QosOption- Implemented is true.
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto reassociationRequestFrame = dynamicPtrCast<const Ieee80211ReassociationRequestFrame>(chunk)) {
//        type = ST_REASSOCIATIONREQUEST;
        // 1    Capability
        stream.writeUint16Be(0); // FIXME
        // 2    Listen interval
        stream.writeUint16Be(0); // FIXME
        // 3    Current AP address
        stream.writeMacAddress(reassociationRequestFrame->getCurrentAP());
        // 4    SSID
        const char *SSID = reassociationRequestFrame->getSSID();
        unsigned int length = strlen(SSID);
        // FIXME buffer.writeByte(buf + packetLength, ???);
        stream.writeByte(0); // FIXME
        stream.writeByte(length);
        stream.writeBytes((uint8_t *)SSID, B(length));
        // 5    Supported rates
        writeLegacyRates(stream, reassociationRequestFrame->getSupportedRates(),
                reassociationRequestFrame->getExtendedSupportedRates());
        writeHeMgmtElements(stream, reassociationRequestFrame);
        // 6    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 7    Power Capability           The Power Capability element shall be present if dot11SpectrumManagementRequired is true.
        // 8    Supported Channels         The Supported Channels element shall be present if dot11SpectrumManagementRequired is true.
        // 9    RSN                        The RSN information element is only present within Reassociation Request frames generated by STAs that have dot11RSNAEnabled set to TRUE.
        // 10   QoS Capability             The QoS Capability element is present when dot11QosOption- Implemented is true.
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto associationResponseFrame = dynamicPtrCast<const Ieee80211AssociationResponseFrame>(chunk);
            associationResponseFrame != nullptr &&
            dynamicPtrCast<const Ieee80211ReassociationResponseFrame>(chunk) == nullptr) {
//        type = ST_ASSOCIATIONRESPONSE;
        // 1    Capability
        stream.writeUint16Be(0); // FIXME
        // 2    Status code
        stream.writeUint16Be(associationResponseFrame->getStatusCode());
        // 3    AID
        stream.writeUint16Be(associationResponseFrame->getAid());
        // 4    Supported rates
        writeLegacyRates(stream, associationResponseFrame->getSupportedRates(),
                associationResponseFrame->getExtendedSupportedRates());
        writeHeMgmtElements(stream, associationResponseFrame);
        // 5    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 6    EDCA Parameter Set
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto reassociationResponseFrame = dynamicPtrCast<const Ieee80211ReassociationResponseFrame>(chunk)) {
//        type = ST_REASSOCIATIONRESPONSE;
        // 1    Capability
        stream.writeUint16Be(0); // FIXME
        // 2    Status code
        stream.writeUint16Be(reassociationResponseFrame->getStatusCode());
        // 3    AID
        stream.writeUint16Be(reassociationResponseFrame->getAid());
        // 4    Supported rates
        writeLegacyRates(stream, reassociationResponseFrame->getSupportedRates(),
                reassociationResponseFrame->getExtendedSupportedRates());
        writeHeMgmtElements(stream, reassociationResponseFrame);
        // 5    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 6    EDCA Parameter Set
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto beaconFrame = dynamicPtrCast<const Ieee80211BeaconFrame>(chunk);
            beaconFrame != nullptr &&
            dynamicPtrCast<const Ieee80211ProbeResponseFrame>(chunk) == nullptr) {
//        type = ST_BEACON;
        // 1    Timestamp
        stream.writeUint64Be(simTime().raw()); // FIXME
        // 2    Beacon interval
        stream.writeUint16Be((uint16_t)(beaconFrame->getBeaconInterval().inUnit(SIMTIME_US) / 1024));
        // 3    Capability
        stream.writeUint16Be(0); // FIXME set  capability
        // 4    Service Set Identifier (SSID)
        const char *SSID = beaconFrame->getSSID();
        unsigned int length = strlen(SSID);
        stream.writeByte(0); // FIXME
        stream.writeByte(length);
        stream.writeBytes((uint8_t *)SSID, B(length));
        // 5    Supported rates
        writeLegacyRates(stream, beaconFrame->getSupportedRates(),
                beaconFrame->getExtendedSupportedRates());
        writeHeMgmtElements(stream, beaconFrame);
        // 6    Frequency-Hopping (FH) Parameter Set   The FH Parameter Set information element is present within Beacon frames generated by STAs using FH PHYs.
        // 7    DS Parameter Set                       The DS Parameter Set information element is present within Beacon frames generated by STAs using Clause 15, Clause 18, and Clause 19 PHYs.
        // 8    CF Parameter Set                       The CF Parameter Set information element is present only within Beacon frames generated by APs supporting a PCF.
        // 9    IBSS Parameter Set                     The IBSS Parameter Set information element is present only within Beacon frames generated by STAs in an IBSS.
        // 10   Traffic indication map (TIM)           The TIM information element is present only within Beacon frames generated by APs.
        // 11   Country                                The Country information element shall be present when dot11MultiDomainCapabilityEnabled is true or dot11SpectrumManagementRequired is true.
        // 12   FH Parameters                          FH Parameters as specified in 7.3.2.10 may be included if dot11MultiDomainCapabilityEnabled is true.
        // 13   FH Pattern Table                       FH Pattern Table information as specified in 7.3.2.11 may be included if dot11MultiDomainCapabilityEnabled is true.
        // 14   Power Constraint                       Power Constraint element shall be present if dot11SpectrumManagementRequired is true.
        // 15   Channel Switch Announcement            Channel Switch Announcement element may be present if dot11SpectrumManagementRequired is true.
        // 16   Quiet                                  Quiet element may be present if dot11SpectrumManagementRequired is true.
        // 17   IBSS DFS                               IBSS DFS element shall be present if dot11SpectrumManagementRequired is true in an IBSS.
        // 18   TPC Report                             TPC Report element shall be present if dot11SpectrumManagementRequired is true.
        // 19   ERP Information                        The ERP Information element is present within Beacon frames generated by STAs using extended rate PHYs (ERPs) defined in Clause 19 and is optionally present in other cases.
        // 20   Extended Supported Rates               The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 21   RSN                                    The RSN information element shall be present within Beacon frames generated by STAs that have dot11RSNAEnabled set to TRUE.
        // 22   BSS Load                               The BSS Load element is present when dot11QosOption- Implemented and dot11QBSSLoadImplemented are both true.
        // 23   EDCA Parameter Set                     The EDCA Parameter Set element is present when dot11QosOptionImplemented is true and the QoS Capability element is not present.
        // 24   QoS Capability                         The QoS Capability element is present when dot11QosOption- Implemented is true and EDCA Parameter Set element is not present.
        // Last Vendor Specific                        One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto probeResponseFrame = dynamicPtrCast<const Ieee80211ProbeResponseFrame>(chunk)) {
//        type = ST_PROBERESPONSE;
        // 1      Timestamp
        stream.writeUint64Be(simTime().raw()); // FIXME
        // 2      Beacon interval
        stream.writeUint16Be((uint16_t)(probeResponseFrame->getBeaconInterval().inUnit(SIMTIME_US) / 1024));
        // 3      Capability
        stream.writeUint16Be(0); // FIXME
        // 4      SSID
        const char *SSID = probeResponseFrame->getSSID();
        unsigned int length = strlen(SSID);
        stream.writeByte(0); // FIXME
        stream.writeByte(length);
        stream.writeBytes((uint8_t *)SSID, B(length));
        // 5      Supported rates
        writeLegacyRates(stream, probeResponseFrame->getSupportedRates(),
                probeResponseFrame->getExtendedSupportedRates());
        writeHeMgmtElements(stream, probeResponseFrame);
        // 6      FH Parameter Set                The FH Parameter Set information element is present within Probe Response frames generated by STAs using FH PHYs.
        // 7      DS Parameter Set                The DS Parameter Set information element is present within Probe Response frames generated by STAs using Clause 15, Clause 18, and Clause 19 PHYs.
        // 8      CF Parameter Set                The CF Parameter Set information element is present only within Probe Response frames generated by APs supporting a PCF.
        // 9      IBSS Parameter Set              The IBSS Parameter Set information element is present only within Probe Response frames generated by STAs in an IBSS.
        // 10     Country                         Included if dot11MultiDomainCapabilityEnabled or dot11SpectrumManagementRequired is true.
        // 11     FH Parameters                   FH Parameters, as specified in 7.3.2.10, may be included if dot11MultiDomainCapabilityEnabled is true.
        // 12     FH Pattern Table                FH Pattern Table information, as specified in 7.3.2.11, may be included if dot11MultiDomainCapabilityEnabled is true.
        // 13     Power Constraint                Shall be included if dot11SpectrumManagementRequired is true.
        // 14     Channel Switch Announcement     May be included if dot11SpectrumManagementRequired is true.
        // 15     Quiet                           May be included if dot11SpectrumManagementRequired is true.
        // 16     IBSS DFS                        Shall be included if dot11SpectrumManagementRequired is true in an IBSS.
        // 17     TPC Report                      Shall be included if dot11SpectrumManagementRequired is true.
        // 18     ERP Information                 The ERP Information element is present within Probe Response frames generated by STAs using ERPs and is optionally present in other cases.
        // 19     Extended Supported Rates        The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 20     RSN                             The RSN information element is only present within Probe Response frames generated by STAs that have dot11RSNA- Enabled set to TRUE.
        // 21     BSS Load                        The BSS Load element is present when dot11QosOption- Implemented and dot11QBSSLoadImplemented are both true.
        // 22     EDCA Parameter Set              The EDCA Parameter Set element is present when dot11QosOptionImplemented is true.
        // Last�1 Vendor Specific                 One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements, except the Requested Information elements.
        // Last�n Requested information elements  Elements requested by the Request information element of the Probe Request frame.
    }
    else
        throw cRuntimeError("Cannot serialize frame");
}

const Ptr<Chunk> Ieee80211MgmtFrameSerializer::deserialize(MemoryInputStream& stream, const std::type_info& typeInfo) const
{
    typeInfoForDeserialize = &typeInfo;
    auto chunk = FieldsChunkSerializer::deserialize(stream, typeInfo);
    typeInfoForDeserialize = nullptr;
    return chunk;
}

const Ptr<Chunk> Ieee80211MgmtFrameSerializer::deserialize(MemoryInputStream& stream) const
{
    if (typeInfoForDeserialize == nullptr)
        throw cRuntimeError("Ieee80211MgmtFrameSerializer: deserialize(stream) without typeInfo is not supported");

    const std::type_info& typeInfo = *typeInfoForDeserialize;
    int type = -1;
    if (typeInfo == typeid(Ieee80211AuthenticationFrame))
        type = 0xB0;
    else if (typeInfo == typeid(Ieee80211DeauthenticationFrame))
        type = 0xC0;
    else if (typeInfo == typeid(Ieee80211DisassociationFrame))
        type = 0xA0;
    else if (typeInfo == typeid(Ieee80211ProbeRequestFrame))
        type = 0x40;
    else if (typeInfo == typeid(Ieee80211AssociationRequestFrame))
        type = 0x00;
    else if (typeInfo == typeid(Ieee80211ReassociationRequestFrame))
        type = 0x02;
    else if (typeInfo == typeid(Ieee80211AssociationResponseFrame))
        type = 0x01;
    else if (typeInfo == typeid(Ieee80211ReassociationResponseFrame))
        type = 0x03;
    else if (typeInfo == typeid(Ieee80211BeaconFrame))
        type = 0x80;
    else if (typeInfo == typeid(Ieee80211ProbeResponseFrame))
        type = 0x50;

    if (type == -1)
        throw cRuntimeError("Ieee80211MgmtFrameSerializer: unsupported typeInfo: %s", typeInfo.name());

    Ptr<Chunk> frame;
    switch (type) {
        case 0xB0: // ST_AUTHENTICATION
        {
            auto f = makeShared<Ieee80211AuthenticationFrame>();
            stream.readUint16Be();
            f->setSequenceNumber(stream.readUint16Be());
            f->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            frame = f;
            break;
        }

        case 0xC0: // ST_DEAUTHENTICATION
        {
            auto f = makeShared<Ieee80211DeauthenticationFrame>();
            f->setReasonCode((Ieee80211ReasonCode)stream.readUint16Be());
            frame = f;
            break;
        }

        case 0xA0: // ST_DISASSOCIATION
        {
            auto f = makeShared<Ieee80211DisassociationFrame>();
            f->setReasonCode((Ieee80211ReasonCode)stream.readUint16Be());
            frame = f;
            break;
        }

        case 0x40: // ST_PROBEREQUEST
        {
            auto f = makeShared<Ieee80211ProbeRequestFrame>();

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            f->setSSID(SSID);

            f->setSupportedRates(readSupportedRates(stream));
            readHeMgmtElements(stream, f);
            frame = f;
            break;
        }

        case 0x00: // ST_ASSOCIATIONREQUEST
        {
            auto f = makeShared<Ieee80211AssociationRequestFrame>();
            stream.readUint16Be();
            stream.readUint16Be();

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            f->setSSID(SSID);

            f->setSupportedRates(readSupportedRates(stream));
            readHeMgmtElements(stream, f);
            frame = f;
            break;
        }

        case 0x02: // ST_REASSOCIATIONREQUEST
        {
            auto f = makeShared<Ieee80211ReassociationRequestFrame>();
            stream.readUint16Be();
            stream.readUint16Be();

            f->setCurrentAP(stream.readMacAddress());

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            f->setSSID(SSID);

            f->setSupportedRates(readSupportedRates(stream));
            readHeMgmtElements(stream, f);
            frame = f;
            break;
        }

        case 0x01: // ST_ASSOCIATIONRESPONSE
        {
            auto f = makeShared<Ieee80211AssociationResponseFrame>();
            stream.readUint16Be();
            f->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            f->setAid(stream.readUint16Be());

            f->setSupportedRates(readSupportedRates(stream));
            readHeMgmtElements(stream, f);
            frame = f;
            break;
        }

        case 0x03: // ST_REASSOCIATIONRESPONSE
        {
            auto f = makeShared<Ieee80211ReassociationResponseFrame>();
            stream.readUint16Be();
            f->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            f->setAid(stream.readUint16Be());

            f->setSupportedRates(readSupportedRates(stream));
            readHeMgmtElements(stream, f);
            frame = f;
            break;
        }

        case 0x80: // ST_BEACON
        {
            auto f = makeShared<Ieee80211BeaconFrame>();

            simtime_t timetstamp;
            timetstamp.setRaw(stream.readUint64Be()); // TODO store timestamp

            f->setBeaconInterval(SimTime((int64_t)stream.readUint16Be() * 1024, SIMTIME_US));
            stream.readUint16Be(); // Capability

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            f->setSSID(SSID);

            f->setSupportedRates(readSupportedRates(stream));
            readHeMgmtElements(stream, f);
            frame = f;
            break;
        }

        case 0x50: // ST_PROBERESPONSE
        {
            auto f = makeShared<Ieee80211ProbeResponseFrame>();

            simtime_t timestamp;
            timestamp.setRaw(stream.readUint64Be()); // TODO store timestamp

            f->setBeaconInterval(SimTime((int64_t)stream.readUint16Be() * 1024, SIMTIME_US));
            stream.readUint16Be();

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            f->setSSID(SSID);

            f->setSupportedRates(readSupportedRates(stream));
            readHeMgmtElements(stream, f);
            frame = f;
            break;
        }

        default:
            throw cRuntimeError("Cannot deserialize frame");
    }

    return frame;
}

// IEEE 802.11-2024 §9.6.28 — HE Action frame body serializer.
//
// Both frame types share the same MAC-layer Action body layout:
//   Octet 0 : Category = 30 (HE)
//   Octet 1 : HE Action code
//               0 = HE Compressed Beamforming / CQI (§9.6.28.2)
//               1 = HE NDP Announcement          (§9.6.28.4)
//   Octet 2+: Frame-type-specific fields (see below)
//
// The Category and HE Action octets are written here (not in the MAC
// header serializer) because the INET HE-sounding frames use a base
// Ieee80211MgmtHeader chunk for the 24-byte 802.11 header and a separate
// Ieee80211HeNdpAnnouncement / Ieee80211HeCompressedBeamformingFeedback
// chunk for the action body.

static constexpr uint8_t HE_CATEGORY_CODE = 30;
static constexpr uint8_t HE_ACTION_COMPRESSED_BF = 0;
static constexpr uint8_t HE_ACTION_NDP_ANNOUNCEMENT = 1;

void Ieee80211HeSoundingMgmtFrameSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    if (auto ndpaFrame = dynamicPtrCast<const Ieee80211HeNdpAnnouncement>(chunk)) {
        b startPos = stream.getLength();
        // §9.6.28.4 HE NDP Announcement frame body
        stream.writeByte(HE_CATEGORY_CODE);
        stream.writeByte(HE_ACTION_NDP_ANNOUNCEMENT);
        stream.writeByte(ndpaFrame->getDialogToken());
        // Per-STA Info (2 bytes each) — Table 9-100
        for (unsigned int i = 0; i < ndpaFrame->getStationsArraySize(); ++i) {
            const auto& sta = ndpaFrame->getStations(i);
            uint16_t perStaInfo = (sta.aid & 0x7FFu) | ((uint16_t)(sta.feedbackType & 0xFu) << 11);
            stream.writeUint16Le(perStaInfo);
        }
        b written = stream.getLength() - startPos;
        b total = ndpaFrame->getChunkLength();
        if (total > written)
            stream.writeByteRepeatedly(0, (total - written).get<B>());
    }
    else if (auto feedbackFrame = dynamicPtrCast<const Ieee80211HeCompressedBeamformingFeedback>(chunk)) {
        b startPos = stream.getLength();
        // §9.6.28.2 HE Compressed Beamforming / CQI frame body
        stream.writeByte(HE_CATEGORY_CODE);
        stream.writeByte(HE_ACTION_COMPRESSED_BF);
        stream.writeByte(feedbackFrame->getDialogToken());
        // MIMO Control (3 bytes) — Table 9-99
        uint8_t bwCode = 0;
        double bw = feedbackFrame->getFeedbackBandwidth();
        if (bw >= 160e6) bwCode = 3;
        else if (bw >= 80e6) bwCode = 2;
        else if (bw >= 40e6) bwCode = 1;
        uint8_t ncIdx = (feedbackFrame->getNc() > 0) ? (feedbackFrame->getNc() - 1) & 0x7u : 0;
        uint8_t nrIdx = (feedbackFrame->getNr() > 0) ? (feedbackFrame->getNr() - 1) & 0x7u : 0;
        uint32_t mimoCtrl = (ncIdx)
                          | ((uint32_t)nrIdx << 3)
                          | ((uint32_t)(bwCode & 0x3u) << 6)
                          | (1u << 11)   // Feedback Type = MU
                          | (1u << 14)   // First Feedback Segment
                          | ((uint32_t)(feedbackFrame->getDialogToken() & 0x3Fu) << 16);
        stream.writeByte(mimoCtrl & 0xFFu);
        stream.writeByte((mimoCtrl >> 8) & 0xFFu);
        stream.writeByte((mimoCtrl >> 16) & 0xFFu);
        b written = stream.getLength() - startPos;
        b total = feedbackFrame->getChunkLength();
        if (total > written)
            stream.writeByteRepeatedly(0, (total - written).get<B>());
    }
    else
        throw cRuntimeError("Ieee80211HeSoundingMgmtFrameSerializer: cannot serialize unknown HE sounding frame type");
}

const Ptr<Chunk> Ieee80211HeSoundingMgmtFrameSerializer::deserialize(MemoryInputStream& stream) const
{
    uint8_t category = stream.readByte();
    uint8_t action   = stream.readByte();
    if (category != HE_CATEGORY_CODE)
        throw cRuntimeError("Ieee80211HeSoundingMgmtFrameSerializer: expected Category=%u, got %u",
                            HE_CATEGORY_CODE, category);

    if (action == HE_ACTION_NDP_ANNOUNCEMENT) {
        // §9.6.28.4 HE NDP Announcement
        auto frame = makeShared<Ieee80211HeNdpAnnouncement>();
        frame->setDialogToken(stream.readByte());
        // Remaining bytes are Per-STA Info fields (2 bytes each)
        std::vector<Ieee80211HeNdpStaInfo> stations;
        while (stream.getRemainingLength() >= B(2)) {
            uint16_t perStaInfo = stream.readUint16Le();
            if ((perStaInfo & 0x7FFu) == 0) // Padding
                continue;
            Ieee80211HeNdpStaInfo sta;
            sta.aid          = perStaInfo & 0x7FFu;
            sta.feedbackType = (perStaInfo >> 11) & 0xFu;
            sta.nc           = 0; // Nc not encoded in the per-STA Info wire field
            stations.push_back(sta);
        }
        frame->setStationsArraySize(stations.size());
        for (size_t i = 0; i < stations.size(); ++i)
            frame->setStations(i, stations[i]);
        if (stream.getRemainingLength() > b(0))
            stream.seek(stream.getLength());
        return frame;
    }
    else if (action == HE_ACTION_COMPRESSED_BF) {
        // §9.6.28.2 HE Compressed Beamforming / CQI
        auto frame = makeShared<Ieee80211HeCompressedBeamformingFeedback>();
        frame->setDialogToken(stream.readByte());
        uint32_t mimoCtrl = stream.readByte();
        mimoCtrl |= (uint32_t)stream.readByte() << 8;
        mimoCtrl |= (uint32_t)stream.readByte() << 16;
        frame->setNc((mimoCtrl & 0x7u) + 1);
        frame->setNr(((mimoCtrl >> 3) & 0x7u) + 1);
        uint8_t bwCode = (mimoCtrl >> 6) & 0x3u;
        frame->setFeedbackBandwidth((bwCode == 3) ? 160e6 : (bwCode == 2) ? 80e6 : (bwCode == 1) ? 40e6 : 20e6);
        frame->setValid(true);
        if (stream.getRemainingLength() > b(0))
            stream.seek(stream.getLength());
        return frame;
    }
    else
        throw cRuntimeError("Ieee80211HeSoundingMgmtFrameSerializer: unsupported HE Action code %u", action);
}

// IEEE Std 802.11-2024 9.6.22.1 through 9.6.22.3. Category 21 is VHT;
// Action 0 is VHT Compressed Beamforming and Action 1 is Group ID Management.
static constexpr uint8_t VHT_CATEGORY_CODE = 21;
static constexpr uint8_t VHT_ACTION_COMPRESSED_BEAMFORMING = 0;
static constexpr uint8_t VHT_ACTION_GROUP_ID_MANAGEMENT = 1;

static int getVhtFeedbackWidthCode(double bandwidth)
{
    if (bandwidth == 20e6)
        return 0;
    if (bandwidth == 40e6)
        return 1;
    if (bandwidth == 80e6)
        return 2;
    if (bandwidth == 160e6)
        return 3; // 160 MHz and 80+80 MHz share the VHT encoding
    throw cRuntimeError("Unsupported VHT feedback bandwidth %.0f Hz", bandwidth);
}

static double getVhtFeedbackBandwidth(uint8_t widthCode)
{
    switch (widthCode) {
        case 0: return 20e6;
        case 1: return 40e6;
        case 2: return 80e6;
        case 3: return 160e6;
        default: throw cRuntimeError("Reserved VHT feedback channel-width code %u", widthCode);
    }
}

static int getVhtGroupingCode(uint8_t grouping)
{
    switch (grouping) {
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        default: throw cRuntimeError("Unsupported VHT feedback subcarrier grouping Ng=%u", grouping);
    }
}

static uint8_t getVhtGrouping(uint8_t groupingCode)
{
    switch (groupingCode) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        default: throw cRuntimeError("Reserved VHT feedback grouping code %u", groupingCode);
    }
}

static int getVhtFeedbackSubcarrierCount(double bandwidth, uint8_t grouping)
{
    int widthCode = getVhtFeedbackWidthCode(bandwidth);
    if (widthCode == 0)
        return grouping == 1 ? 52 : grouping == 2 ? 30 : 16;
    if (widthCode == 1)
        return grouping == 1 ? 108 : grouping == 2 ? 58 : 30;
    if (widthCode == 2)
        return grouping == 1 ? 234 : grouping == 2 ? 122 : 62;
    return grouping == 1 ? 468 : grouping == 2 ? 244 : 124;
}

static int getVhtMuExclusiveSubcarrierCount(double bandwidth, uint8_t grouping)
{
    int widthCode = getVhtFeedbackWidthCode(bandwidth);
    if (widthCode == 0)
        return grouping == 1 ? 30 : grouping == 2 ? 16 : 10;
    if (widthCode == 1)
        return grouping == 1 ? 58 : grouping == 2 ? 30 : 16;
    if (widthCode == 2)
        return grouping == 1 ? 122 : grouping == 2 ? 62 : 32;
    return grouping == 1 ? 244 : grouping == 2 ? 124 : 64;
}

static int getVhtFeedbackAngleCount(uint8_t nr, uint8_t nc)
{
    if (nr < 2 || nr > 8 || nc < 1 || nc > nr)
        throw cRuntimeError("Invalid VHT feedback matrix dimensions Nr=%u Nc=%u", nr, nc);
    return nc * (2 * nr - nc - 1);
}

static size_t getVhtFeedbackMatrixBytes(const Ieee80211VhtCompressedBeamformingFeedback *feedback)
{
    const int angleBits = feedback->getFeedbackTypeMu() ?
            (feedback->getCodebookInformation() ? 8 : 6) :
            (feedback->getCodebookInformation() ? 5 : 3);
    const int bits = getVhtFeedbackSubcarrierCount(feedback->getFeedbackBandwidth(), feedback->getGrouping()) *
            getVhtFeedbackAngleCount(feedback->getNr(), feedback->getNc()) * angleBits;
    return (bits + 7) / 8;
}

static size_t getVhtMuExclusiveReportBytes(const Ieee80211VhtCompressedBeamformingFeedback *feedback)
{
    if (!feedback->getFeedbackTypeMu())
        return 0;
    const int bits = getVhtMuExclusiveSubcarrierCount(feedback->getFeedbackBandwidth(), feedback->getGrouping()) *
            feedback->getNc() * 4;
    return (bits + 7) / 8;
}

static void validateVhtFeedback(const Ieee80211VhtCompressedBeamformingFeedback *feedback)
{
    if (feedback->getSoundingDialogTokenNumber() > 63 || feedback->getRemainingFeedbackSegments() > 7)
        throw cRuntimeError("Malformed VHT Compressed Beamforming feedback control fields");
    getVhtFeedbackWidthCode(feedback->getFeedbackBandwidth());
    getVhtGroupingCode(feedback->getGrouping());
    getVhtFeedbackAngleCount(feedback->getNr(), feedback->getNc());
    if (feedback->getCompressedBeamformingReportLength() > feedback->getCompressedBeamformingReportArraySize())
        throw cRuntimeError("VHT compressed beamforming report length exceeds storage");
    const size_t reportBytes = feedback->getCompressedBeamformingReportLength() +
            feedback->getCompressedBeamformingReportExtensionArraySize();
    const size_t matrixBytes = getVhtFeedbackMatrixBytes(feedback);
    if (feedback->getRemainingFeedbackSegments() == 0 && feedback->getFirstFeedbackSegment() && reportBytes != matrixBytes)
        throw cRuntimeError("VHT compressed beamforming matrix has %zu bytes, expected %zu", reportBytes, matrixBytes);
    if (feedback->getAverageSnrAdditionalArraySize() + 1 != feedback->getNc())
        throw cRuntimeError("VHT compressed beamforming feedback requires one average SNR per Nc column");
    if (!feedback->getFeedbackTypeMu() && feedback->getMuExclusiveBeamformingReportArraySize() != 0)
        throw cRuntimeError("SU VHT feedback must not contain an MU Exclusive Beamforming Report");
    const size_t expectedMuBytes = getVhtMuExclusiveReportBytes(feedback);
    if (feedback->getFeedbackTypeMu() && feedback->getRemainingFeedbackSegments() == 0 && feedback->getFirstFeedbackSegment() &&
            feedback->getMuExclusiveBeamformingReportArraySize() != expectedMuBytes)
        throw cRuntimeError("VHT MU Exclusive Beamforming Report has %zu bytes, expected %zu", feedback->getMuExclusiveBeamformingReportArraySize(), expectedMuBytes);
}

void Ieee80211VhtActionFrameBodySerializer::serialize(MemoryOutputStream& stream,
        const Ptr<const Chunk>& chunk) const
{
    if (auto feedback = dynamicPtrCast<const Ieee80211VhtCompressedBeamformingFeedback>(chunk)) {
        validateVhtFeedback(feedback.get());
        stream.writeByte(VHT_CATEGORY_CODE);
        stream.writeByte(VHT_ACTION_COMPRESSED_BEAMFORMING);
        const uint32_t mimoControl = ((feedback->getNc() - 1) << 0) |
                ((feedback->getNr() - 1) << 3) |
                (getVhtFeedbackWidthCode(feedback->getFeedbackBandwidth()) << 6) |
                (getVhtGroupingCode(feedback->getGrouping()) << 8) |
                (feedback->getCodebookInformation() << 10) |
                (feedback->getFeedbackTypeMu() << 11) |
                (feedback->getRemainingFeedbackSegments() << 12) |
                (feedback->getFirstFeedbackSegment() << 15) |
                (static_cast<uint32_t>(feedback->getSoundingDialogTokenNumber()) << 18);
        stream.writeByte(mimoControl & 0xff);
        stream.writeByte((mimoControl >> 8) & 0xff);
        stream.writeByte((mimoControl >> 16) & 0xff);
        stream.writeByte(static_cast<uint8_t>(feedback->getAverageSnr()));
        for (size_t i = 0; i < feedback->getAverageSnrAdditionalArraySize(); i++)
            stream.writeByte(static_cast<uint8_t>(feedback->getAverageSnrAdditional(i)));
        for (size_t i = 0; i < feedback->getCompressedBeamformingReportLength(); i++)
            stream.writeByte(feedback->getCompressedBeamformingReport(i));
        for (size_t i = 0; i < feedback->getCompressedBeamformingReportExtensionArraySize(); i++)
            stream.writeByte(feedback->getCompressedBeamformingReportExtension(i));
        for (size_t i = 0; i < feedback->getMuExclusiveBeamformingReportArraySize(); i++)
            stream.writeByte(feedback->getMuExclusiveBeamformingReport(i));
    }
    else if (auto group = dynamicPtrCast<const Ieee80211VhtGroupIdManagement>(chunk)) {
        if (group->getChunkLength() != B(26))
            throw cRuntimeError("Malformed VHT Group ID Management body");
        stream.writeByte(VHT_CATEGORY_CODE);
        stream.writeByte(VHT_ACTION_GROUP_ID_MANAGEMENT);
        for (size_t i = 0; i < group->getMembershipStatusArraySize(); i++)
            stream.writeByte(group->getMembershipStatus(i));
        for (size_t i = 0; i < group->getUserPositionArraySize(); i++)
            stream.writeByte(group->getUserPosition(i));
    }
    else
        throw cRuntimeError("Unsupported VHT action-frame body type");
}

const Ptr<Chunk> Ieee80211VhtActionFrameBodySerializer::deserialize(MemoryInputStream& stream) const
{
    if (stream.getRemainingLength() < B(1))
        throw cRuntimeError("Truncated VHT action-frame body");
    auto category = stream.readByte();
    if (category != VHT_CATEGORY_CODE)
        throw cRuntimeError("Expected VHT action category 21, received %u", category);
    auto action = stream.readByte();
    if (action == VHT_ACTION_COMPRESSED_BEAMFORMING) {
        if (stream.getRemainingLength() < B(4))
            throw cRuntimeError("Truncated VHT Compressed Beamforming body");
        uint32_t mimoControl = stream.readByte();
        mimoControl |= static_cast<uint32_t>(stream.readByte()) << 8;
        mimoControl |= static_cast<uint32_t>(stream.readByte()) << 16;
        if (mimoControl & (3u << 16))
            throw cRuntimeError("Reserved bits set in VHT MIMO Control field");
        auto feedback = makeShared<Ieee80211VhtCompressedBeamformingFeedback>();
        feedback->setNc((mimoControl & 0x7) + 1);
        feedback->setNr(((mimoControl >> 3) & 0x7) + 1);
        feedback->setFeedbackBandwidth(getVhtFeedbackBandwidth((mimoControl >> 6) & 0x3));
        feedback->setGrouping(getVhtGrouping((mimoControl >> 8) & 0x3));
        feedback->setCodebookInformation((mimoControl >> 10) & 0x1);
        feedback->setFeedbackTypeMu((mimoControl >> 11) & 0x1);
        feedback->setRemainingFeedbackSegments((mimoControl >> 12) & 0x7);
        feedback->setFirstFeedbackSegment((mimoControl >> 15) & 0x1);
        feedback->setSoundingDialogTokenNumber((mimoControl >> 18) & 0x3f);
        feedback->setAverageSnr(static_cast<int8_t>(stream.readByte()));
        feedback->setAverageSnrAdditionalArraySize(feedback->getNc() - 1);
        for (size_t i = 0; i < feedback->getAverageSnrAdditionalArraySize(); i++)
            feedback->setAverageSnrAdditional(i, static_cast<int8_t>(stream.readByte()));
        const size_t payloadBytes = stream.getRemainingLength().get<B>();
        const size_t expectedMatrixBytes = getVhtFeedbackMatrixBytes(feedback.get());
        const size_t matrixBytes = feedback->getRemainingFeedbackSegments() != 0 || !feedback->getFirstFeedbackSegment() ?
                payloadBytes : expectedMatrixBytes;
        if (payloadBytes < matrixBytes)
            throw cRuntimeError("Truncated VHT compressed beamforming report");
        feedback->setCompressedBeamformingReportLength(std::min<size_t>(12, matrixBytes));
        feedback->setCompressedBeamformingReportExtensionArraySize(matrixBytes > 12 ? matrixBytes - 12 : 0);
        for (size_t i = 0; i < feedback->getCompressedBeamformingReportLength(); i++)
            feedback->setCompressedBeamformingReport(i, stream.readByte());
        for (size_t i = 0; i < feedback->getCompressedBeamformingReportExtensionArraySize(); i++)
            feedback->setCompressedBeamformingReportExtension(i, stream.readByte());
        feedback->setMuExclusiveBeamformingReportArraySize(payloadBytes - matrixBytes);
        for (size_t i = 0; i < feedback->getMuExclusiveBeamformingReportArraySize(); i++)
            feedback->setMuExclusiveBeamformingReport(i, stream.readByte());
        feedback->setChunkLength(B(2 + 3 + 1 + (feedback->getNc() - 1) + payloadBytes));
        return feedback;
    }
    else if (action == VHT_ACTION_GROUP_ID_MANAGEMENT) {
        if (stream.getRemainingLength() != B(24))
            throw cRuntimeError("VHT Group ID Management body must be 26 octets");
        auto group = makeShared<Ieee80211VhtGroupIdManagement>();
        for (size_t i = 0; i < group->getMembershipStatusArraySize(); i++)
            group->setMembershipStatus(i, stream.readByte());
        for (size_t i = 0; i < group->getUserPositionArraySize(); i++)
            group->setUserPosition(i, stream.readByte());
        return group;
    }
    throw cRuntimeError("Reserved VHT Action value %u", action);
}

static uint8_t encodeHtFeedbackGrouping(uint8_t grouping)
{
    if (grouping == 1) return 0;
    if (grouping == 2) return 1;
    if (grouping == 4) return 2;
    throw cRuntimeError("HT feedback grouping must be 1, 2, or 4");
}

static uint8_t decodeHtFeedbackGrouping(uint8_t value)
{
    if (value > 2)
        throw cRuntimeError("Reserved HT feedback grouping value");
    return 1 << value;
}

static uint8_t encodeHtCoefficientSize(uint8_t action, uint8_t size)
{
    if (action == 4) {
        if (size == 4) return 0;
        if (size == 5) return 1;
        if (size == 6) return 2;
        if (size == 8) return 3;
    }
    else if (action == 5) {
        if (size == 4) return 0;
        if (size == 2) return 1;
        if (size == 6) return 2;
        if (size == 8) return 3;
    }
    else if (action == 6 && size == 0)
        return 0; // Reserved in a Compressed Beamforming frame.
    throw cRuntimeError("Invalid coefficient size for HT Action %u", action);
}

static uint8_t decodeHtCoefficientSize(uint8_t action, uint8_t value)
{
    static const uint8_t csiSizes[] = {4, 5, 6, 8};
    static const uint8_t noncompressedSizes[] = {4, 2, 6, 8};
    if (action == 4) return csiSizes[value];
    if (action == 5) return noncompressedSizes[value];
    if (action == 6 && value == 0) return 0;
    throw cRuntimeError("Reserved coefficient-size bits in HT Compressed Beamforming frame");
}

void Ieee80211HtActionFrameBodySerializer::serialize(MemoryOutputStream& stream,
        const Ptr<const Chunk>& chunk) const
{
    auto feedback = dynamicPtrCast<const Ieee80211HtMimoFeedback>(chunk);
    if (feedback == nullptr)
        throw cRuntimeError("Unsupported HT action-frame body type");
    uint8_t action = dynamicPtrCast<const Ieee80211HtCsiFeedback>(chunk) ? 4 :
            dynamicPtrCast<const Ieee80211HtNoncompressedBeamformingFeedback>(chunk) ? 5 :
            dynamicPtrCast<const Ieee80211HtCompressedBeamformingFeedback>(chunk) ? 6 : 0;
    if (action == 0 || feedback->getNc() < 1 || feedback->getNc() > 4 ||
            feedback->getNr() < 2 || feedback->getNr() > 4 ||
            feedback->getNc() > feedback->getNr() ||
            (feedback->getChannelWidth() != 20e6 && feedback->getChannelWidth() != 40e6) ||
            feedback->getCodebookInformation() > 3 ||
            feedback->getRemainingMatrixSegments() > 7 ||
            feedback->getReportArraySize() == 0 || feedback->getReportArraySize() > 64)
        throw cRuntimeError("Invalid HT CSI/beamforming feedback fields");
    if ((action == 4 || action == 5) && feedback->getCodebookInformation() != 0)
        throw cRuntimeError("Codebook Information is reserved for HT CSI/noncompressed feedback");
    // IEEE Std 802.11-2024, Table 9-517 and Figure 9-163.
    stream.writeByte(7);
    stream.writeByte(action);
    uint64_t mimoControl = (feedback->getNc() - 1) |
            (uint64_t(feedback->getNr() - 1) << 2) |
            (uint64_t(feedback->getChannelWidth() == 40e6) << 4) |
            (uint64_t(encodeHtFeedbackGrouping(feedback->getGrouping())) << 5) |
            (uint64_t(encodeHtCoefficientSize(action, feedback->getCoefficientSize())) << 7) |
            (uint64_t(feedback->getCodebookInformation()) << 9) |
            (uint64_t(feedback->getRemainingMatrixSegments()) << 11) |
            (uint64_t(feedback->getSoundingTimestamp()) << 16);
    for (int i = 0; i < 6; i++)
        stream.writeByte((mimoControl >> (8 * i)) & 0xff);
    for (size_t i = 0; i < feedback->getReportArraySize(); i++)
        stream.writeByte(feedback->getReport(i));
}

const Ptr<Chunk> Ieee80211HtActionFrameBodySerializer::deserialize(MemoryInputStream& stream) const
{
    if (stream.getRemainingLength() < B(9))
        throw cRuntimeError("Truncated HT CSI/beamforming action body");
    if (stream.readByte() != 7)
        throw cRuntimeError("Expected HT action category 7");
    auto action = stream.readByte();
    if (action < 4 || action > 6)
        throw cRuntimeError("Reserved or unsupported HT Action value %u", action);
    uint64_t mimoControl = 0;
    for (int i = 0; i < 6; i++)
        mimoControl |= uint64_t(stream.readByte()) << (8 * i);
    if (mimoControl & (uint64_t(3) << 14))
        throw cRuntimeError("Reserved bits set in HT MIMO Control field");
    Ptr<Ieee80211HtMimoFeedback> feedback;
    if (action == 4) feedback = makeShared<Ieee80211HtCsiFeedback>();
    else if (action == 5) feedback = makeShared<Ieee80211HtNoncompressedBeamformingFeedback>();
    else feedback = makeShared<Ieee80211HtCompressedBeamformingFeedback>();
    feedback->setNc((mimoControl & 3) + 1);
    feedback->setNr(((mimoControl >> 2) & 3) + 1);
    if (feedback->getNc() > feedback->getNr())
        throw cRuntimeError("HT MIMO Control Nc exceeds Nr");
    feedback->setChannelWidth((mimoControl & (uint64_t(1) << 4)) ? 40e6 : 20e6);
    feedback->setGrouping(decodeHtFeedbackGrouping((mimoControl >> 5) & 3));
    feedback->setCoefficientSize(decodeHtCoefficientSize(action, (mimoControl >> 7) & 3));
    feedback->setCodebookInformation((mimoControl >> 9) & 3);
    if ((action == 4 || action == 5) && feedback->getCodebookInformation() != 0)
        throw cRuntimeError("Reserved Codebook Information in HT CSI/noncompressed feedback");
    feedback->setRemainingMatrixSegments((mimoControl >> 11) & 7);
    feedback->setSoundingTimestamp(mimoControl >> 16);
    auto reportLength = stream.getRemainingLength().get<B>();
    if (reportLength == 0 || reportLength > 64)
        throw cRuntimeError("HT feedback report must contain 1..64 octets");
    feedback->setReportArraySize(reportLength);
    for (size_t i = 0; i < reportLength; i++)
        feedback->setReport(i, stream.readByte());
    feedback->setChunkLength(B(8 + reportLength));
    return feedback;
}

} // namespace ieee80211

} // namespace inet
