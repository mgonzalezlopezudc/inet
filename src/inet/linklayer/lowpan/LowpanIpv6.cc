// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIpv6.h"

#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/networklayer/icmpv6/Icmpv6Header_m.h"
#include "inet/networklayer/ipv6/Ipv6Header.h"
#include "inet/networklayer/ipv6/Ipv6ExtensionHeaders.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/NextHopAddressTag_m.h"

namespace inet { namespace lowpan {

Define_Module(LowpanIpv6);

bool LowpanIpv6::isNativeNdControl(const Packet *packet) const
{
    try {
        auto header = packet->peekAtFront<Ipv6Header>();
        auto protocol = header->getProtocolId();
        b offset = header->getChunkLength();
        while (isIpv6ExtensionHeader(protocol)) {
            auto extension = peekIpv6ExtensionHeaderAt(packet, offset, protocol);
            protocol = extension->getNextHeaderProtocol();
            offset += extension->getChunkLength();
        }
        if (protocol != IP_PROT_IPv6_ICMP)
            return false;
        int type = packet->peekDataAt<Icmpv6Header>(offset)->getType();
        return (type >= ICMPv6_ROUTER_SOL && type <= ICMPv6_REDIRECT) ||
            type == ICMPv6_DUPLICATE_ADDRESS_REQUEST || type == ICMPv6_DUPLICATE_ADDRESS_CONFIRMATION;
    }
    catch (const cRuntimeError&) {
        return false;
    }
}

void LowpanIpv6::resolveMACAddressAndSendPacket(Packet *packet, int interfaceId, Ipv6Address nextHop, bool fromHL)
{
    auto interface = ift->getInterfaceById(interfaceId);
    if (interface->getProtocol() != &lowpanProtocol) {
        Ipv6::resolveMACAddressAndSendPacket(packet, interfaceId, nextHop, fromHL);
        return;
    }
    auto nativeNd = dynamic_cast<LowpanIpv6NeighbourDiscovery *>(nd.get());
    auto ipv6 = packet->peekAtFront<Ipv6Header>();
    if (nativeNd && !nativeNd->isNativeSourceUsable(interface, ipv6->getSrcAddress())) {
        if (!isNativeNdControl(packet)) {
            PacketDropDetails details;
            details.setReason(ADDRESS_RESOLUTION_FAILED);
            emit(packetDroppedSignal, packet, &details);
            numDropped++;
            delete packet;
            return;
        }
    }
    packet->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
    numForwarded++;
    fragmentPostRouting(packet, interface, MacAddress::UNSPECIFIED_ADDRESS, fromHL);
}

void LowpanIpv6::localDeliverFinish(Packet *packet)
{
    auto nativeNd = dynamic_cast<LowpanIpv6NeighbourDiscovery *>(nd.get());
    auto destination = packet->peekAtFront<Ipv6Header>()->getDestAddress();
    if (nativeNd && nativeNd->usesNativeHostRegistration() && !destination.isMulticast() && !isNativeNdControl(packet)) {
        bool nativeInterface = false;
        bool usable = false;
        for (int i = 0; i < ift->getNumInterfaces(); i++) {
            auto interface = ift->getInterface(i);
            auto data = interface->findProtocolData<Ipv6InterfaceData>();
            if (!data)
                continue;
            if (dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary"))) {
                nativeInterface = true;
                usable |= nativeNd->isNativeSourceUsable(interface, destination);
            }
            else
                usable |= data->hasValidAddress(destination, simTime()) && !data->isTentativeAddress(destination);
        }
        if (nativeInterface && !usable) {
            PacketDropDetails details;
            details.setReason(ADDRESS_RESOLUTION_FAILED);
            emit(packetDroppedSignal, packet, &details);
            numDropped++;
            delete packet;
            return;
        }
    }
    Ipv6::localDeliverFinish(packet);
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
