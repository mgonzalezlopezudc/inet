// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIpv6.h"

#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/networklayer/icmpv6/Icmpv6Header_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/NextHopAddressTag_m.h"

namespace inet { namespace lowpan {

Define_Module(LowpanIpv6);

void LowpanIpv6::handleReceivedIcmp(Packet *packet)
{
    constexpr int flags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE;
    if (packet->getDataLength() >= B(1)) {
        auto type = packet->peekDataAt<BytesChunk>(b(0), B(1), flags)->getBytes()[0];
        int minimumLength = 0;
        switch (type) {
            case ICMPv6_ROUTER_SOL: minimumLength = 8; break;
            case ICMPv6_ROUTER_AD: minimumLength = 16; break;
            case ICMPv6_NEIGHBOUR_SOL:
            case ICMPv6_NEIGHBOUR_AD: minimumLength = 24; break;
            case ICMPv6_REDIRECT: minimumLength = 40; break;
            case ICMPv6_DUPLICATE_ADDRESS_REQUEST:
            case ICMPv6_DUPLICATE_ADDRESS_CONFIRMATION: minimumLength = 32; break;
        }
        if (minimumLength != 0) {
            bool malformed = packet->getDataLength() < B(minimumLength);
            if (!malformed) {
                auto header = packet->peekAtFront<Icmpv6Header>(b(-1), flags);
                malformed = header->isIncorrect() || header->isIncomplete();
            }
            if (malformed) {
                // Malformed ND options and incomplete headers must not reach ordinary dispatch's checked peek.
                LowpanPacketDropDetails details;
                details.setReason(OTHER_PACKET_DROP);
                details.setLowpanReason(LOWPAN_MALFORMED);
                emit(packetDroppedSignal, packet, &details);
                numDropped++;
                delete packet;
                return;
            }
        }
    }
    Ipv6::handleReceivedIcmp(packet);
}

void LowpanIpv6::resolveMACAddressAndSendPacket(Packet *packet, int interfaceId, Ipv6Address nextHop, bool fromHL)
{
    auto interface = ift->getInterfaceById(interfaceId);
    if (interface->getProtocol() != &lowpanProtocol) {
        Ipv6::resolveMACAddressAndSendPacket(packet, interfaceId, nextHop, fromHL);
        return;
    }
    packet->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
    numForwarded++;
    fragmentPostRouting(packet, interface, MacAddress::UNSPECIFIED_ADDRESS, fromHL);
}

void LowpanIpv6::fragmentAndSend(Packet *packet)
{
    // Source fragmentation creates new packets without copying request tags.
    // Save context here, including reinjection after a queued POST_ROUTING hook.
    auto previousNextHop = fragmentNextHop;
    auto request = packet->findTag<NextHopAddressReq>();
    fragmentNextHop = request != nullptr ? request->getNextHopAddress().toIpv6() : Ipv6Address::UNSPECIFIED_ADDRESS;
    try {
        Ipv6::fragmentAndSend(packet);
    }
    catch (...) {
        fragmentNextHop = previousNextHop;
        throw;
    }
    fragmentNextHop = previousNextHop;
}

void LowpanIpv6::sendDatagramToOutput(Packet *packet, const NetworkInterface *destIE, const MacAddress& macAddr)
{
    if (destIE->getProtocol() != &lowpanProtocol) {
        Ipv6::sendDatagramToOutput(packet, destIE, macAddr);
        return;
    }
    packet->removeTagIfPresent<MacAddressReq>();
    if (!fragmentNextHop.isUnspecified())
        packet->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(fragmentNextHop);
    packet->addTagIfAbsent<InterfaceReq>()->setInterfaceId(destIE->getInterfaceId());
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv6);
    packet->addTagIfAbsent<DispatchProtocolInd>()->setProtocol(&Protocol::ipv6);
    // Each NIC is selected by InterfaceReq; a shared service registration would
    // collide when a router has more than one LoWPAN interface.
    packet->removeTagIfPresent<DispatchProtocolReq>();
    send(packet, "queueOut");
}

} } // namespace inet::lowpan
