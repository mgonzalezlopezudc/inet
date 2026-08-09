//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_PCAPNGWRITER_H
#define __INET_PCAPNGWRITER_H

#include "inet/common/packet/Packet.h"
#include "inet/common/packet/recorder/IPcapWriter.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {

/**
 * Dumps packets into a PCAP Next Generation file; see the "pcap-savefile"
 * man page or http://www.tcpdump.org/ for details on the file format.
 */
class INET_API PcapngWriter : public IPcapWriter, public ISegmentedPcapWriter
{
  protected:
    std::string fileName;
    FILE *dumpfile = nullptr; // pcap file
    bool flush = false;
    int nextPcapngInterfaceId = 0;
    int timePrecision = 6;
    unsigned int snaplen = 0;
    size_t bufferSize = 0;
    size_t flushInterval = 0;
    size_t numRecordsWritten = 0;
    uint64_t numPayloadBytesWritten = 0;
    uint64_t numFlushes = 0;
    std::vector<char> stdioBuffer;
    struct InterfaceEntry {
        PcapInterfaceDescriptor descriptor;
        PcapLinkType linkType;
        int pcapngInterfaceId;
    };
    std::vector<InterfaceEntry> interfaces;

  protected:
    virtual size_t writeFile(const void *data, size_t size, size_t count, FILE *file) { return fwrite(data, size, count, file); }
    virtual int flushFile(FILE *file) { return fflush(file); }
    virtual int closeFileHandle(FILE *file) { return fclose(file); }
    virtual int setFileBuffer(FILE *file, char *buffer, int mode, size_t size) { return setvbuf(file, buffer, mode, size); }
    void writeBytes(const void *data, size_t length);
    void closeFile(bool checkError);

  public:
    /**
     * Constructor. It does not open the output file.
     */
    PcapngWriter() {}

    /**
     * Destructor. It closes the output file if it is open.
     */
    ~PcapngWriter();

    /**
     * Opens a PCAP file with the given file name. Throws an exception
     * if the file cannot be opened.
     */
    void open(const char *filename, unsigned int snaplen, int timePrecision) override;

    /**
     * Returns true if the pcap file is currently open.
     */
    bool isOpen() const override { return dumpfile != nullptr; }

    /**
     * Records the interface into the output file.
     */
    void writeInterface(const PcapInterfaceDescriptor& interfaceDescriptor, PcapLinkType linkType);

    /**
     * Records the given packet into the output file if it is open,
     * and throws an exception otherwise.
     */
    void writePacket(simtime_t time, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *ie, PcapLinkType linkType) override;
    void writePacketWithPrefix(simtime_t time, const Packet *packet, b frontOffset, b backOffset, Direction direction, const PcapInterfaceDescriptor& interfaceDescriptor, PcapLinkType linkType,
            const std::vector<uint8_t>& packetPrefix) override;
    void setBufferSize(size_t bufferSize) override;
    void setFlushInterval(size_t flushInterval) override { this->flushInterval = flushInterval; }
    uint64_t getNumPayloadBytesWritten() const override { return numPayloadBytesWritten; }
    uint64_t getNumFlushes() const override { return numFlushes; }

    /**
     * Closes the output file if it is open.
     */
    void close() override;

    /**
     * Force flushing of pcap dump.
     */
    void setFlush(bool flush) override { this->flush = flush; }
};

} // namespace inet

#endif
