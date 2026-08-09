//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/common/packet/recorder/PcapngWriter.h"

#include <algorithm>
#include <cerrno>
#include <limits>

#include "inet/common/INETUtils.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/chunk/BytesChunk.h"

namespace inet {

#define PCAP_MAGIC    0x1a2b3c4d

struct pcapng_option_header
{
    uint16_t code;
    uint16_t length;
};

struct pcapng_section_block_header
{
    uint32_t blockType = 0x0A0D0D0A;
    uint32_t blockTotalLength;
    uint32_t byteOrderMagic;
    uint16_t majorVersion;
    uint16_t minorVersion;
    uint64_t sectionLength;
};

struct pcapng_section_block_trailer
{
    uint32_t blockTotalLength;
};

struct pcapng_interface_block_header
{
    uint32_t blockType = 0x00000001;
    uint32_t blockTotalLength;
    uint16_t linkType;
    uint16_t reserved;
    uint32_t snaplen;
};

struct pcapng_interface_block_trailer
{
    uint32_t blockTotalLength;
};

struct pcapng_packet_block_header
{
    uint32_t blockType = 0x00000006;
    uint32_t blockTotalLength;
    uint32_t interfaceId;
    uint32_t timestampHigh;
    uint32_t timestampLow;
    uint32_t capturedPacketLength;
    uint32_t originalPacketLength;
};

struct pcapng_packet_block_trailer
{
    uint32_t blockTotalLength;
};

static size_t pad(size_t value, size_t multiplier = 4)
{
    return (multiplier - value % multiplier) % multiplier;
}

static uint64_t roundUp(uint64_t value, uint64_t multiplier = 4)
{
    return value + pad(value, multiplier);
}

static void writeBytes(FILE *file, const void *data, size_t length, const std::string& fileName)
{
    if (length != 0 && fwrite(data, length, 1, file) != 1)
        throw cRuntimeError("Cannot write pcapng file [%s]: %s", fileName.c_str(), strerror(errno));
}

PcapngWriter::~PcapngWriter()
{
    closeFile(false);
}

void PcapngWriter::open(const char *filename, unsigned int snaplen, int timePrecision)
{
    if (opp_isempty(filename))
        throw cRuntimeError("Cannot open pcap file: file name is empty");

    inet::utils::makePathForFile(filename);
    dumpfile = fopen(filename, "wb");
    fileName = filename;

    if (!dumpfile)
        throw cRuntimeError("Cannot open pcap file [%s] for writing: %s", filename, strerror(errno));

    flush = false;
    this->snaplen = snaplen;
    nextPcapngInterfaceId = 0;
    interfaceAndLinkTypeToPcapngInterfaceId.clear();

    // TODO check validity of timePrecision
    this->timePrecision = timePrecision;

    // header
    int blockTotalLength = 28;
    ASSERT(blockTotalLength % 4 == 0);
    struct pcapng_section_block_header sbh;
    sbh.blockTotalLength = blockTotalLength;
    sbh.byteOrderMagic = PCAP_MAGIC;
    sbh.majorVersion = 1;
    sbh.minorVersion = 0;
    sbh.sectionLength = -1L;
    writeBytes(dumpfile, &sbh, sizeof(sbh), fileName);

    // trailer
    struct pcapng_section_block_trailer sbt;
    sbt.blockTotalLength = blockTotalLength;
    writeBytes(dumpfile, &sbt, sizeof(sbt), fileName);
}

void PcapngWriter::writeInterface(NetworkInterface *networkInterface, PcapLinkType linkType)
{
    EV_INFO << "Writing interface to file" << EV_FIELD(fileName) << EV_FIELD(networkInterface) << EV_ENDL;
    if (!dumpfile)
        throw cRuntimeError("Cannot write interface: pcap output file is not open");

    std::string name = networkInterface->getInterfaceName();
    std::string fullPath = networkInterface->getInterfaceFullPath();
    fullPath = fullPath.substr(fullPath.find('.') + 1);
    uint32_t optionsLength = (4 + roundUp(name.length())) + (4 + roundUp(fullPath.length())) + (4 + 8) + (4 + 4 + 4) + (4 + 4) + 4;
    uint32_t blockTotalLength = 20 + optionsLength;
    ASSERT(blockTotalLength % 4 == 0);

    // header
    pcapng_interface_block_header ibh;
    ibh.blockTotalLength = blockTotalLength;
    ibh.linkType = linkType;
    ibh.reserved = 0;
    ibh.snaplen = snaplen;
    writeBytes(dumpfile, &ibh, sizeof(ibh), fileName);

    // interface name option
    pcapng_option_header doh;
    doh.code = 0x0002;
    doh.length = name.length();
    writeBytes(dumpfile, &doh, sizeof(doh), fileName);
    writeBytes(dumpfile, name.data(), name.length(), fileName);
    char padding[] = { 0, 0, 0, 0 };
    int paddingLength = pad(name.length());
    writeBytes(dumpfile, padding, paddingLength, fileName);

    // interface description option
    doh.code = 0x0003;
    doh.length = fullPath.length();
    writeBytes(dumpfile, &doh, sizeof(doh), fileName);
    writeBytes(dumpfile, fullPath.data(), fullPath.length(), fileName);
    paddingLength = pad(fullPath.length());
    writeBytes(dumpfile, padding, paddingLength, fileName);

    // MAC address option
    doh.code = 0x0006;
    doh.length = 6;
    writeBytes(dumpfile, &doh, sizeof(doh), fileName);
    uint8_t macAddressBytes[6];
    networkInterface->getMacAddress().getAddressBytes(macAddressBytes);
    writeBytes(dumpfile, macAddressBytes, 6, fileName);
    writeBytes(dumpfile, padding, 2, fileName);

    // IP address/netmask option
    doh.code = 0x0004;
    doh.length = 4 + 4;
    writeBytes(dumpfile, &doh, sizeof(doh), fileName);
    uint8_t ipAddressBytes[4];
    auto ipv4Address = networkInterface->getIpv4Address();
    for (int i = 0; i < 4; i++) ipAddressBytes[i] = ipv4Address.getDByte(i);
    writeBytes(dumpfile, ipAddressBytes, 4, fileName);
    auto ipv4Netmask = networkInterface->getIpv4Netmask();
    for (int i = 0; i < 4; i++) ipAddressBytes[i] = ipv4Netmask.getDByte(i);
    writeBytes(dumpfile, ipAddressBytes, 4, fileName);

    // tsresol option
    doh.code = 0x0009;
    doh.length = 1;
    writeBytes(dumpfile, &doh, sizeof(doh), fileName);
    uint8_t d = timePrecision;
    writeBytes(dumpfile, &d, 1, fileName);
    paddingLength = pad(1);
    writeBytes(dumpfile, padding, paddingLength, fileName);

    // end of options
    uint32_t endOfOptions = 0;
    writeBytes(dumpfile, &endOfOptions, sizeof(endOfOptions), fileName);

    // trailer
    pcapng_interface_block_trailer ibt;
    ibt.blockTotalLength = blockTotalLength;
    writeBytes(dumpfile, &ibt, sizeof(ibt), fileName);
}

void PcapngWriter::writePacket(simtime_t stime, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface, PcapLinkType linkType)
{
    writePacketWithPrefix(stime, packet, frontOffset, backOffset, direction, networkInterface, linkType, {});
}

void PcapngWriter::writePacketWithPrefix(simtime_t stime, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface, PcapLinkType linkType,
        const std::vector<uint8_t>& packetPrefix)
{
    EV_INFO << "Writing packet to file" << EV_FIELD(fileName) << EV_FIELD(packet) << EV_ENDL;
    if (!dumpfile)
        throw cRuntimeError("Cannot write frame: pcap output file is not open");
    if (networkInterface == nullptr)
        throw cRuntimeError("The interface entry not found for packet");

    auto interfaceKey = std::make_pair(networkInterface->getId(), linkType);
    auto it = interfaceAndLinkTypeToPcapngInterfaceId.find(interfaceKey);
    int pcapngInterfaceId;
    if (it != interfaceAndLinkTypeToPcapngInterfaceId.end())
        pcapngInterfaceId = it->second;
    else {
        writeInterface(networkInterface, linkType);
        pcapngInterfaceId = nextPcapngInterfaceId++;
        interfaceAndLinkTypeToPcapngInterfaceId[interfaceKey] = pcapngInterfaceId;
    }

    b packetDataLength = packet->getDataLength() - frontOffset - backOffset;
    auto packetDataLengthBytes = packetDataLength.get<B>();
    if (packetDataLengthBytes < 0 || packetPrefix.size() > std::numeric_limits<uint32_t>::max() ||
            static_cast<uint64_t>(packetDataLengthBytes) + packetPrefix.size() > std::numeric_limits<uint32_t>::max())
        throw cRuntimeError("Invalid original packet length");
    uint32_t originalPacketLength = static_cast<uint32_t>(packetDataLengthBytes + packetPrefix.size());
    uint32_t capturedPacketLength = snaplen == 0 ? originalPacketLength : std::min(originalPacketLength, snaplen);
    uint32_t optionsLength = (4 + 4) + 4;
    uint64_t blockTotalLength64 = 32 + roundUp(capturedPacketLength) + optionsLength;
    if (blockTotalLength64 > std::numeric_limits<uint32_t>::max())
        throw cRuntimeError("Captured packet is too large for a pcapng block");
    uint32_t blockTotalLength = static_cast<uint32_t>(blockTotalLength64);
    ASSERT(blockTotalLength % 4 == 0);

    // header
    struct pcapng_packet_block_header pbh;
    pbh.blockTotalLength = blockTotalLength;
    pbh.interfaceId = pcapngInterfaceId;
    ASSERT(stime >= SIMTIME_ZERO);
    uint64_t timestamp = stime.inUnit(static_cast<SimTimeUnit>(-timePrecision));
    pbh.timestampHigh = static_cast<uint32_t>((timestamp >> 32) & 0xFFFFFFFFLLU);
    pbh.timestampLow = static_cast<uint32_t>(timestamp & 0xFFFFFFFFLLU);
    pbh.capturedPacketLength = capturedPacketLength;
    pbh.originalPacketLength = originalPacketLength;
    writeBytes(dumpfile, &pbh, sizeof(pbh), fileName);

    auto capturedPrefixLength = std::min<size_t>(packetPrefix.size(), capturedPacketLength);
    writeBytes(dumpfile, packetPrefix.data(), capturedPrefixLength, fileName);
    auto capturedDataLength = capturedPacketLength - capturedPrefixLength;
    if (capturedDataLength != 0) {
        auto data = packet->peekDataAt<BytesChunk>(frontOffset, B(capturedDataLength));
        const auto& bytes = data->getBytes();
        writeBytes(dumpfile, bytes.data(), bytes.size(), fileName);
    }
    char padding[] = { 0, 0, 0, 0 };
    writeBytes(dumpfile, padding, pad(capturedPacketLength), fileName);

    // direction option
    pcapng_option_header doh;
    doh.code = 0x0002;
    doh.length = 4;
    uint32_t flagsOptionValue = 0;
    switch (direction) {
        case DIRECTION_INBOUND:
            flagsOptionValue = 0b01;
            break;
        case DIRECTION_OUTBOUND:
            flagsOptionValue = 0b10;
            break;
        default:
            throw cRuntimeError("Unknown direction value");
    }
    writeBytes(dumpfile, &doh, sizeof(doh), fileName);
    writeBytes(dumpfile, &flagsOptionValue, sizeof(flagsOptionValue), fileName);

    // end of options
    uint32_t endOfOptions = 0;
    writeBytes(dumpfile, &endOfOptions, sizeof(endOfOptions), fileName);

    // trailer
    struct pcapng_packet_block_trailer pbt;
    pbt.blockTotalLength = blockTotalLength;
    writeBytes(dumpfile, &pbt, sizeof(pbt), fileName);

    if (flush && fflush(dumpfile) != 0)
        throw cRuntimeError("Cannot flush pcapng file [%s]: %s", fileName.c_str(), strerror(errno));
}

void PcapngWriter::close()
{
    closeFile(true);
}

void PcapngWriter::closeFile(bool checkError)
{
    if (dumpfile == nullptr)
        return;
    auto file = dumpfile;
    dumpfile = nullptr;
    if (fclose(file) != 0 && checkError)
        throw cRuntimeError("Cannot close pcapng file [%s]: %s", fileName.c_str(), strerror(errno));
}

} // namespace inet
