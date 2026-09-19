// SPDX-License-Identifier: LGPL-3.0-or-later
// Standalone evidence adapter; links the separately installed ns-3 implementation.
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/sixlowpan-net-device.h"
#include "ns3/iana-ieee802-numbers.h"
#include "mock-net-device.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace ns3;
static int outputs = 0;
static void Write(Ptr<const Packet> packet) {
    std::vector<uint8_t> bytes(packet->GetSize());
    packet->CopyData(bytes.data(), bytes.size());
    for (auto byte : bytes) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    std::cout << '\n'; outputs++;
}
static bool Encoded(Ptr<NetDevice>, Ptr<const Packet> p, uint16_t, const Address&, const Address&, NetDevice::PacketType) { Write(p); return true; }
static bool Decoded(Ptr<NetDevice>, Ptr<const Packet> p, uint16_t, const Address&) { Write(p); return true; }
int main(int argc, char **argv) {
    if (argc != 3) { std::cerr << "usage: peer encode|decode hex-lines-file\n"; return 2; }
    bool encode = std::string(argv[1]) == "encode";
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));
    auto node = CreateObject<Node>();
    auto lower = CreateObject<MockNetDevice>();
    Mac64Address source("00:11:22:33:44:55:66:01"), destination("00:11:22:33:44:55:66:02");
    lower->SetAddress(encode ? source : destination);
    lower->SetMtu(104); node->AddDevice(lower);
    auto six = CreateObject<SixLowPanNetDevice>(); node->AddDevice(six);
    six->SetAttribute("CompressionType", EnumValue(SixLowPanNetDevice::IPHC));
    six->SetAttribute("OmitUdpChecksum", BooleanValue(false));
    six->SetAttribute("UseMeshUnder", BooleanValue(false));
    six->AssignStreams(1); six->SetNetDevice(lower);
    lower->SetSendCallback(MakeCallback(&Encoded)); six->SetReceiveCallback(MakeCallback(&Decoded));
    std::ifstream input(argv[2]); std::string line;
    if (!input) return 2;
    while (input >> line) {
        if (line.size() % 2) return 2;
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < line.size(); i += 2) bytes.push_back(std::stoul(line.substr(i, 2), nullptr, 16));
        auto packet = Create<Packet>(bytes.data(), bytes.size());
        if (encode) six->Send(packet, destination, 0x86dd);
        else Simulator::ScheduleWithContext(node->GetId(), Seconds(0), [=]() {
            lower->Receive(packet, iana::ieee802numbers::LoWPAN, destination, source, NetDevice::PACKET_HOST);
        });
    }
    Simulator::Stop(MilliSeconds(1)); Simulator::Run(); Simulator::Destroy();
    return outputs ? 0 : 1;
}
