// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_TEST_IEEE802154_RECEIVECASES_H
#define __INET_TEST_IEEE802154_RECEIVECASES_H

#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameCodec.h"
#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"

namespace inet {
namespace test802154 {

struct ReceiveCase {
    const char *name;
    bool ack;
    bool data;
    bool monitor;
    int security; // 0 none, 1 legacy, 2 unsupported security.
};

inline const ReceiveCase receiveCases[] = {
    {"local unicast", true, true, false, 0},
    {"foreign PAN", false, false, false, 0},
    {"foreign high EUI bits", false, false, false, 0},
    {"broadcast PAN local unicast", true, true, false, 0},
    {"bad FCS", false, false, false, 0},
    {"ignored reserved bit 7", true, true, false, 0},
    {"legacy sequence suppression", false, false, false, 0},
    {"legacy IE flag", false, false, false, 0},
    {"reserved address mode", false, false, false, 0},
    {"unsupported version 2", false, false, false, 0},
    {"secured legacy version 0", true, false, false, 1},
    {"secured legacy version 1", true, false, false, 2},
    {"foreign promiscuous", false, false, true, 0},
    {"local promiscuous", true, false, true, 0},
    {"bad FCS promiscuous", false, false, false, 0},
    {"group reception enabled", true, true, false, 0},
    {"group reception disabled", false, false, false, 0},
    {"broadcast address with injected AR", false, true, false, 0},
    {"source only PAN coordinator", true, true, false, 0},
    {"source only ordinary device", false, false, false, 0},
    {"implicit broadcast source only", false, true, false, 0},
    {"no addressing without implicit broadcast", false, false, false, 0},
    {"truncated common header", false, false, false, 0},
    {"implicit broadcast without addresses", false, true, false, 0},
    {"command is not MCPS data", false, false, false, 0},
    {"beacon is not MCPS data", false, false, false, 0},
    {"command promiscuous monitor", false, false, true, 0},
    {"reserved frame type", false, false, false, 0},
    {"reserved frame version", false, false, false, 0},
    {"reserved source address mode", false, false, false, 0},
    {"local short destination", true, true, false, 0},
    {"short broadcast with injected AR", false, true, false, 0},
    {"foreign source PAN at PAN coordinator", false, false, false, 0},
};

inline void mutateReceiveCase(Packet *packet, int scenario)
{
    auto chunk = packet->peekDataAsBytes();
    std::vector<uint8_t> bytes = chunk->getBytes();
    ASSERT(bytes.size() == 27);
    bytes.resize(bytes.size() - 2);
    uint16_t control = uint16_t(bytes[0]) | uint16_t(bytes[1]) << 8;
    if (scenario == 1) { bytes[3] = 0x78; bytes[4] = 0x56; }
    if (scenario == 2 || scenario == 12) bytes[12] = 0x06;
    if (scenario == 3) { bytes[3] = 0xff; bytes[4] = 0xff; }
    if (scenario == 5) control |= 0x80;
    if (scenario == 6) control |= 0x100;
    if (scenario == 7) control |= 0x200;
    if (scenario == 8) control = (control & ~0xc00) | 0x400;
    if (scenario == 9) control = (control & ~0x3000) | 0x2000;
    if (scenario == 10) control = (control & ~0x3000) | 8;
    if (scenario == 11) control |= 8;
    if (scenario == 15 || scenario == 16) bytes[12] = 0x05;
    if (scenario == 17) std::fill(bytes.begin() + 5, bytes.begin() + 13, 0xff);
    if ((scenario >= 18 && scenario <= 20) || scenario == 32) {
        std::vector<uint8_t> source(bytes.begin() + 13, bytes.end());
        bytes.resize(3);
        bytes.push_back(0x34); bytes.push_back(0x12);
        bytes.insert(bytes.end(), source.begin(), source.end());
        control = 0xd021; // Source extended only, explicit source PAN, AR set.
        if (scenario == 32) { bytes[3] = 0x78; bytes[4] = 0x56; }
    }
    if (scenario == 21 || scenario == 23) {
        bytes = {0x21, 0x10, 7, 0xde, 0xad, 0xbe, 0xef};
        control = 0x1021;
    }
    if (scenario == 24 || scenario == 26) {
        control = (control & ~7) | 3;
        bytes.resize(22);
        bytes[21] = 4; // A complete Data Request command; management is not enabled in this fixture.
    }
    if (scenario == 25) {
        bytes = {0, 0xd0, 7, 0x34, 0x12, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 1, 2, 0xff, 0xcf, 0, 0};
        control = 0xd000; // Version 1 beacon, extended source-only MHR.
    }
    if (scenario == 27) control = (control & ~7) | 4;
    if (scenario == 28) control = (control & ~0x3000) | 0x3000;
    if (scenario == 29) control = (control & ~0xc000) | 0x4000;
    if (scenario == 30 || scenario == 31) {
        std::vector<uint8_t> sourceAndPayload(bytes.begin() + 13, bytes.end());
        bytes.resize(5);
        bytes.push_back(scenario == 30 ? 0x42 : 0xff);
        bytes.push_back(scenario == 30 ? 0 : 0xff);
        bytes.insert(bytes.end(), sourceAndPayload.begin(), sourceAndPayload.end());
        control = (control & ~0xc00) | 0x800;
    }
    bytes[0] = control;
    bytes[1] = control >> 8;
    if (scenario == 22) bytes.resize(4);
    auto crc = Ieee802154FrameCodec::calculateFcs(bytes);
    bytes.push_back(crc); bytes.push_back(crc >> 8);
    if (scenario == 4 || scenario == 14) bytes.back() ^= 1;
    packet->eraseAll();
    packet->insertAtBack(makeShared<BytesChunk>(bytes));
}

} // namespace test802154
} // namespace inet
#endif
