//
// Copyright (C) 2005 Michael Tuexen
// Copyright (C) 2008 Irene Ruengeler
// Copyright (C) 2009 Thomas Dreibholz
// Copyright (C) 2009 Thomas Reschka
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/common/packet/recorder/PcapWriter.h"

#include <algorithm>
#include <cerrno>
#include <limits>

#include "inet/common/INETUtils.h"
#include "inet/common/packet/chunk/BytesChunk.h"

namespace inet {

#define PCAP_MAGIC      0xa1b2c3d4
#define PCAP_MAGIC_NANO 0xa1b23c4d

/* "libpcap" file header (minus magic number). */
struct pcap_hdr
{
    uint32_t magic; /* magic */
    uint16_t version_major; /* major version number */
    uint16_t version_minor; /* minor version number */
    uint32_t thiszone; /* GMT to local correction */
    uint32_t sigfigs; /* accuracy of timestamps */
    uint32_t snaplen; /* max length of captured packets, in octets */
    uint32_t network; /* data link type */
};

/* "libpcap" record header. */
struct pcaprec_hdr
{
    int32_t ts_sec; /* timestamp seconds */
    uint32_t ts_usec; /* timestamp microseconds */
    uint32_t incl_len; /* number of octets of packet saved in file */
    uint32_t orig_len; /* actual length of packet */
};

static void writeBytes(FILE *file, const void *data, size_t length, const std::string& fileName)
{
    if (length != 0 && fwrite(data, length, 1, file) != 1)
        throw cRuntimeError("Cannot write pcap file [%s]: %s", fileName.c_str(), strerror(errno));
}

PcapWriter::~PcapWriter()
{
    closeFile(false);
}

void PcapWriter::open(const char *filename, unsigned int snaplen_par, int timePrecision)
{
    if (opp_isempty(filename))
        throw cRuntimeError("Cannot open pcap file: file name is empty");

    switch (timePrecision) {
        case 6:
        case 9:
            this->timePrecision = timePrecision;
            break;
        default: throw cRuntimeError("Unsupported time precision (%d) in PcapWriter.", timePrecision);
    }
    inet::utils::makePathForFile(filename);
    dumpfile = fopen(filename, "wb");
    fileName = filename;

    if (!dumpfile)
        throw cRuntimeError("Cannot open pcap file [%s] for writing: %s", filename, strerror(errno));

    snaplen = snaplen_par;
    needHeader = true;
    network = LINKTYPE_INVALID;

    flush = false;
}

void PcapWriter::writeHeader(PcapLinkType linkType)
{
    struct pcap_hdr fh;

    switch(timePrecision) {
        case 6: fh.magic = PCAP_MAGIC; break;
        case 9: fh.magic = PCAP_MAGIC_NANO; break;
        default: throw cRuntimeError("Unsupported time precision (%d) in PcapWriter.", timePrecision);
    }
    fh.version_major = 2;
    fh.version_minor = 4;
    fh.thiszone = 0;
    fh.sigfigs = 0;
    fh.snaplen = snaplen;
    fh.network = linkType;
    writeBytes(dumpfile, &fh, sizeof(fh), fileName);
}

void PcapWriter::writePacket(simtime_t stime, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *ie, PcapLinkType linkTypePar)
{
    writePacketWithPrefix(stime, packet, frontOffset, backOffset, direction, ie, linkTypePar, {});
}

void PcapWriter::writePacketWithPrefix(simtime_t stime, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *ie, PcapLinkType linkTypePar,
        const std::vector<uint8_t>& packetPrefix)
{
    if (!dumpfile)
        throw cRuntimeError("Cannot write frame: pcap output file is not open");

    if (needHeader) {
        if (linkTypePar == LINKTYPE_INVALID)
            throw cRuntimeError("invalid linktype arrived");
        writeHeader(linkTypePar);
        network = linkTypePar;
        needHeader = false;
    }
    else {
        if (network != linkTypePar)
            throw cRuntimeError("linktype mismatch error: required linktype = %d, arrived linktype = %d", network, linkTypePar);
    }

    (void)direction; // unused
    (void)ie; // unused

    EV_INFO << "Writing packet" << EV_FIELD(packet) << EV_FIELD(fileName) << EV_ENDL;
    struct pcaprec_hdr ph;
    ph.ts_sec = (int32_t)stime.inUnit(SIMTIME_S);
    switch(timePrecision) {
        case 6: ph.ts_usec = (uint32_t)(stime.inUnit(SIMTIME_US) - (uint32_t)1000000 * stime.inUnit(SIMTIME_S)); break;
        case 9: ph.ts_usec = (uint32_t)(stime.inUnit(SIMTIME_NS) - (uint32_t)1000000000 * stime.inUnit(SIMTIME_S)); break;
        default: throw cRuntimeError("Unsupported time precision (%d) in PcapWriter.", timePrecision);
    }
    b packetDataLength = packet->getDataLength() - frontOffset - backOffset;
    auto packetDataLengthBytes = packetDataLength.get<B>();
    if (packetDataLengthBytes < 0 || packetPrefix.size() > std::numeric_limits<uint32_t>::max() ||
            static_cast<uint64_t>(packetDataLengthBytes) + packetPrefix.size() > std::numeric_limits<uint32_t>::max())
        throw cRuntimeError("Invalid original packet length");

    ph.orig_len = static_cast<uint32_t>(packetDataLengthBytes + packetPrefix.size());
    ph.incl_len = std::min(ph.orig_len, snaplen);

    writeBytes(dumpfile, &ph, sizeof(ph), fileName);
    auto capturedPrefixLength = std::min<size_t>(packetPrefix.size(), ph.incl_len);
    writeBytes(dumpfile, packetPrefix.data(), capturedPrefixLength, fileName);
    auto capturedPacketLength = ph.incl_len - capturedPrefixLength;
    if (capturedPacketLength != 0) {
        auto data = packet->peekDataAt<BytesChunk>(frontOffset, B(capturedPacketLength));
        const auto& bytes = data->getBytes();
        writeBytes(dumpfile, bytes.data(), bytes.size(), fileName);
    }
    if (flush && fflush(dumpfile) != 0)
        throw cRuntimeError("Cannot flush pcap file [%s]: %s", fileName.c_str(), strerror(errno));
}

void PcapWriter::close()
{
    closeFile(true);
}

void PcapWriter::closeFile(bool checkError)
{
    if (dumpfile == nullptr)
        return;
    auto file = dumpfile;
    dumpfile = nullptr;
    if (fclose(file) != 0 && checkError)
        throw cRuntimeError("Cannot close pcap file [%s]: %s", fileName.c_str(), strerror(errno));
}

} // namespace inet
