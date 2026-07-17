//
// Copyright (C) 2005 Michael Tuexen
// Copyright (C) 2008 Irene Ruengeler
// Copyright (C) 2009 Thomas Dreibholz
// Copyright (C) 2009 Thomas Reschka
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/common/packet/recorder/PcapRecorder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "inet/common/DirectionTag_m.h"
#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/packet/recorder/PcapngWriter.h"
#include "inet/common/packet/recorder/PcapWriter.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/stlutils.h"
#include "inet/common/StringFormat.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/InterfaceTable.h"

#ifdef INET_WITH_IEEE80211
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#endif

#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
#include "inet/physicallayer/common/Signal.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReception.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmission.h"
#endif

namespace inet {

namespace {

enum RadiotapPresentBit {
    RADIOTAP_FLAGS = 1,
    RADIOTAP_CHANNEL = 3,
    RADIOTAP_ANTENNA_SIGNAL = 5,
    RADIOTAP_DBM_TX_POWER = 10,
    RADIOTAP_RX_FLAGS = 14,
    RADIOTAP_TX_FLAGS = 15,
    RADIOTAP_HE = 23,
    RADIOTAP_HE_MU = 24,
};

enum RadiotapFlags {
    RADIOTAP_F_FCS = 0x10,
    RADIOTAP_F_BADFCS = 0x40,
};

enum RadiotapChannelFlags {
    RADIOTAP_CHANNEL_2GHZ = 0x0080,
    RADIOTAP_CHANNEL_5GHZ = 0x0100,
};

void appendPadding(std::vector<uint8_t>& bytes, size_t alignment)
{
    bytes.resize(bytes.size() + (alignment - bytes.size() % alignment) % alignment, 0);
}

void appendUint16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(value & 0xff);
    bytes.push_back(value >> 8);
}

void setUint16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value)
{
    bytes.at(offset) = value & 0xff;
    bytes.at(offset + 1) = value >> 8;
}

void setUint32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
    for (size_t i = 0; i < 4; i++)
        bytes.at(offset + i) = value >> (8 * i);
}

#ifdef INET_WITH_IEEE80211

struct MpduRange
{
    b offset;
    b length;
};

bool getIeee80211AmpduMpduRanges(const Packet *packet, b frontOffset, b backOffset, std::vector<MpduRange>& mpduRanges)
{
    const int parsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    auto dataLength = packet->getDataLength();
    auto endOffset = dataLength - backOffset;
    if (frontOffset + ieee80211::LENGTH_A_MPDU_SUBFRAME_HEADER > endOffset)
        return false;

    auto peekDelimiter = [&] (b offset) {
        return dynamicPtrCast<const ieee80211::Ieee80211MpduSubframeHeader>(packet->peekDataAt(offset, b(-1), parsingFlags));
    };

    try {
        if (peekDelimiter(frontOffset) == nullptr)
            return false;
        auto offset = frontOffset;
        while (offset < endOffset) {
            if (offset + ieee80211::LENGTH_A_MPDU_SUBFRAME_HEADER > endOffset)
                return false;
            const auto& delimiter = peekDelimiter(offset);
            if (delimiter == nullptr || delimiter->getLength() <= 0)
                return false;
            auto mpduOffset = offset + delimiter->getChunkLength();
            auto mpduLength = B(delimiter->getLength());
            if (mpduOffset + mpduLength > endOffset)
                return false;
            mpduRanges.push_back({mpduOffset, mpduLength});
            offset = mpduOffset + mpduLength;
            if (offset == endOffset)
                return true;
            auto paddingLength = B((4 - (delimiter->getChunkLength() + mpduLength).get<B>() % 4) % 4);
            if (offset + paddingLength >= endOffset)
                return false;
            offset += paddingLength;
        }
    }
    catch (cRuntimeError&) {
        return false;
    }
    return false;
}

#endif

std::vector<uint8_t> makeRadiotapHeader(const Packet *packet, Direction direction, const physicallayer::ITransmission *transmission, const physicallayer::IReception *reception)
{
    uint32_t present = 1U << RADIOTAP_FLAGS;
    std::vector<uint8_t> bytes(8, 0); // version, pad, length, present bitmap

    bool isHe = false;
    bool isHeMu = false;
    Ptr<const physicallayer::Ieee80211HeMuReq> heMuReq;
    Ptr<const physicallayer::Ieee80211HeMuRxTag> heMuRx;
    const physicallayer::Ieee80211HeMode *heMode = nullptr;

#ifdef INET_WITH_IEEE80211
    heMuReq = packet->findTag<physicallayer::Ieee80211HeMuReq>();
    heMuRx = packet->findTag<physicallayer::Ieee80211HeMuRxTag>();
    if (heMuReq != nullptr || heMuRx != nullptr) {
        isHe = true;
        isHeMu = true;
    }
    else {
        const physicallayer::IIeee80211Mode *mode = nullptr;
        auto modeReq = packet->findTag<physicallayer::Ieee80211ModeReq>();
        if (modeReq != nullptr)
            mode = modeReq->getMode();
        else {
            auto modeInd = packet->findTag<physicallayer::Ieee80211ModeInd>();
            if (modeInd != nullptr)
                mode = modeInd->getMode();
        }
        if (mode != nullptr) {
            heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(mode);
            if (heMode != nullptr) {
                isHe = true;
            }
        }
    }
#endif

    if (isHe) {
        present |= 1U << RADIOTAP_HE;
        if (isHeMu) {
            present |= 1U << RADIOTAP_HE_MU;
        }
    }

    // IEEE 802.11 packets recorded by PcapRecorder contain the MAC trailer.
    uint8_t flags = RADIOTAP_F_FCS;
    if (packet->hasBitError())
        flags |= RADIOTAP_F_BADFCS;
    bytes.push_back(flags);

#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
    const physicallayer::ISignalAnalogModel *analogModel = nullptr;
    simtime_t startTime;
    simtime_t endTime;
    if (reception != nullptr) {
        analogModel = reception->getAnalogModel();
        startTime = reception->getStartTime();
        endTime = reception->getEndTime();
    }
    else if (transmission != nullptr) {
        analogModel = transmission->getAnalogModel();
        startTime = transmission->getStartTime();
        endTime = transmission->getEndTime();
    }

    auto narrowbandAnalogModel = dynamic_cast<const physicallayer::INarrowbandSignalAnalogModel *>(analogModel);
    if (narrowbandAnalogModel != nullptr) {
        double frequencyMHz = narrowbandAnalogModel->getCenterFrequency().get() / 1E6;
        if (std::isfinite(frequencyMHz) && 0 < frequencyMHz && frequencyMHz <= UINT16_MAX) {
            present |= 1U << RADIOTAP_CHANNEL;
            appendPadding(bytes, 2);
            appendUint16(bytes, static_cast<uint16_t>(std::round(frequencyMHz)));
            uint16_t channelFlags = frequencyMHz < 3000 ? RADIOTAP_CHANNEL_2GHZ :
                    frequencyMHz < 6000 ? RADIOTAP_CHANNEL_5GHZ : 0;
            appendUint16(bytes, channelFlags);
        }

        auto power = narrowbandAnalogModel->computeMinPower(startTime, endTime);
        double powerMilliwatts = power.get<units::values::mW>();
        if (std::isfinite(powerMilliwatts) && 0 < powerMilliwatts &&
                (direction == DIRECTION_INBOUND || direction == DIRECTION_OUTBOUND)) {
            int powerDbm = static_cast<int>(std::round(math::mW2dBmW(powerMilliwatts)));
            powerDbm = std::clamp(powerDbm, -128, 127);
            if (direction == DIRECTION_INBOUND)
                present |= 1U << RADIOTAP_ANTENNA_SIGNAL;
            else if (direction == DIRECTION_OUTBOUND)
                present |= 1U << RADIOTAP_DBM_TX_POWER;
            bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(powerDbm)));
        }
    }
#endif

    if (direction == DIRECTION_INBOUND) {
        present |= 1U << RADIOTAP_RX_FLAGS;
        appendPadding(bytes, 2);
        appendUint16(bytes, 0);
    }
    else if (direction == DIRECTION_OUTBOUND) {
        present |= 1U << RADIOTAP_TX_FLAGS;
        appendPadding(bytes, 2);
        appendUint16(bytes, 0);
    }

#ifdef INET_WITH_IEEE80211
    if (isHe) {
        appendPadding(bytes, 2);

        uint16_t data1 = 0;
        uint16_t data2 = 0;
        uint16_t data3 = 0;
        uint16_t data4 = 0;
        uint16_t data5 = 0;
        uint16_t data6 = 0;

        if (heMuReq != nullptr) {
            // HE TB format is 3
            data1 |= 3 & 0x3;
            // Known flags: PPDU Format, UL/DL, Coding, MCS, GI, PE
            data1 |= (1U << 0) | (1U << 4) | (1U << 5) | (1U << 7);
            
            // Uplink
            data2 |= (1U << 1);
            
            // Guard Interval: 0=0.8us, 1=1.6us, 2=3.2us
            data3 |= (heMuReq->getGuardInterval() & 0x7);
            
            // Set coding, MCS, NSS, DCM in data5
            data5 |= (heMuReq->getMcs() & 0xF);
            data5 |= ((heMuReq->getNumberOfSpatialStreams() - 1) & 0x7) << 4;
            if (heMuReq->getDcm()) data5 |= (1U << 7);
            data5 |= (heMuReq->getCoding() & 0x1) << 8; // Coding: 0=BCC, 1=LDPC
            
            // Punctured subchannel mask
            data4 |= (heMuReq->getPuncturedSubchannelMask() & 0xF) << 3;
        }
        else if (heMuRx != nullptr) {
            // Format from tag
            uint8_t format = heMuRx->getPpduFormat(); // 0: DL MU, 1: UL TB
            data1 |= (format == 0 ? 2 : 3) & 0x3;
            data1 |= (1U << 0) | (1U << 4) | (1U << 7);
            
            data2 |= (format == 0 ? 0 : 1) << 1; // DL=0, UL=1
            data3 |= (heMuRx->getGuardInterval() & 0x7);
            
            data4 |= (heMuRx->getPuncturedSubchannelMask() & 0xF) << 3;
        }
        else if (heMode != nullptr) {
            // HE SU format = 0
            data1 |= 0 & 0x3;
            data1 |= (1U << 0) | (1U << 5) | (1U << 7);
            
            auto dm = heMode->getDataMode();
            if (dm != nullptr) {
                data5 |= (dm->getMcsIndex() & 0xF);
                data5 |= ((dm->getNumberOfSpatialStreams() - 1) & 0x7) << 4;
                
                auto gi = dm->getGuardIntervalType();
                data3 |= (gi == physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT ? 0 :
                          gi == physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM ? 1 : 2);
                
                double bw = dm->getBandwidth().get();
                uint8_t cbw = (bw < 30e6 ? 0 : bw < 60e6 ? 1 : bw < 100e6 ? 2 : 3);
                data4 |= cbw & 0x7;
            }
        }

        appendUint16(bytes, data1);
        appendUint16(bytes, data2);
        appendUint16(bytes, data3);
        appendUint16(bytes, data4);
        appendUint16(bytes, data5);
        appendUint16(bytes, data6);
    }

    if (isHeMu) {
        appendPadding(bytes, 2);
        
        uint16_t flags1 = 0;
        uint16_t flags2 = 0;
        uint8_t ruChannel1[4] = {0};
        uint8_t ruChannel2[4] = {0};

        if (heMuReq != nullptr) {
            flags1 |= (1U << 5); // DL MU-MIMO configuration known
            if (heMuReq->getRuIndex() >= 0 && heMuReq->getRuIndex() < 8) {
                ruChannel1[heMuReq->getRuIndex() / 2] = heMuReq->getRuIndex();
            }
        }
        else if (heMuRx != nullptr) {
            for (size_t i = 0; i < heMuRx->getAllocationsArraySize(); ++i) {
                const auto& alloc = heMuRx->getAllocations(i);
                if (alloc.ruIndex >= 0 && alloc.ruIndex < 8) {
                    ruChannel1[alloc.ruIndex / 2] = alloc.ruIndex;
                }
            }
        }

        appendUint16(bytes, flags1);
        appendUint16(bytes, flags2);
        for (int i = 0; i < 4; ++i) bytes.push_back(ruChannel1[i]);
        for (int i = 0; i < 4; ++i) bytes.push_back(ruChannel2[i]);
    }
#endif

    setUint16(bytes, 2, bytes.size());
    setUint32(bytes, 4, present);
    return bytes;
}

} // namespace

// ----

Define_Module(PcapRecorder);

simsignal_t PcapRecorder::packetRecordedSignal = registerSignal("packetRecorded");

PcapRecorder::~PcapRecorder()
{
    delete pcapWriter;
    for (auto helper : helpers)
        delete helper;
}

PcapRecorder::PcapRecorder() : SimpleModule()
{
}

bool PcapRecorder::shouldDissectProtocolDataUnit(const Protocol *protocol)
{
    return !contains(dumpProtocols, protocol);
}

void PcapRecorder::startProtocolDataUnit(const Protocol *protocol)
{
    if (contains(dumpProtocols, protocol))
        dumpProtocol = protocol;
}

void PcapRecorder::visitChunk(const Ptr<const Chunk>& chunk, const Protocol *protocol)
{
    if (!contains(dumpProtocols, protocol)) {
        if (dumpProtocol == nullptr)
            frontOffset += chunk->getChunkLength();
        else
            backOffset += chunk->getChunkLength();
    }
    else
        dumpProtocol = protocol;
}

void PcapRecorder::initialize()
{
    verbose = par("verbose");
    recordEmptyPackets = par("recordEmptyPackets");
    enableConvertingPackets = par("enableConvertingPackets");
    snaplen = this->par("snaplen");
    dumpBadFrames = par("dumpBadFrames");
    signalList.clear();
    packetFilter.setExpression(par("packetFilter").objectValue());

    {
        cStringTokenizer signalTokenizer(par("sendingSignalNames"));

        while (signalTokenizer.hasMoreTokens())
            signalList[registerSignal(signalTokenizer.nextToken())] = DIRECTION_OUTBOUND;
    }

    {
        cStringTokenizer signalTokenizer(par("receivingSignalNames"));

        while (signalTokenizer.hasMoreTokens())
            signalList[registerSignal(signalTokenizer.nextToken())] = DIRECTION_INBOUND;
    }

    {
        cStringTokenizer protocolTokenizer(par("dumpProtocols"));

        while (protocolTokenizer.hasMoreTokens())
            dumpProtocols.push_back(Protocol::getProtocol(protocolTokenizer.nextToken()));
    }

    {
        cStringTokenizer protocolTokenizer(par("helpers"));

        while (protocolTokenizer.hasMoreTokens())
            helpers.push_back(check_and_cast<IHelper *>(createOne(protocolTokenizer.nextToken())));
    }

    const char *moduleNames = par("moduleNamePatterns");
    cStringTokenizer moduleTokenizer(moduleNames);

    while (moduleTokenizer.hasMoreTokens()) {
        bool found = false;
        std::string mname(moduleTokenizer.nextToken());
        bool isAllIndex = (mname.length() > 3) && mname.rfind("[*]") == mname.length() - 3;

        if (isAllIndex)
            mname.replace(mname.length() - 3, 3, "");

        if (mname[0] == '.') {
            for (auto& elem : signalList)
                getParentModule()->subscribe(elem.first, this);
            found = true;
        }
        else {
            for (cModule::SubmoduleIterator i(getParentModule()); !i.end(); i++) {
                cModule *submod = *i;
                if (0 == strcmp(isAllIndex ? submod->getName() : submod->getFullName(), mname.c_str())) {
                    found = true;

                    for (auto& elem : signalList) {
                        if (!submod->isSubscribed(elem.first, this)) {
                            submod->subscribe(elem.first, this);
                            EV_INFO << "Subscribing to " << submod->getFullPath() << ":" << getSignalName(elem.first) << EV_ENDL;
                        }
                    }
                }
            }
        }

        if (!found && !isAllIndex)
            EV_INFO << "The module " << mname << (isAllIndex ? "[*]" : "") << " not found" << EV_ENDL;
    }

    std::string fileName = getEnvir()->getConfig()->substituteVariables(par("pcapFile"));
    const char *fileFormat = par("fileFormat");
    int timePrecision = par("timePrecision");
    if (!strcmp(fileFormat, "pcap"))
        pcapWriter = new PcapWriter();
    else if (!strcmp(fileFormat, "pcapng"))
        pcapWriter = new PcapngWriter();
    else
        throw cRuntimeError("Unknown fileFormat parameter: '%s'", fileFormat);

    recordPcap = !fileName.empty();
    if (recordPcap) {
        pcapWriter->open(fileName.c_str(), snaplen, timePrecision);
        pcapWriter->setFlush(par("alwaysFlush"));
    }

    WATCH(recordPcap);
    WATCH(frontOffset);
    WATCH(backOffset);
    WATCH(numRecorded);
}

void PcapRecorder::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not handle messages");
}

std::string PcapRecorder::resolveDirective(char directive) const
{
    switch (directive) {
        case 'n':
            return std::to_string(numRecorded);
        default:
            return SimpleModule::resolveDirective(directive);   
    }
}

void PcapRecorder::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (pcapWriter->isOpen()) {
        auto i = signalList.find(signalID);
        ASSERT(i != signalList.end());
        Direction direction = i->second;
        if (false)
            ;
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
        else if (auto signal = dynamic_cast<const physicallayer::Signal *>(obj))
            recordPacket(signal->getEncapsulatedPacket(), direction, source);
#endif
        else if (auto packet = dynamic_cast<cPacket *>(obj))
            recordPacket(packet, direction, source);
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
        else if (auto transmission = dynamic_cast<const physicallayer::ITransmission *>(obj)) {
            physicalLayerTransmission = transmission;
            recordPacket(transmission->getPacket(), direction, source);
            physicalLayerTransmission = nullptr;
        }
        else if (auto reception = dynamic_cast<const physicallayer::IReception *>(obj)) {
            physicalLayerTransmission = reception->getTransmission();
            physicalLayerReception = reception;
            recordPacket(reception->getTransmission()->getPacket(), direction, source);
            physicalLayerTransmission = nullptr;
            physicalLayerReception = nullptr;
        }
#endif
    }
}

void PcapRecorder::writePacket(const Protocol *protocol, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface)
{
#ifdef INET_WITH_IEEE80211
    if (*protocol == Protocol::ieee80211Mac) {
        std::vector<MpduRange> mpduRanges;
        if (getIeee80211AmpduMpduRanges(packet, frontOffset, backOffset, mpduRanges)) {
            for (const auto& mpduRange : mpduRanges)
                writePacketRecord(protocol, packet, mpduRange.offset, packet->getDataLength() - mpduRange.offset - mpduRange.length, direction, networkInterface);
            return;
        }
    }
#endif
    writePacketRecord(protocol, packet, frontOffset, backOffset, direction, networkInterface);
}

void PcapRecorder::writePacketRecord(const Protocol *protocol, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface)
{
    auto pcapLinkType = protocolToLinkType(protocol);
    if (pcapLinkType == LINKTYPE_INVALID)
        throw cRuntimeError("Cannot determine the PCAP link type from protocol '%s'", protocol->getName());
    bool convertPacket = !matchesLinkType(pcapLinkType, protocol);
    if (convertPacket) {
        recordingDirection = direction;
        packet = tryConvertToLinkType(packet, frontOffset, backOffset, pcapLinkType, protocol);
        recordingDirection = DIRECTION_UNDEFINED;
        if (packet == nullptr)
            throw cRuntimeError("The protocol '%s' doesn't match PCAP link type %d", protocol->getName(), pcapLinkType);
        frontOffset = b(0);
        backOffset = b(0);
    }
    b recordedLength = packet->getDataLength() - frontOffset - backOffset;
    if (recordEmptyPackets || recordedLength != b(0)) {
        pcapWriter->writePacket(simTime(), packet, frontOffset, backOffset, direction, networkInterface, pcapLinkType);
        numRecorded++;
        emit(packetRecordedSignal, packet);
    }
    if (convertPacket)
        delete packet;
}

void PcapRecorder::recordPacket(const cPacket *cpacket, Direction direction, cComponent *source)
{
    if (auto packet = dynamic_cast<const Packet *>(cpacket)) {
        EV_INFO << "Recording packet" << EV_FIELD(source, source->getFullPath()) << EV_FIELD(direction, direction) << EV_FIELD(packet) << EV_ENDL;
        if (verbose)
            EV_DEBUG << "Dumping packet" << EV_FIELD(packet, packetPrinter.printPacketToString(const_cast<Packet *>(packet), "%i")) << EV_ENDL;
        if (recordPcap && packetFilter.matches(packet) && (dumpBadFrames || !packet->hasBitError())) {
            // get Direction
            if (direction == DIRECTION_UNDEFINED) {
                if (auto directionTag = packet->findTag<DirectionTag>())
                    direction = directionTag->getDirection();
            }

            // get NetworkInterface
            auto srcModule = check_and_cast<cModule *>(source);
            auto networkInterface = findContainingNicModule(srcModule);
            if (networkInterface == nullptr) {
                int ifaceId = -1;
                if (direction == DIRECTION_OUTBOUND) {
                    if (auto ifaceTag = packet->findTag<InterfaceReq>())
                        ifaceId = ifaceTag->getInterfaceId();
                }
                else if (direction == DIRECTION_INBOUND) {
                    if (auto ifaceTag = packet->findTag<InterfaceInd>())
                        ifaceId = ifaceTag->getInterfaceId();
                }
                if (ifaceId != -1) {
                    auto ift = check_and_cast_nullable<InterfaceTable *>(getContainingNode(srcModule)->getSubmodule("interfaceTable"));
                    networkInterface = ift->getInterfaceById(ifaceId);
                }
            }

            const auto& packetProtocolTag = packet->getTag<PacketProtocolTag>();
            auto protocol = packetProtocolTag->getProtocol();
            if (contains(dumpProtocols, protocol))
                writePacket(protocol, packet, packetProtocolTag->getFrontOffset(), packetProtocolTag->getBackOffset(), direction, networkInterface);
            else {
                frontOffset = b(0);
                backOffset = b(0);
                dumpProtocol = nullptr;
                Packet dissectedPacket(*packet);
                PacketDissector packetDissector(ProtocolDissectorRegistry::getInstance(), *this);
                packetDissector.dissectPacket(&dissectedPacket);
                if (dumpProtocol != nullptr)
                    writePacket(dumpProtocol, packet, frontOffset, backOffset, direction, networkInterface);
            }
        }
    }
}

void PcapRecorder::finish()
{
    pcapWriter->close();
}

bool PcapRecorder::matchesLinkType(PcapLinkType pcapLinkType, const Protocol *protocol) const
{
    if (protocol == nullptr)
        return false;
    else if (*protocol == Protocol::ethernetPhy)
        return pcapLinkType == LINKTYPE_ETHERNET_MPACKET;
    else if (*protocol == Protocol::ethernetMac)
        return pcapLinkType == LINKTYPE_ETHERNET;
    else if (*protocol == Protocol::ppp)
        return pcapLinkType == LINKTYPE_PPP_WITH_DIR;
    else if (*protocol == Protocol::ieee80211Mac)
        // A bare MAC frame only matches the non-Radiotap IEEE 802.11 link type.
        return pcapLinkType == LINKTYPE_IEEE802_11;
    else if (*protocol == Protocol::ipv4)
        return pcapLinkType == LINKTYPE_RAW || pcapLinkType == LINKTYPE_IPV4;
    else if (*protocol == Protocol::ipv6)
        return pcapLinkType == LINKTYPE_RAW || pcapLinkType == LINKTYPE_IPV6;
    else if (*protocol == Protocol::ieee802154)
        return pcapLinkType == LINKTYPE_IEEE802_15_4 || pcapLinkType == LINKTYPE_IEEE802_15_4_NOFCS;
    else {
        for (auto helper : helpers) {
            if (helper->matchesLinkType(pcapLinkType, protocol))
                return true;
        }
    }
    return false;
}

PcapLinkType PcapRecorder::protocolToLinkType(const Protocol *protocol) const
{
    if (*protocol == Protocol::ethernetPhy)
        return LINKTYPE_ETHERNET_MPACKET;
    else if (*protocol == Protocol::ethernetMac)
        return LINKTYPE_ETHERNET;
    else if (*protocol == Protocol::ppp)
        return LINKTYPE_PPP_WITH_DIR;
    else if (*protocol == Protocol::ieee80211Mac)
        return LINKTYPE_IEEE802_11_RADIOTAP;
    else if (*protocol == Protocol::ipv4 || *protocol == Protocol::ipv6)
        return LINKTYPE_RAW;
    else if (*protocol == Protocol::ieee802154)
        return LINKTYPE_IEEE802_15_4;
    else {
        for (auto helper : helpers) {
            auto lt = helper->protocolToLinkType(protocol);
            if (lt != LINKTYPE_INVALID)
                return lt;
        }
    }
    return LINKTYPE_INVALID;
}

Packet *PcapRecorder::tryConvertToLinkType(const Packet *packet, b frontOffset, b backOffset, PcapLinkType pcapLinkType, const Protocol *protocol) const
{
    if (enableConvertingPackets) {
        if (*protocol == Protocol::ieee80211Mac && pcapLinkType == LINKTYPE_IEEE802_11_RADIOTAP) {
            auto convertedPacket = new Packet(packet->getName());
            convertedPacket->insertAtBack(makeShared<BytesChunk>(makeRadiotapHeader(packet, recordingDirection, physicalLayerTransmission, physicalLayerReception)));
            b dataLength = packet->getDataLength() - frontOffset - backOffset;
            if (dataLength != b(0))
                convertedPacket->insertAtBack(packet->peekDataAt(frontOffset, dataLength));
            convertedPacket->setBitError(packet->hasBitError());
            return convertedPacket;
        }
        for (IHelper *helper : helpers) {
            if (auto newPacket = helper->tryConvertToLinkType(packet, frontOffset, backOffset, pcapLinkType, protocol))
                return newPacket;
        }
    }
    return nullptr;
}

} // namespace inet
