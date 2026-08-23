//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"

namespace inet {

namespace ieee80211 {

namespace {

void serializeCapabilityElements(MemoryOutputStream& stream, const Ieee80211MgmtFrame& frame)
{
    if (frame.getHtCapabilitiesPresent()) {
        stream.writeByte(IEEE80211_HT_CAPABILITIES_ELEMENT_ID);
        stream.writeByte(IEEE80211_HT_CAPABILITIES_ELEMENT_LENGTH);
        for (size_t i = 0; i < IEEE80211_HT_CAPABILITIES_ELEMENT_LENGTH; i++)
            stream.writeByte(frame.getHtCapabilities(i));
    }
    if (frame.getExtendedCapabilitiesPresent()) {
        auto length = frame.getExtendedCapabilitiesArraySize();
        if (length > 255)
            throw cRuntimeError("IEEE 802.11 Extended Capabilities element is too long");
        stream.writeByte(IEEE80211_EXTENDED_CAPABILITIES_ELEMENT_ID);
        stream.writeByte(length);
        for (size_t i = 0; i < length; i++)
            stream.writeByte(frame.getExtendedCapabilities(i));
    }
    if (frame.getVhtCapabilitiesPresent()) {
        stream.writeByte(IEEE80211_VHT_CAPABILITIES_ELEMENT_ID);
        stream.writeByte(IEEE80211_VHT_CAPABILITIES_ELEMENT_LENGTH);
        for (size_t i = 0; i < IEEE80211_VHT_CAPABILITIES_ELEMENT_LENGTH; i++)
            stream.writeByte(frame.getVhtCapabilities(i));
    }
    if (frame.getOperatingModePresent()) {
        stream.writeByte(IEEE80211_OPERATING_MODE_ELEMENT_ID);
        stream.writeByte(IEEE80211_OPERATING_MODE_ELEMENT_LENGTH);
        stream.writeByte(frame.getOperatingMode());
    }
}

void markMalformedElement(Ieee80211MgmtFrame& frame, MemoryInputStream& stream, b length)
{
    frame.markIncorrect();
    // Consume the complete advertised payload when it is available.  This
    // keeps the resulting chunk length deterministic even for malformed input.
    if (stream.getRemainingLength() >= length)
        stream.seek(stream.getPosition() + length);
    else
        stream.seek(stream.getLength());
}

void deserializeCapabilityElements(MemoryInputStream& stream, Ieee80211MgmtFrame& frame)
{
    while (stream.getRemainingLength() > b(0)) {
        if (stream.getRemainingLength() < B(2)) {
            frame.markIncorrect();
            stream.seek(stream.getLength());
            return;
        }

        uint8_t elementId = stream.readByte();
        uint8_t elementLength = stream.readByte();
        if (stream.getRemainingLength() < B(elementLength)) {
            markMalformedElement(frame, stream, B(elementLength));
            return;
        }

        switch (elementId) {
            case IEEE80211_HT_CAPABILITIES_ELEMENT_ID:
                if (elementLength != IEEE80211_HT_CAPABILITIES_ELEMENT_LENGTH || frame.getHtCapabilitiesPresent()) {
                    markMalformedElement(frame, stream, B(elementLength));
                    return;
                }
                for (size_t i = 0; i < IEEE80211_HT_CAPABILITIES_ELEMENT_LENGTH; i++)
                    frame.setHtCapabilities(i, stream.readByte());
                frame.setHtCapabilitiesPresent(true);
                break;

            case IEEE80211_EXTENDED_CAPABILITIES_ELEMENT_ID:
                if (frame.getExtendedCapabilitiesPresent()) {
                    markMalformedElement(frame, stream, B(elementLength));
                    return;
                }
                frame.setExtendedCapabilitiesArraySize(elementLength);
                for (size_t i = 0; i < elementLength; i++)
                    frame.setExtendedCapabilities(i, stream.readByte());
                frame.setExtendedCapabilitiesPresent(true);
                break;

            case IEEE80211_VHT_CAPABILITIES_ELEMENT_ID:
                if (elementLength != IEEE80211_VHT_CAPABILITIES_ELEMENT_LENGTH || frame.getVhtCapabilitiesPresent()) {
                    markMalformedElement(frame, stream, B(elementLength));
                    return;
                }
                for (size_t i = 0; i < IEEE80211_VHT_CAPABILITIES_ELEMENT_LENGTH; i++)
                    frame.setVhtCapabilities(i, stream.readByte());
                frame.setVhtCapabilitiesPresent(true);
                break;

            case IEEE80211_OPERATING_MODE_ELEMENT_ID:
                if (elementLength != IEEE80211_OPERATING_MODE_ELEMENT_LENGTH || frame.getOperatingModePresent()) {
                    markMalformedElement(frame, stream, B(elementLength));
                    return;
                }
                frame.setOperatingMode(stream.readByte());
                frame.setOperatingModePresent(true);
                break;

            default:
                // Unknown information elements are legal here.  Their strict
                // two-byte TLV framing and advertised length are still checked
                // above, while known fixed-length elements are preserved.
                stream.seek(stream.getPosition() + B(elementLength));
                break;
        }
    }
}

} // namespace

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
        // 3    Request information         May be included if dot11MultiDomainCapabilityEnabled is true.
        // 4    Extended Supported Rates    The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // Last Vendor Specific             One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto associationRequestFrame = dynamicPtrCast<const Ieee80211AssociationRequestFrame>(chunk);
             associationRequestFrame != nullptr && dynamicPtrCast<const Ieee80211ReassociationRequestFrame>(chunk) == nullptr) {
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
        // 6    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 7    Power Capability           The Power Capability element shall be present if dot11SpectrumManagementRequired is true.
        // 8    Supported Channels         The Supported Channels element shall be present if dot11SpectrumManagementRequired is true.
        // 9    RSN                        The RSN information element is only present within Reassociation Request frames generated by STAs that have dot11RSNAEnabled set to TRUE.
        // 10   QoS Capability             The QoS Capability element is present when dot11QosOption- Implemented is true.
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto associationResponseFrame = dynamicPtrCast<const Ieee80211AssociationResponseFrame>(chunk);
             associationResponseFrame != nullptr && dynamicPtrCast<const Ieee80211ReassociationResponseFrame>(chunk) == nullptr) {
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
        // 5    Extended Supported Rates   The Extended Supported Rates element is present whenever there are more than eight supported rates, and it is optional otherwise.
        // 6    EDCA Parameter Set
        // Last Vendor Specific            One or more vendor-specific information elements may appear in this frame. This information element follows all other information elements.
    }
    else if (auto beaconFrame = dynamicPtrCast<const Ieee80211BeaconFrame>(chunk);
             beaconFrame != nullptr && dynamicPtrCast<const Ieee80211ProbeResponseFrame>(chunk) == nullptr) {
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

    auto mgmtFrame = staticPtrCast<const Ieee80211MgmtFrame>(chunk);
    serializeCapabilityElements(stream, *mgmtFrame);
}

const Ptr<Chunk> Ieee80211MgmtFrameSerializer::deserialize(MemoryInputStream& stream) const
{
    throw cRuntimeError("Ieee80211MgmtFrameSerializer requires a chunk type for deserialization");
}

const Ptr<Chunk> Ieee80211MgmtFrameSerializer::deserialize(MemoryInputStream& stream, const std::type_info& typeInfo) const
{
    const b startPosition = stream.getPosition();
    Ptr<Chunk> chunk;

    auto require = [&](Ieee80211MgmtFrame& frame, b length) {
        if (stream.getRemainingLength() < length) {
            frame.markIncorrect();
            stream.seek(stream.getLength());
            return false;
        }
        return true;
    };
    auto readSsid = [&](Ieee80211MgmtFrame& frame, auto setter) {
        if (!require(frame, B(2)))
            return;
        stream.readByte(); // element ID (the legacy model uses zero here)
        uint8_t length = stream.readByte();
        if (!require(frame, B(length)))
            return;
        std::vector<uint8_t> bytes;
        stream.readBytes(bytes, B(length));
        std::string ssid(bytes.begin(), bytes.end());
        setter(ssid.c_str());
    };
    auto readSupportedRates = [&](Ieee80211MgmtFrame& frame, Ieee80211SupportedRatesElement& rates) {
        if (!require(frame, B(2)))
            return;
        stream.readByte(); // Supported Rates element ID
        uint8_t count = stream.readByte();
        if (count > 8 || !require(frame, B(count))) {
            frame.markIncorrect();
            stream.seek(stream.getLength());
            return;
        }
        rates.numRates = count;
        for (uint8_t i = 0; i < count; i++)
            rates.rate[i] = (double)(stream.readByte() & 0x7F) * 0.5;
    };

    if (typeInfo == typeid(Ieee80211AuthenticationFrame)) {
        auto frame = makeShared<Ieee80211AuthenticationFrame>();
        if (require(*frame, B(6))) {
            stream.readUint16Be();
            frame->setSequenceNumber(stream.readUint16Be());
            frame->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
        }
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211DeauthenticationFrame)) {
        auto frame = makeShared<Ieee80211DeauthenticationFrame>();
        if (require(*frame, B(2)))
            frame->setReasonCode((Ieee80211ReasonCode)stream.readUint16Be());
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211DisassociationFrame)) {
        auto frame = makeShared<Ieee80211DisassociationFrame>();
        if (require(*frame, B(2)))
            frame->setReasonCode((Ieee80211ReasonCode)stream.readUint16Be());
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211ProbeRequestFrame)) {
        auto frame = makeShared<Ieee80211ProbeRequestFrame>();
        readSsid(*frame, [&](const char *ssid) { frame->setSSID(ssid); });
        if (!frame->isIncorrect())
            readSupportedRates(*frame, frame->getSupportedRatesForUpdate());
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211AssociationRequestFrame)) {
        auto frame = makeShared<Ieee80211AssociationRequestFrame>();
        if (require(*frame, B(4))) {
            stream.readUint16Be(); // Capability
            stream.readUint16Be(); // Listen interval
            readSsid(*frame, [&](const char *ssid) { frame->setSSID(ssid); });
            if (!frame->isIncorrect())
                readSupportedRates(*frame, frame->getSupportedRatesForUpdate());
        }
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211ReassociationRequestFrame)) {
        auto frame = makeShared<Ieee80211ReassociationRequestFrame>();
        if (require(*frame, B(12))) {
            stream.readUint16Be(); // Capability
            stream.readUint16Be(); // Listen interval
            frame->setCurrentAP(stream.readMacAddress());
            readSsid(*frame, [&](const char *ssid) { frame->setSSID(ssid); });
            if (!frame->isIncorrect())
                readSupportedRates(*frame, frame->getSupportedRatesForUpdate());
        }
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211AssociationResponseFrame)) {
        auto frame = makeShared<Ieee80211AssociationResponseFrame>();
        if (require(*frame, B(6))) {
            stream.readUint16Be(); // Capability
            frame->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            frame->setAid(stream.readUint16Be());
            readSupportedRates(*frame, frame->getSupportedRatesForUpdate());
        }
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211ReassociationResponseFrame)) {
        auto frame = makeShared<Ieee80211ReassociationResponseFrame>();
        if (require(*frame, B(6))) {
            stream.readUint16Be(); // Capability
            frame->setStatusCode((Ieee80211StatusCode)stream.readUint16Be());
            frame->setAid(stream.readUint16Be());
            readSupportedRates(*frame, frame->getSupportedRatesForUpdate());
        }
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211BeaconFrame)) {
        auto frame = makeShared<Ieee80211BeaconFrame>();
        if (require(*frame, B(12))) {
            stream.readUint64Be(); // Timestamp
            frame->setBeaconInterval(SimTime((int64_t)stream.readUint16Be() * 1024, SIMTIME_US));
            stream.readUint16Be(); // Capability
            readSsid(*frame, [&](const char *ssid) { frame->setSSID(ssid); });
            if (!frame->isIncorrect())
                readSupportedRates(*frame, frame->getSupportedRatesForUpdate());
        }
        chunk = frame;
    }
    else if (typeInfo == typeid(Ieee80211ProbeResponseFrame)) {
        auto frame = makeShared<Ieee80211ProbeResponseFrame>();
        if (require(*frame, B(12))) {
            stream.readUint64Be(); // Timestamp
            frame->setBeaconInterval(SimTime((int64_t)stream.readUint16Be() * 1024, SIMTIME_US));
            stream.readUint16Be(); // Capability
            readSsid(*frame, [&](const char *ssid) { frame->setSSID(ssid); });
            if (!frame->isIncorrect())
                readSupportedRates(*frame, frame->getSupportedRatesForUpdate());
        }
        chunk = frame;
    }
    else
        throw cRuntimeError("Cannot deserialize frame type %s", typeInfo.name());

    auto frame = staticPtrCast<Ieee80211MgmtFrame>(chunk);
    if (!frame->isIncorrect())
        deserializeCapabilityElements(stream, *frame);
    frame->setChunkLength(stream.getPosition() - startPosition);

    return frame;
}

} // namespace ieee80211

} // namespace inet
