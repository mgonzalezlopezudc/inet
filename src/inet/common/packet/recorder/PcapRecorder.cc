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
#include <cstdint>
#include <vector>

#include "inet/common/DirectionTag_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/packet/recorder/PcapCaptureAdapterRegistry.h"
#include "inet/common/packet/recorder/PcapngWriter.h"
#include "inet/common/packet/recorder/PcapWriter.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/stlutils.h"
#include "inet/common/StringFormat.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/InterfaceTable.h"

namespace inet {

Define_Module(PcapRecorder);

simsignal_t PcapRecorder::packetRecordedSignal = registerSignal("packetRecorded");

PcapRecorder::~PcapRecorder()
{
    delete pcapWriter;
    for (auto helper : helpers)
        delete helper;
}

IPcapWriter *PcapRecorder::createPcapWriter(const char *fileFormat) const
{
    if (!strcmp(fileFormat, "pcap"))
        return new PcapWriter();
    else if (!strcmp(fileFormat, "pcapng"))
        return new PcapngWriter();
    else
        throw cRuntimeError("Unknown fileFormat parameter: '%s'", fileFormat);
}

PcapRecorder::PcapRecorder() : SimpleModule()
{
}

bool PcapRecorder::shouldDissectProtocolDataUnit(const Protocol *protocol)
{
    return !isDumpProtocol(protocol);
}

void PcapRecorder::startProtocolDataUnit(const Protocol *protocol)
{
    if (isDumpProtocol(protocol))
        dumpProtocol = protocol;
}

void PcapRecorder::visitChunk(const Ptr<const Chunk>& chunk, const Protocol *protocol)
{
    if (!isDumpProtocol(protocol)) {
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
    const char *captureDirectionString = par("captureDirection");
    if (!strcmp(captureDirectionString, "both"))
        captureDirection = DIRECTION_UNDEFINED;
    else if (!strcmp(captureDirectionString, "inbound"))
        captureDirection = DIRECTION_INBOUND;
    else if (!strcmp(captureDirectionString, "outbound"))
        captureDirection = DIRECTION_OUTBOUND;
    else
        throw cRuntimeError("Unknown captureDirection parameter: '%s'", captureDirectionString);
    captureStartTime = par("captureStartTime");
    captureEndTime = par("captureEndTime");
    maxNumRecords = par("maxNumRecords");
    int64_t outputBufferSize = par("outputBufferSize");
    int64_t flushInterval = par("flushInterval");
    if (outputBufferSize < 0)
        throw cRuntimeError("outputBufferSize must be nonnegative");
    if (flushInterval < 0)
        throw cRuntimeError("flushInterval must be nonnegative");
    if (captureStartTime < SIMTIME_ZERO)
        throw cRuntimeError("captureStartTime must be nonnegative");
    if (captureEndTime != SimTime(-1, SIMTIME_S) && captureEndTime < captureStartTime)
        throw cRuntimeError("captureEndTime must be -1s or not precede captureStartTime");
    if (maxNumRecords < -1)
        throw cRuntimeError("maxNumRecords must be -1 or nonnegative");
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
        {
            auto protocol = Protocol::getProtocol(protocolTokenizer.nextToken());
            dumpProtocols.push_back(protocol);
            dumpProtocolSet.insert(protocol);
        }
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
    pcapWriter = createPcapWriter(fileFormat);

    recordPcap = !fileName.empty();
    if (recordPcap) {
        if (auto segmentedPcapWriter = dynamic_cast<ISegmentedPcapWriter *>(pcapWriter)) {
            segmentedPcapWriter->setBufferSize(outputBufferSize);
            segmentedPcapWriter->setFlushInterval(flushInterval);
        }
        pcapWriter->setFlush(par("alwaysFlush"));
        pcapWriter->open(fileName.c_str(), snaplen, timePrecision);
    }

    WATCH(recordPcap);
    WATCH(frontOffset);
    WATCH(backOffset);
    WATCH(numRecorded);
    WATCH(numOfferedPackets);
    WATCH(numDirectionRejected);
    WATCH(numTimeRejected);
    WATCH(numLimitRejected);
    WATCH(numFilterRejected);
    WATCH(numBadFrameRejected);
    WATCH(numEmptyRejected);
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
        auto observation = PcapCaptureAdapterRegistry::getInstance().tryCreateObservation(obj, direction);
        if (observation.has_value())
            recordPacket(*observation, source);
        else if (auto packet = dynamic_cast<const Packet *>(obj))
            recordPacket(PcapCaptureObservation(packet, direction), source);
    }
}

void PcapRecorder::writePacket(const Protocol *protocol, const PcapCaptureObservation& observation, b frontOffset, b backOffset, NetworkInterface *networkInterface)
{
    auto adapter = PcapCaptureAdapterRegistry::getInstance().findProtocolAdapter(protocol);
    if (adapter != nullptr && enableConvertingPackets) {
        auto records = adapter->createRecords(observation, frontOffset, backOffset);
        for (const auto& record : records) {
            if (writePacketRecord(protocol, observation.packet, record.frontOffset, record.backOffset, observation.direction, networkInterface,
                    adapter->getLinkType(), record.getPrefix(), true) && record.aggregateSubframe)
                numAmpduMpduRecords++;
        }
        return;
    }
    writePacketRecord(protocol, observation.packet, frontOffset, backOffset, observation.direction, networkInterface);
}

bool PcapRecorder::writePacketRecord(const Protocol *protocol, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface,
        PcapLinkType linkType, const std::vector<uint8_t>& packetPrefix, bool adapterConversion)
{
    if (maxNumRecords >= 0 && numRecorded >= maxNumRecords) {
        numLimitRejected++;
        return false;
    }
    auto pcapLinkType = linkType == LINKTYPE_INVALID ? protocolToLinkType(protocol) : linkType;
    if (pcapLinkType == LINKTYPE_INVALID)
        throw cRuntimeError("Cannot determine the PCAP link type from protocol '%s'", protocol->getName());
    bool convertPacket = !adapterConversion && !matchesLinkType(pcapLinkType, protocol);
    auto segmentedPcapWriter = !packetPrefix.empty() ? dynamic_cast<ISegmentedPcapWriter *>(pcapWriter) : nullptr;
    bool deletePacket = false;
    if (adapterConversion)
        numConversions++;
    if (adapterConversion && segmentedPcapWriter == nullptr && !packetPrefix.empty()) {
        auto convertedPacket = new Packet(packet->getName());
        convertedPacket->insertAtBack(makeShared<BytesChunk>(packetPrefix));
        b dataLength = packet->getDataLength() - frontOffset - backOffset;
        if (dataLength != b(0))
            convertedPacket->insertAtBack(packet->peekDataAt(frontOffset, dataLength));
        convertedPacket->setBitError(packet->hasBitError());
        packet = convertedPacket;
        frontOffset = backOffset = b(0);
        deletePacket = true;
    }
    if (convertPacket) {
        packet = tryConvertToLinkType(packet, frontOffset, backOffset, pcapLinkType, protocol);
        if (packet == nullptr)
            throw cRuntimeError("The protocol '%s' doesn't match PCAP link type %d", protocol->getName(), pcapLinkType);
        numConversions++;
        frontOffset = b(0);
        backOffset = b(0);
        deletePacket = true;
    }
    b recordedLength = packet->getDataLength() - frontOffset - backOffset;
    if (segmentedPcapWriter != nullptr)
        recordedLength += B(packetPrefix.size());
    if (recordEmptyPackets || recordedLength != b(0)) {
        numPayloadBytesRequested += recordedLength.get<B>();
        if (segmentedPcapWriter != nullptr) {
            auto interfaceDescriptor = makePcapInterfaceDescriptor(networkInterface);
            segmentedPcapWriter->writePacketWithPrefix(simTime(), packet, frontOffset, backOffset, direction, interfaceDescriptor, pcapLinkType, packetPrefix);
        }
        else
            pcapWriter->writePacket(simTime(), packet, frontOffset, backOffset, direction, networkInterface, pcapLinkType);
        numRecorded++;
        if (segmentedPcapWriter != nullptr && hasListeners(packetRecordedSignal)) {
            auto recordedPacket = new Packet(packet->getName());
            recordedPacket->insertAtBack(makeShared<BytesChunk>(packetPrefix));
            b dataLength = packet->getDataLength() - frontOffset - backOffset;
            if (dataLength != b(0))
                recordedPacket->insertAtBack(packet->peekDataAt(frontOffset, dataLength));
            recordedPacket->setBitError(packet->hasBitError());
            emit(packetRecordedSignal, recordedPacket);
            delete recordedPacket;
        }
        else if (segmentedPcapWriter == nullptr)
            emit(packetRecordedSignal, packet);
    }
    else
        numEmptyRejected++;
    if (deletePacket)
        delete packet;
    return recordEmptyPackets || recordedLength != b(0);
}

bool PcapRecorder::isDumpProtocol(const Protocol *protocol) const
{
    return dumpProtocolSet.find(protocol) != dumpProtocolSet.end();
}

NetworkInterface *PcapRecorder::resolveNetworkInterface(cModule *sourceModule, const Packet *packet, Direction direction)
{
    auto sourceModuleId = sourceModule->getId();
    auto cachedInterface = sourceModuleToInterfaceModule.find(sourceModuleId);
    if (cachedInterface != sourceModuleToInterfaceModule.end()) {
        auto networkInterface = dynamic_cast<NetworkInterface *>(getSimulation()->getModule(cachedInterface->second));
        if (networkInterface != nullptr) {
            numCachedInterfaceResolutions++;
            return networkInterface;
        }
        sourceModuleToInterfaceModule.erase(cachedInterface);
    }

    if (auto networkInterface = findContainingNicModule(sourceModule)) {
        sourceModuleToInterfaceModule[sourceModuleId] = networkInterface->getId();
        numDirectInterfaceResolutions++;
        return networkInterface;
    }

    int interfaceId = -1;
    if (direction == DIRECTION_OUTBOUND) {
        if (auto interfaceTag = packet->findTag<InterfaceReq>())
            interfaceId = interfaceTag->getInterfaceId();
    }
    else if (direction == DIRECTION_INBOUND) {
        if (auto interfaceTag = packet->findTag<InterfaceInd>())
            interfaceId = interfaceTag->getInterfaceId();
    }
    if (interfaceId != -1) {
        auto node = findContainingNode(sourceModule);
        if (node != nullptr) {
            InterfaceTable *interfaceTable = nullptr;
            auto cachedInterfaceTable = nodeModuleToInterfaceTableModule.find(node->getId());
            if (cachedInterfaceTable != nodeModuleToInterfaceTableModule.end()) {
                interfaceTable = dynamic_cast<InterfaceTable *>(getSimulation()->getModule(cachedInterfaceTable->second));
                if (interfaceTable == nullptr)
                    nodeModuleToInterfaceTableModule.erase(cachedInterfaceTable);
            }
            if (interfaceTable == nullptr) {
                interfaceTable = dynamic_cast<InterfaceTable *>(node->getSubmodule("interfaceTable"));
                if (interfaceTable != nullptr)
                    nodeModuleToInterfaceTableModule[node->getId()] = interfaceTable->getId();
            }
            if (interfaceTable != nullptr) {
                auto networkInterface = interfaceTable->getInterfaceById(interfaceId);
                if (networkInterface != nullptr) {
                    numTaggedInterfaceResolutions++;
                    return networkInterface;
                }
            }
        }
    }
    numUnresolvedInterfaceResolutions++;
    return nullptr;
}

void PcapRecorder::recordPacket(const PcapCaptureObservation& observation, cComponent *source)
{
    auto packet = observation.packet;
    Direction direction = observation.direction;
    numOfferedPackets++;
    if (direction == DIRECTION_UNDEFINED) {
        if (auto directionTag = packet->findTag<DirectionTag>())
            direction = directionTag->getDirection();
    }
    if (captureDirection != DIRECTION_UNDEFINED && direction != captureDirection) {
        numDirectionRejected++;
        return;
    }
    if (simTime() < captureStartTime || (captureEndTime >= SIMTIME_ZERO && simTime() > captureEndTime)) {
        numTimeRejected++;
        return;
    }
    if (maxNumRecords >= 0 && numRecorded >= maxNumRecords) {
        numLimitRejected++;
        return;
    }

    EV_INFO << "Recording packet" << EV_FIELD(source, source->getFullPath()) << EV_FIELD(direction, direction) << EV_FIELD(packet) << EV_ENDL;
    if (verbose)
        EV_DEBUG << "Dumping packet" << EV_FIELD(packet, packetPrinter.printPacketToString(const_cast<Packet *>(packet), "%i")) << EV_ENDL;
    if (!recordPcap)
        return;
    if (!packetFilter.matches(packet)) {
        numFilterRejected++;
        return;
    }
    if (!dumpBadFrames && packet->hasBitError()) {
        numBadFrameRejected++;
        return;
    }

    auto sourceModule = check_and_cast<cModule *>(source);
    auto networkInterface = resolveNetworkInterface(sourceModule, packet, direction);
    PcapCaptureObservation effectiveObservation(packet, direction, observation.transmission, observation.reception);

    const auto& packetProtocolTag = packet->getTag<PacketProtocolTag>();
    auto protocol = packetProtocolTag->getProtocol();
    if (isDumpProtocol(protocol)) {
        numDirectProtocols++;
        writePacket(protocol, effectiveObservation, packetProtocolTag->getFrontOffset(), packetProtocolTag->getBackOffset(), networkInterface);
    }
    else if (auto resolution = PcapCaptureAdapterRegistry::getInstance().tryResolveProtocol(protocol, packet,
            packetProtocolTag->getFrontOffset(), packetProtocolTag->getBackOffset());
            resolution.has_value() && isDumpProtocol(std::get<0>(*resolution))) {
        numDirectProtocols++;
        writePacket(std::get<0>(*resolution), effectiveObservation, std::get<1>(*resolution), std::get<2>(*resolution), networkInterface);
    }
    else {
        numFallbackDissections++;
        frontOffset = b(0);
        backOffset = b(0);
        dumpProtocol = nullptr;
        Packet dissectedPacket(*packet);
        PacketDissector packetDissector(ProtocolDissectorRegistry::getInstance(), *this);
        packetDissector.dissectPacket(&dissectedPacket);
        if (dumpProtocol != nullptr)
            writePacket(dumpProtocol, effectiveObservation, frontOffset, backOffset, networkInterface);
    }
}

void PcapRecorder::finish()
{
    auto segmentedPcapWriter = dynamic_cast<ISegmentedPcapWriter *>(pcapWriter);
    pcapWriter->close();
    auto emitCount = [this](const char *signalName, int64_t value) { emit(registerSignal(signalName), value); };
    emitCount("offeredPacketCount", numOfferedPackets);
    emitCount("directionRejectedCount", numDirectionRejected);
    emitCount("timeRejectedCount", numTimeRejected);
    emitCount("limitRejectedCount", numLimitRejected);
    emitCount("filterRejectedCount", numFilterRejected);
    emitCount("badFrameRejectedCount", numBadFrameRejected);
    emitCount("emptyRejectedCount", numEmptyRejected);
    emitCount("directProtocolCount", numDirectProtocols);
    emitCount("fallbackDissectionCount", numFallbackDissections);
    emitCount("conversionCount", numConversions);
    emitCount("ampduMpduRecordCount", numAmpduMpduRecords);
    emitCount("directInterfaceResolutionCount", numDirectInterfaceResolutions);
    emitCount("cachedInterfaceResolutionCount", numCachedInterfaceResolutions);
    emitCount("taggedInterfaceResolutionCount", numTaggedInterfaceResolutions);
    emitCount("unresolvedInterfaceResolutionCount", numUnresolvedInterfaceResolutions);
    emitCount("payloadBytesRequested", numPayloadBytesRequested);
    emitCount("payloadBytesWritten", segmentedPcapWriter != nullptr ? segmentedPcapWriter->getNumPayloadBytesWritten() : 0);
    emitCount("writerFlushCount", segmentedPcapWriter != nullptr ? segmentedPcapWriter->getNumFlushes() : 0);
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
    auto captureAdapter = PcapCaptureAdapterRegistry::getInstance().findProtocolAdapter(protocol);
    if (captureAdapter != nullptr)
        return captureAdapter->getLinkType();
    else if (*protocol == Protocol::ethernetPhy)
        return LINKTYPE_ETHERNET_MPACKET;
    else if (*protocol == Protocol::ethernetMac)
        return LINKTYPE_ETHERNET;
    else if (*protocol == Protocol::ppp)
        return LINKTYPE_PPP_WITH_DIR;
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
        for (IHelper *helper : helpers) {
            if (auto newPacket = helper->tryConvertToLinkType(packet, frontOffset, backOffset, pcapLinkType, protocol))
                return newPacket;
        }
    }
    return nullptr;
}

} // namespace inet
