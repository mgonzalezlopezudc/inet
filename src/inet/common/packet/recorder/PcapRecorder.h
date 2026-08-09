//
// Copyright (C) 2005 Michael Tuexen
// Copyright (C) 2008 Irene Ruengeler
// Copyright (C) 2009 Thomas Dreibholz
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_PCAPRECORDER_H
#define __INET_PCAPRECORDER_H

#include <unordered_map>
#include <unordered_set>

#include "inet/common/SimpleModule.h"
#include "inet/common/packet/dissector/PacketDissector.h"
#include "inet/common/packet/PacketFilter.h"
#include "inet/common/packet/printer/PacketPrinter.h"
#include "inet/common/packet/recorder/IPcapCaptureAdapter.h"
#include "inet/common/packet/recorder/IPcapWriter.h"

namespace inet {

/**
 * Dumps every packet using the IPacketWriter and PacketDump classes
 */
class INET_API PcapRecorder : public SimpleModule, protected cListener, public PacketDissector::ICallback
{
  public:
    class INET_API IHelper {
      public:
        virtual ~IHelper() {}

        /// returns pcapLinkType for given protocol or returns LINKTYPE_INVALID. Protocol storable as or convertable to pcapLinkType.
        virtual PcapLinkType protocolToLinkType(const Protocol *protocol) const = 0;

        /// returns true when the protocol storable as pcapLinkType without conversion.
        virtual bool matchesLinkType(PcapLinkType pcapLinkType, const Protocol *protocol) const = 0;

        /// Create a new Packet or return nullptr. The new packet contains the original packet converted to pcapLinkType format.
        virtual Packet *tryConvertToLinkType(const Packet *packet, b frontOffset, b backOffset, PcapLinkType pcapLinkType, const Protocol *protocol) const = 0;
    };

  protected:
    typedef std::map<simsignal_t, Direction> SignalList;
    std::vector<const Protocol *> dumpProtocols;
    std::unordered_set<const Protocol *> dumpProtocolSet;
    SignalList signalList;
    IPcapWriter *pcapWriter = nullptr;
    unsigned int snaplen = 0;
    bool dumpBadFrames = false;
    PacketFilter packetFilter;
    int numRecorded = 0;
    bool verbose = false;
    bool recordEmptyPackets = false;
    bool enableConvertingPackets = true;
    bool recordPcap = false;
    std::vector<IHelper *> helpers;
    PacketPrinter packetPrinter;
    Direction captureDirection = DIRECTION_UNDEFINED;
    simtime_t captureStartTime = SIMTIME_ZERO;
    simtime_t captureEndTime = SimTime(-1, SIMTIME_S);
    int64_t maxNumRecords = -1;
    std::unordered_map<int, int> sourceModuleToInterfaceModule;
    std::unordered_map<int, int> nodeModuleToInterfaceTableModule;

    int64_t numOfferedPackets = 0;
    int64_t numDirectionRejected = 0;
    int64_t numTimeRejected = 0;
    int64_t numLimitRejected = 0;
    int64_t numFilterRejected = 0;
    int64_t numBadFrameRejected = 0;
    int64_t numEmptyRejected = 0;
    int64_t numDirectProtocols = 0;
    int64_t numFallbackDissections = 0;
    int64_t numConversions = 0;
    int64_t numAmpduMpduRecords = 0;
    int64_t numDirectInterfaceResolutions = 0;
    int64_t numCachedInterfaceResolutions = 0;
    int64_t numTaggedInterfaceResolutions = 0;
    int64_t numUnresolvedInterfaceResolutions = 0;
    int64_t numPayloadBytesRequested = 0;

    static simsignal_t packetRecordedSignal;

    b frontOffset;
    b backOffset;
    const Protocol *dumpProtocol = nullptr;

  public:
    PcapRecorder();
    virtual ~PcapRecorder();
    virtual std::string resolveDirective(char directive) const override;

    virtual bool shouldDissectProtocolDataUnit(const Protocol *protocol) override;

    virtual void startProtocolDataUnit(const Protocol *protocol) override;
    virtual void endProtocolDataUnit(const Protocol *protocol) override { }
    virtual void markIncorrect() override { }

    virtual void visitChunk(const Ptr<const Chunk>& chunk, const Protocol *protocol) override;

  protected:
    virtual void initialize() override;
    virtual IPcapWriter *createPcapWriter(const char *fileFormat) const;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
    virtual void recordPacket(const PcapCaptureObservation& observation, cComponent *source);
    virtual NetworkInterface *resolveNetworkInterface(cModule *sourceModule, const Packet *packet, Direction direction);
    virtual bool isDumpProtocol(const Protocol *protocol) const;
    virtual bool matchesLinkType(PcapLinkType pcapLinkType, const Protocol *protocol) const;
    virtual Packet *tryConvertToLinkType(const Packet *packet, b frontOffset, b backOffset, PcapLinkType pcapLinkType, const Protocol *protocol) const;
    virtual PcapLinkType protocolToLinkType(const Protocol *protocol) const;
    virtual void writePacket(const Protocol *protocol, const PcapCaptureObservation& observation, b frontOffset, b backOffset, NetworkInterface *networkInterface);
    virtual bool writePacketRecord(const Protocol *protocol, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface,
            PcapLinkType linkType = LINKTYPE_INVALID, const std::vector<uint8_t>& packetPrefix = {}, bool adapterConversion = false);
};

} // namespace inet

#endif
