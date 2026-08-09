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

static uint64_t encodeTimestamp(simtime_t time, int precision)
{
    auto raw = time.raw();
    if (raw < 0)
        throw cRuntimeError("Cannot write packet with a negative timestamp");
    unsigned __int128 value = static_cast<uint64_t>(raw);
    int exponent = SimTime::getScaleExp() + precision;
    if (exponent >= 0) {
        for (int i = 0; i < exponent; i++)
            value *= 10;
    }
    else {
        for (int i = 0; i < -exponent; i++)
            value /= 10;
    }
    if (value > std::numeric_limits<uint64_t>::max())
        throw cRuntimeError("Packet timestamp cannot be represented at PCAPng time precision %d", precision);
    return static_cast<uint64_t>(value);
}

void PcapngWriter::writeBytes(const void *data, size_t length)
{
    if (length != 0 && writeFile(data, length, 1, dumpfile) != 1)
        throw cRuntimeError("Cannot write pcapng file [%s]: %s", fileName.c_str(), strerror(errno));
}

PcapInterfaceDescriptor makePcapInterfaceDescriptor(NetworkInterface *networkInterface)
{
    if (networkInterface == nullptr)
        return {};
    PcapInterfaceDescriptor interfaceDescriptor;
    interfaceDescriptor.interfaceId = networkInterface->getId();
    interfaceDescriptor.name = networkInterface->getInterfaceName();
    interfaceDescriptor.description = networkInterface->getInterfaceFullPath();
    auto separatorPosition = interfaceDescriptor.description.find('.');
    if (separatorPosition != std::string::npos)
        interfaceDescriptor.description = interfaceDescriptor.description.substr(separatorPosition + 1);
    networkInterface->getMacAddress().getAddressBytes(interfaceDescriptor.macAddress.data());
    auto ipv4Address = networkInterface->getIpv4Address();
    auto ipv4Netmask = networkInterface->getIpv4Netmask();
    for (size_t i = 0; i < 4; i++) {
        interfaceDescriptor.ipv4Address[i] = ipv4Address.getDByte(i);
        interfaceDescriptor.ipv4Netmask[i] = ipv4Netmask.getDByte(i);
    }
    return interfaceDescriptor;
}

PcapngWriter::~PcapngWriter()
{
    closeFile(false);
}

void PcapngWriter::open(const char *filename, unsigned int snaplen, int timePrecision)
{
    if (dumpfile != nullptr)
        throw cRuntimeError("Cannot open pcapng file: another file is already open");
    if (opp_isempty(filename))
        throw cRuntimeError("Cannot open pcap file: file name is empty");
    if (timePrecision < 0 || timePrecision > 18)
        throw cRuntimeError("Unsupported time precision (%d) in PcapngWriter", timePrecision);

    inet::utils::makePathForFile(filename);
    fileName = filename;
    dumpfile = fopen(filename, "wb");

    if (!dumpfile)
        throw cRuntimeError("Cannot open pcap file [%s] for writing: %s", filename, strerror(errno));
    if (bufferSize != 0) {
        stdioBuffer.resize(bufferSize);
        if (setFileBuffer(dumpfile, stdioBuffer.data(), _IOFBF, stdioBuffer.size()) != 0) {
            closeFile(false);
            throw cRuntimeError("Cannot configure stdio buffer for pcapng file [%s]", filename);
        }
    }

    this->snaplen = snaplen;
    nextPcapngInterfaceId = 0;
    numRecordsWritten = 0;
    numPayloadBytesWritten = 0;
    numFlushes = 0;
    interfaces.clear();
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
    writeBytes(&sbh, sizeof(sbh));

    // trailer
    struct pcapng_section_block_trailer sbt;
    sbt.blockTotalLength = blockTotalLength;
    writeBytes(&sbt, sizeof(sbt));
}

void PcapngWriter::writeInterface(const PcapInterfaceDescriptor& interfaceDescriptor, PcapLinkType linkType)
{
    EV_INFO << "Writing interface to file" << EV_FIELD(fileName) << EV_ENDL;
    if (!dumpfile)
        throw cRuntimeError("Cannot write interface: pcap output file is not open");

    const auto& name = interfaceDescriptor.name;
    const auto& fullPath = interfaceDescriptor.description;
    if (name.size() > std::numeric_limits<uint16_t>::max() || fullPath.size() > std::numeric_limits<uint16_t>::max())
        throw cRuntimeError("PCAPng interface name or description is too long");
    uint32_t optionsLength = (4 + roundUp(name.length())) + (4 + roundUp(fullPath.length())) + (4 + 8) + (4 + 4 + 4) + (4 + 4) + 4;
    uint32_t blockTotalLength = 20 + optionsLength;
    ASSERT(blockTotalLength % 4 == 0);

    // header
    pcapng_interface_block_header ibh;
    ibh.blockTotalLength = blockTotalLength;
    ibh.linkType = linkType;
    ibh.reserved = 0;
    ibh.snaplen = snaplen;
    writeBytes(&ibh, sizeof(ibh));

    // interface name option
    pcapng_option_header doh;
    doh.code = 0x0002;
    doh.length = name.length();
    writeBytes(&doh, sizeof(doh));
    writeBytes(name.data(), name.length());
    char padding[] = { 0, 0, 0, 0 };
    int paddingLength = pad(name.length());
    writeBytes(padding, paddingLength);

    // interface description option
    doh.code = 0x0003;
    doh.length = fullPath.length();
    writeBytes(&doh, sizeof(doh));
    writeBytes(fullPath.data(), fullPath.length());
    paddingLength = pad(fullPath.length());
    writeBytes(padding, paddingLength);

    // MAC address option
    doh.code = 0x0006;
    doh.length = 6;
    writeBytes(&doh, sizeof(doh));
    writeBytes(interfaceDescriptor.macAddress.data(), interfaceDescriptor.macAddress.size());
    writeBytes(padding, 2);

    // IP address/netmask option
    doh.code = 0x0004;
    doh.length = 4 + 4;
    writeBytes(&doh, sizeof(doh));
    writeBytes(interfaceDescriptor.ipv4Address.data(), interfaceDescriptor.ipv4Address.size());
    writeBytes(interfaceDescriptor.ipv4Netmask.data(), interfaceDescriptor.ipv4Netmask.size());

    // tsresol option
    doh.code = 0x0009;
    doh.length = 1;
    writeBytes(&doh, sizeof(doh));
    uint8_t d = timePrecision;
    writeBytes(&d, 1);
    paddingLength = pad(1);
    writeBytes(padding, paddingLength);

    // end of options
    uint32_t endOfOptions = 0;
    writeBytes(&endOfOptions, sizeof(endOfOptions));

    // trailer
    pcapng_interface_block_trailer ibt;
    ibt.blockTotalLength = blockTotalLength;
    writeBytes(&ibt, sizeof(ibt));
}

void PcapngWriter::writePacket(simtime_t stime, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface, PcapLinkType linkType)
{
    if (networkInterface == nullptr)
        throw cRuntimeError("The interface entry not found for packet");
    writePacketWithPrefix(stime, packet, frontOffset, backOffset, direction, makePcapInterfaceDescriptor(networkInterface), linkType, {});
}

void PcapngWriter::writePacketWithPrefix(simtime_t stime, const Packet *packet, b frontOffset, b backOffset, Direction direction, const PcapInterfaceDescriptor& interfaceDescriptor, PcapLinkType linkType,
        const std::vector<uint8_t>& packetPrefix)
{
    EV_INFO << "Writing packet to file" << EV_FIELD(fileName) << EV_FIELD(packet) << EV_ENDL;
    if (!dumpfile)
        throw cRuntimeError("Cannot write frame: pcap output file is not open");
    auto timestamp = encodeTimestamp(stime, timePrecision);
    if (direction != DIRECTION_INBOUND && direction != DIRECTION_OUTBOUND)
        throw cRuntimeError("Unknown direction value");
    if (interfaceDescriptor.interfaceId < 0)
        throw cRuntimeError("Invalid PCAP interface descriptor");

    int pcapngInterfaceId;
    auto it = std::find_if(interfaces.begin(), interfaces.end(), [&](const InterfaceEntry& entry) {
        return entry.linkType == linkType && entry.descriptor == interfaceDescriptor;
    });
    if (it != interfaces.end())
        pcapngInterfaceId = it->pcapngInterfaceId;
    else {
        writeInterface(interfaceDescriptor, linkType);
        pcapngInterfaceId = nextPcapngInterfaceId++;
        interfaces.push_back({interfaceDescriptor, linkType, pcapngInterfaceId});
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
    pbh.timestampHigh = static_cast<uint32_t>((timestamp >> 32) & 0xFFFFFFFFLLU);
    pbh.timestampLow = static_cast<uint32_t>(timestamp & 0xFFFFFFFFLLU);
    pbh.capturedPacketLength = capturedPacketLength;
    pbh.originalPacketLength = originalPacketLength;
    writeBytes(&pbh, sizeof(pbh));

    auto capturedPrefixLength = std::min<size_t>(packetPrefix.size(), capturedPacketLength);
    writeBytes(packetPrefix.data(), capturedPrefixLength);
    auto capturedDataLength = capturedPacketLength - capturedPrefixLength;
    if (capturedDataLength != 0) {
        auto data = packet->peekDataAt<BytesChunk>(frontOffset, B(capturedDataLength));
        const auto& bytes = data->getBytes();
        writeBytes(bytes.data(), bytes.size());
    }
    char padding[] = { 0, 0, 0, 0 };
    writeBytes(padding, pad(capturedPacketLength));

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
        default: ASSERT(false);
    }
    writeBytes(&doh, sizeof(doh));
    writeBytes(&flagsOptionValue, sizeof(flagsOptionValue));

    // end of options
    uint32_t endOfOptions = 0;
    writeBytes(&endOfOptions, sizeof(endOfOptions));

    // trailer
    struct pcapng_packet_block_trailer pbt;
    pbt.blockTotalLength = blockTotalLength;
    writeBytes(&pbt, sizeof(pbt));

    numPayloadBytesWritten += capturedPacketLength;
    numRecordsWritten++;
    if (flush || (flushInterval != 0 && numRecordsWritten % flushInterval == 0)) {
        if (flushFile(dumpfile) != 0)
            throw cRuntimeError("Cannot flush pcapng file [%s]: %s", fileName.c_str(), strerror(errno));
        numFlushes++;
    }
}

void PcapngWriter::setBufferSize(size_t bufferSize)
{
    if (dumpfile != nullptr)
        throw cRuntimeError("Cannot change pcapng stdio buffer size while the file is open");
    this->bufferSize = bufferSize;
    stdioBuffer.clear();
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
    auto result = closeFileHandle(file);
    stdioBuffer.clear();
    if (result != 0 && checkError)
        throw cRuntimeError("Cannot close pcapng file [%s]: %s", fileName.c_str(), strerror(errno));
}

} // namespace inet
