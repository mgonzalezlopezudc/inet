// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"

#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"

namespace inet { namespace lowpan {

Define_Module(LowpanIpv6NeighbourDiscovery);

void LowpanIpv6NeighbourDiscovery::start()
{
    Ipv6NeighbourDiscovery::start();
    for (int i = 0; i < ift->getNumInterfaces(); i++) {
        auto interface = ift->getInterface(i);
        if (interface->getProtocol() != &lowpanProtocol)
            continue;
        auto data = interface->getProtocolDataForUpdate<Ipv6InterfaceData>();
        for (int j = 0; j < data->getNumAddresses(); j++) {
            auto address = data->getAddress(j);
            if (data->isTentativeAddress(address))
                initiateDad(address, interface);
        }
    }
}

void LowpanIpv6NeighbourDiscovery::initiateDad(const Ipv6Address& tentativeAddr, NetworkInterface *ie)
{
    if (ie->getProtocol() != &lowpanProtocol) {
        Ipv6NeighbourDiscovery::initiateDad(tentativeAddr, ie);
        return;
    }
    auto data = ie->getProtocolDataForUpdate<Ipv6InterfaceData>();
    data->permanentlyAssign(tentativeAddr);
    data->setDadInProgress(false);
}

void LowpanIpv6NeighbourDiscovery::startRouterDiscovery(NetworkInterface *ie)
{
    if (ie->getProtocol() != &lowpanProtocol)
        Ipv6NeighbourDiscovery::startRouterDiscovery(ie);
}

void LowpanIpv6NeighbourDiscovery::createRaTimer(NetworkInterface *ie)
{
    if (ie->getProtocol() != &lowpanProtocol)
        Ipv6NeighbourDiscovery::createRaTimer(ie);
}

void LowpanIpv6NeighbourDiscovery::processNDMessage(Packet *packet, const Icmpv6Header *message)
{
    auto indication = packet->findTag<InterfaceInd>();
    auto interface = indication != nullptr ? ift->getInterfaceById(indication->getInterfaceId()) : nullptr;
    if (interface != nullptr && interface->getProtocol() == &lowpanProtocol) {
        LowpanPacketDropDetails details;
        details.setReason(OTHER_PACKET_DROP);
        details.setLowpanReason(LOWPAN_ND_UNSUPPORTED);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }
    Ipv6NeighbourDiscovery::processNDMessage(packet, message);
}

void LowpanIpv6NeighbourDiscovery::sendRedirect(Packet *redirectedPacket, const Ipv6Address& targetAddr,
        const Ipv6Address& destAddr, NetworkInterface *ie)
{
    // redirectedPacket is borrowed from IPv6, including on the suppressed path.
    if (ie->getProtocol() != &lowpanProtocol)
        Ipv6NeighbourDiscovery::sendRedirect(redirectedPacket, targetAddr, destAddr, ie);
}

} } // namespace inet::lowpan
