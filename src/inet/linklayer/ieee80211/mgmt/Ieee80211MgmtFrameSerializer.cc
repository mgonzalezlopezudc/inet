//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.h"

#include <algorithm>
#include <vector>

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211HeMgmtElements.h"
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

static const uint8_t ELEMENT_ID_EXTENSION = 255;
static const uint8_t ELEMENT_ID_EXTENSION_HE_CAPABILITIES = 35;
static const uint8_t ELEMENT_ID_EXTENSION_HE_OPERATION = 36;
static const uint8_t ELEMENT_ID_EXTENSION_HE_6GHZ_BAND_CAPABILITIES = 59;

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

static void writeHeCapabilitiesElement(MemoryOutputStream& stream, const Ieee80211HeCapabilitiesElement& capabilities)
{
    int mcsNssLength = capabilities.supportedChannelWidth160MHz || capabilities.supportedChannelWidth80Plus80MHz ? 8 : 4;
    stream.writeByte(ELEMENT_ID_EXTENSION);
    stream.writeByte(1 + 6 + 11 + mcsNssLength);
    stream.writeByte(ELEMENT_ID_EXTENSION_HE_CAPABILITIES);

    uint64_t macCapabilities = 0;
    setBits(macCapabilities, 12, 3, capabilities.multiTidAggregationRx ? 1 : 0);
    setBits(macCapabilities, 17, 1, capabilities.heTbBlockAckTx ? 1 : 0);
    setBits(macCapabilities, 19, 1, capabilities.ulOfdma ? 1 : 0);
    setBits(macCapabilities, 26, 1, capabilities.ulOfdma ? 1 : 0);
    setBits(macCapabilities, 27, 2, std::clamp((int)capabilities.maxAmpduLengthExponent - 3, 0, 3));
    setBits(macCapabilities, 35, 1, capabilities.multiTidAggregationTx ? 1 : 0);
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
    int dcmConstellation = capabilities.dcm ? encodeDcmConstellation(capabilities.maxDcmConstellation) : 0;
    int dcmNss = capabilities.dcm && capabilities.maxDcmNss > 1 ? 1 : 0;
    setBits(phyCapabilities, 24, 2, dcmConstellation);
    setBits(phyCapabilities, 26, 1, dcmNss);
    setBits(phyCapabilities, 27, 2, dcmConstellation);
    setBits(phyCapabilities, 29, 1, dcmNss);
    setBits(phyCapabilities, 70, 2, encodeDcmMaxRu(capabilities));
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

static void writeHeMgmtElements(MemoryOutputStream& stream, const Ptr<const Ieee80211MgmtFrame>& frame)
{
    if (frame->getHeCapabilitiesPresent())
        writeHeCapabilitiesElement(stream, frame->getHeCapabilities());
    if (frame->getHeOperationPresent())
        writeHeOperationElement(stream, frame->getHeOperation());
    if (frame->getHe6GhzBandCapabilitiesPresent())
        writeHe6GhzBandCapabilitiesElement(stream, frame->getHe6GhzBandCapabilities());
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
    capabilities.supportedChannelWidth80MHz = (supportedChannelWidthSet & (1 << 1)) != 0;
    capabilities.supportedChannelWidth160MHz = (supportedChannelWidthSet & (1 << 2)) != 0;
    capabilities.supportedChannelWidth80Plus80MHz = (supportedChannelWidthSet & (1 << 3)) != 0;
    capabilities.dlOfdma = true;
    capabilities.ulOfdma = getBits(macCapabilities, 26, 1) != 0 || getBits(macCapabilities, 19, 1) != 0;
    capabilities.ldpc = getBit(phyCapabilities, 13);
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

static void readHeMgmtElements(MemoryInputStream& stream, const Ptr<Ieee80211MgmtFrame>& frame)
{
    while (stream.getRemainingLength() >= B(2)) {
        int elementId = stream.readByte();
        int length = stream.readByte();
        if (stream.getRemainingLength() < B(length))
            throw cRuntimeError("Malformed IEEE 802.11 management element: id=%d length=%d remaining=%" PRId64,
                    elementId, length, stream.getRemainingLength().get<B>());
        if (elementId == ELEMENT_ID_EXTENSION && length >= 1) {
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
                default:
                    skipBytes(stream, payloadLength);
                    break;
            }
        }
        else
            skipBytes(stream, length);
    }
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
        const Ieee80211SupportedRatesElement& supportedRates = probeRequestFrame->getSupportedRates();
        stream.writeByte(1);
        stream.writeByte(supportedRates.numRates);
        for (int i = 0; i < supportedRates.numRates; i++) {
            uint8_t rate = ceil(supportedRates.rate[i] / 0.5);
            // rate |= 0x80 if rate contained in the BSSBasicRateSet parameter
            stream.writeByte(rate);
        }
        writeHeMgmtElements(stream, probeRequestFrame);
        // 3    Request information         May be included if dot11MultiDomainCapabilityEnabled is true.
        // 4    Extended Supported Rates    The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // Last Vendor Specific             One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto associationRequestFrame = dynamicPtrCast<const Ieee80211AssociationRequestFrame>(chunk)) {
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
        const Ieee80211SupportedRatesElement& supportedRates = associationRequestFrame->getSupportedRates();
        stream.writeByte(1);
        stream.writeByte(supportedRates.numRates);
        for (int i = 0; i < supportedRates.numRates; i++) {
            uint8_t rate = ceil(supportedRates.rate[i] / 0.5);
            // rate |= 0x80 if rate contained in the BSSBasicRateSet parameter
            stream.writeByte(rate);
        }
        stream.writeUint16Be((uint16_t)std::lround((associationRequestFrame->getTransmitPowerDbm() + 128) * 100));
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
        const Ieee80211SupportedRatesElement& supportedRates = reassociationRequestFrame->getSupportedRates();
        stream.writeByte(1);
        stream.writeByte(supportedRates.numRates);
        for (int i = 0; i < supportedRates.numRates; i++) {
            uint8_t rate = ceil(supportedRates.rate[i] / 0.5);
            // rate |= 0x80 if rate contained in the BSSBasicRateSet parameter
            stream.writeByte(rate);
        }
        writeHeMgmtElements(stream, reassociationRequestFrame);
        // 6    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 7    Power Capability           The Power Capability element shall be present if dot11SpectrumManagementRequired is true.
        // 8    Supported Channels         The Supported Channels element shall be present if dot11SpectrumManagementRequired is true.
        // 9    RSN                        The RSN information element is only present within Reassociation Request frames generated by STAs that have dot11RSNAEnabled set to TRUE.
        // 10   QoS Capability             The QoS Capability element is present when dot11QosOption- Implemented is true.
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto associationResponseFrame = dynamicPtrCast<const Ieee80211AssociationResponseFrame>(chunk)) {
//        type = ST_ASSOCIATIONRESPONSE;
        // 1    Capability
        stream.writeUint16Be(0); // FIXME
        // 2    Status code
        stream.writeUint16Be(associationResponseFrame->getStatusCode());
        // 3    AID
        stream.writeUint16Be(associationResponseFrame->getAid());
        // 4    Supported rates
        stream.writeByte(1);
        stream.writeByte(associationResponseFrame->getSupportedRates().numRates);
        for (int i = 0; i < associationResponseFrame->getSupportedRates().numRates; i++) {
            uint8_t rate = ceil(associationResponseFrame->getSupportedRates().rate[i] / 0.5);
            // rate |= 0x80 if rate contained in the BSSBasicRateSet parameter
            stream.writeByte(rate);
        }
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
        stream.writeByte(1);
        stream.writeByte(reassociationResponseFrame->getSupportedRates().numRates);
        for (int i = 0; i < reassociationResponseFrame->getSupportedRates().numRates; i++) {
            uint8_t rate = ceil(reassociationResponseFrame->getSupportedRates().rate[i] / 0.5);
            // rate |= 0x80 if rate contained in the BSSBasicRateSet parameter
            stream.writeByte(rate);
        }
        writeHeMgmtElements(stream, reassociationResponseFrame);
        // 5    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 6    EDCA Parameter Set
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto beaconFrame = dynamicPtrCast<const Ieee80211BeaconFrame>(chunk)) {
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
        stream.writeByte(1);
        stream.writeByte(beaconFrame->getSupportedRates().numRates);
        for (int i = 0; i < beaconFrame->getSupportedRates().numRates; i++) {
            uint8_t rate = ceil(beaconFrame->getSupportedRates().rate[i] / 0.5);
            // rate |= 0x80 if rate contained in the BSSBasicRateSet parameter
            stream.writeByte(rate);
        }
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
        stream.writeByte(1);
        stream.writeByte(probeResponseFrame->getSupportedRates().numRates);
        for (int i = 0; i < probeResponseFrame->getSupportedRates().numRates; i++) {
            uint8_t rate = ceil(probeResponseFrame->getSupportedRates().rate[i] / 0.5);
            // rate |= 0x80 if rate contained in the BSSBasicRateSet parameter
            stream.writeByte(rate);
        }
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

const Ptr<Chunk> Ieee80211MgmtFrameSerializer::deserialize(MemoryInputStream& stream) const
{
    switch (0) { // TODO receive and dispatch on type_info parameter
        case 0xB0: // ST_AUTHENTICATION
        {
            auto frame = makeShared<Ieee80211AuthenticationFrame>();
            stream.readUint16Be();
            frame->setSequenceNumber(stream.readUint16Be());
            frame->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            return frame;
        }

        case 0xC0: // ST_ST_DEAUTHENTICATION
        {
            auto frame = makeShared<Ieee80211DeauthenticationFrame>();
            frame->setReasonCode((Ieee80211ReasonCode)stream.readUint16Be());
            return frame;
        }

        case 0xA0: // ST_DISASSOCIATION
        {
            auto frame = makeShared<Ieee80211DisassociationFrame>();
            frame->setReasonCode((Ieee80211ReasonCode)stream.readUint16Be());
            return frame;
        }

        case 0x40: // ST_PROBEREQUEST
        {
            auto frame = makeShared<Ieee80211ProbeRequestFrame>();

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            frame->setSSID(SSID);

            Ieee80211SupportedRatesElement supRat;
            stream.readByte();
            supRat.numRates = stream.readByte();
            for (int i = 0; i < supRat.numRates; i++)
                supRat.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
            frame->setSupportedRates(supRat);
            readHeMgmtElements(stream, frame);
            return frame;
        }

        case 0x00: // ST_ASSOCIATIONREQUEST
        {
            auto frame = makeShared<Ieee80211AssociationRequestFrame>();
            stream.readUint16Be();
            stream.readUint16Be();

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            frame->setSSID(SSID);

            Ieee80211SupportedRatesElement supRat;
            stream.readByte();
            supRat.numRates = stream.readByte();
            for (int i = 0; i < supRat.numRates; i++)
                supRat.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
            frame->setSupportedRates(supRat);
            frame->setTransmitPowerDbm(stream.readUint16Be() / 100.0 - 128);
            readHeMgmtElements(stream, frame);
            return frame;
        }

        case 0x02: // ST_REASSOCIATIONREQUEST
        {
            auto frame = makeShared<Ieee80211ReassociationRequestFrame>();
            stream.readUint16Be();
            stream.readUint16Be();

            frame->setCurrentAP(stream.readMacAddress());

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            frame->setSSID(SSID);

            Ieee80211SupportedRatesElement supRat;
            stream.readByte();
            supRat.numRates = stream.readByte();
            for (int i = 0; i < supRat.numRates; i++)
                supRat.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
            frame->setSupportedRates(supRat);
            readHeMgmtElements(stream, frame);
            return frame;
        }

        case 0x01: // ST_ASSOCIATIONRESPONSE
        {
            auto frame = makeShared<Ieee80211AssociationResponseFrame>();
            stream.readUint16Be();
            frame->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            frame->setAid(stream.readUint16Be());

            Ieee80211SupportedRatesElement supRat;
            stream.readByte();
            supRat.numRates = stream.readByte();
            for (int i = 0; i < supRat.numRates; i++)
                supRat.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
            frame->setSupportedRates(supRat);
            readHeMgmtElements(stream, frame);
            return frame;
        }

        case 0x03: // ST_REASSOCIATIONRESPONSE
        {
            auto frame = makeShared<Ieee80211ReassociationResponseFrame>();
            stream.readUint16Be();
            frame->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            frame->setAid(stream.readUint16Be());

            Ieee80211SupportedRatesElement supRat;
            stream.readByte();
            supRat.numRates = stream.readByte();
            for (int i = 0; i < supRat.numRates; i++)
                supRat.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
            frame->setSupportedRates(supRat);
            readHeMgmtElements(stream, frame);
            return frame;
        }

        case 0x80: // ST_BEACON
        {
            auto frame = makeShared<Ieee80211BeaconFrame>();

            simtime_t timetstamp;
            timetstamp.setRaw(stream.readUint64Be()); // TODO store timestamp

            frame->setBeaconInterval(SimTime((int64_t)stream.readUint16Be() * 1024, SIMTIME_US));
            stream.readUint16Be(); // Capability

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            frame->setSSID(SSID);

            Ieee80211SupportedRatesElement supRat;
            stream.readByte();
            supRat.numRates = stream.readByte();
            for (int i = 0; i < supRat.numRates; i++)
                supRat.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
            frame->setSupportedRates(supRat);
            readHeMgmtElements(stream, frame);
            return frame;
        }

        case 0x50: // ST_PROBERESPONSE
        {
            auto frame = makeShared<Ieee80211ProbeResponseFrame>();

            simtime_t timestamp;
            timestamp.setRaw(stream.readUint64Be()); // TODO store timestamp

            frame->setBeaconInterval(SimTime((int64_t)stream.readUint16Be() * 1024, SIMTIME_US));
            stream.readUint16Be();

            char SSID[256];
            stream.readByte();
            unsigned int length = stream.readByte();
            stream.readBytes((uint8_t *)SSID, B(length));
            SSID[length] = '\0';
            frame->setSSID(SSID);

            Ieee80211SupportedRatesElement supRat;
            stream.readByte();
            supRat.numRates = stream.readByte();
            for (int i = 0; i < supRat.numRates; i++)
                supRat.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
            frame->setSupportedRates(supRat);
            readHeMgmtElements(stream, frame);
            return frame;
        }

        default:
            throw cRuntimeError("Cannot deserialize frame");
    }
}

} // namespace ieee80211

} // namespace inet
