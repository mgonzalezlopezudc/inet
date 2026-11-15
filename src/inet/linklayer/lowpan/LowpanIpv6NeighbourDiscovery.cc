// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"

#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6RoutingTable.h"

namespace inet { namespace lowpan {

Define_Module(LowpanIpv6NeighbourDiscovery);
simsignal_t LowpanIpv6NeighbourDiscovery::registrationCountChangedSignal = registerSignal("registrationCountChanged");

LowpanIpv6NeighbourDiscovery::~LowpanIpv6NeighbourDiscovery()
{
    cancelAndDelete(registrationExpiryTimer);
    registrationExpiryTimer = nullptr;
    clearRegistrations(false);
}

void LowpanIpv6NeighbourDiscovery::initialize(int stage)
{
    Ipv6NeighbourDiscovery::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        std::string role = par("nativeNdRole").stdstringValue();
        if (role != "static" && role != "router")
            throw cRuntimeError("Invalid native LoWPAN ND role");
        registrationRouter = role == "router";
        maxRegistrations = par("maxRegistrations");
        if (maxRegistrations <= 0)
            throw cRuntimeError("Native registration capacity must be positive");
        registrationExpiryTimer = new cMessage("registrationExpiryTimer");
        WATCH_EXPR("numNativeRegistrations", registrations.size());
    }
}

void LowpanIpv6NeighbourDiscovery::handleMessageWhenUp(cMessage *message)
{
    if (message == registrationExpiryTimer) {
        expireRegistrations();
        scheduleRegistrationExpiry();
    }
    else
        Ipv6NeighbourDiscovery::handleMessageWhenUp(message);
}

void LowpanIpv6NeighbourDiscovery::stop()
{
    clearRegistrations(true);
    Ipv6NeighbourDiscovery::stop();
}

void LowpanIpv6NeighbourDiscovery::removeRegistrationRoute(const RegistrationKey& key)
{
    if (!rt6)
        return;
    for (int i = rt6->getNumRoutes() - 1; i >= 0; i--) {
        auto route = rt6->getRoute(i);
        if (route->getSource() == this && route->getInterface() != nullptr &&
                route->getInterface()->getInterfaceId() == key.first && route->getDestPrefix() == key.second && route->getPrefixLength() == 128)
            rt6->deleteRoute(route);
    }
    rt6->purgeDestCacheEntriesToNeighbour(key.second, key.first);
}

void LowpanIpv6NeighbourDiscovery::removeRegistration(const RegistrationKey& key, bool notify)
{
    auto entry = registrations.find(key);
    if (entry == registrations.end())
        return;
    if (entry->second.domain)
        entry->second.domain->removeDynamicNeighbor(entry->second.interfaceModuleId, key.second, this);
    removeRegistrationRoute(key);
    registrations.erase(entry);
    if (notify)
        emit(registrationCountChangedSignal, long(registrations.size()));
}

void LowpanIpv6NeighbourDiscovery::clearRegistrations(bool notify)
{
    if (registrationExpiryTimer != nullptr)
        cancelEvent(registrationExpiryTimer);
    while (!registrations.empty())
        removeRegistration(registrations.begin()->first, notify);
}

void LowpanIpv6NeighbourDiscovery::expireRegistrations()
{
    for (auto it = registrations.begin(); it != registrations.end();) {
        auto current = it++;
        if (current->second.validUntil <= simTime())
            removeRegistration(current->first);
    }
}

void LowpanIpv6NeighbourDiscovery::scheduleRegistrationExpiry()
{
    cancelEvent(registrationExpiryTimer);
    if (registrations.empty())
        return;
    auto earliest = SIMTIME_MAX;
    for (const auto& entry : registrations)
        earliest = std::min(earliest, entry.second.validUntil);
    scheduleAt(earliest, registrationExpiryTimer);
}

bool LowpanIpv6NeighbourDiscovery::readRegistration(const Packet *packet, const Ipv6NeighbourSolicitation *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain, Registration& requested, const Ipv6NdOption *&aro) const
{
    // RFC 6775 section 6.5: registration is bound to the arriving interface.
    auto source = packet->findTag<L3AddressInd>();
    auto hop = packet->findTag<HopLimitInd>();
    auto native = packet->findTag<Ieee802154AddressInd>();
    const Ipv6NdOption *sllao = nullptr;
    aro = nullptr;
    int aroCount = 0, sllaoCount = 0;
    for (size_t i = 0; i < message->getOptions().getOptionArraySize(); i++) {
        auto option = message->getOptions().getOption(i);
        if (int(option->getType()) == 33) { aro = option; aroCount++; }
        if (option->getType() == IPv6ND_SOURCE_LINK_LAYER_ADDR_OPTION) { sllao = option; sllaoCount++; }
    }
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    bool valid = !message->isIncorrect() && !message->isIncomplete() && message->getCode() == 0 &&
        source != nullptr && source->getSrcAddress().getType() == L3Address::IPv6 &&
        source->getSrcAddress().toIpv6().isUnicast() && !source->getSrcAddress().toIpv6().isUnspecified() &&
        source->getDestAddress() == L3Address(message->getTargetAddress()) &&
        hop != nullptr && hop->getHopLimit() == 255 && native != nullptr &&
        data->hasAddress(message->getTargetAddress()) && !data->isTentativeAddress(message->getTargetAddress()) &&
        aroCount == 1 && sllaoCount == 1 && aro->getOptionLength() == 2 && aro->getPaddingBytesArraySize() == 14 &&
        sllao->getOptionLength() == 2 && sllao->getPaddingBytesArraySize() == 14 && uint8_t(aro->getPaddingBytes(0)) == 0 &&
        message->getChunkLength() == packet->getDataLength();
    uint64_t ownerValue = 0, nativeValue = 0;
    if (valid) {
        for (int i = 0; i < 8; i++) {
            ownerValue = (ownerValue << 8) | uint8_t(aro->getPaddingBytes(i + 6));
            nativeValue = (nativeValue << 8) | uint8_t(sllao->getPaddingBytes(i));
        }
        valid = native->getSrcAddress() == Ieee802154Address(Ieee802154Address::EXTENDED, nativeValue) &&
            domain->contains(native->getSrcAddress()) && domain->contains(Ieee802154Address(Ieee802154Address::EXTENDED, ownerValue)) &&
            native->getSrcPanId() == domain->getPanId() && native->getDestPanId() == domain->getPanId() && Icmpv6::verifyChecksum(packet);
    }
    if (!valid)
        return false;
    int lifetime = (uint8_t(aro->getPaddingBytes(4)) << 8) | uint8_t(aro->getPaddingBytes(5));
    requested = {Ieee802154Address(Ieee802154Address::EXTENDED, ownerValue), native->getSrcAddress(),
        domain, interface->getId(), simTime() + SimTime(60 * lifetime)};
    return true;
}

int LowpanIpv6NeighbourDiscovery::applyRegistration(const RegistrationKey& key, const Registration& requested)
{
    auto existing = registrations.find(key);
    auto fixed = requested.domain->resolveStaticNeighbor(requested.interfaceModuleId, key.second);
    int status = 0;
    if (rt6->isLocalAddress(key.second) || (existing != registrations.end() && existing->second.owner != requested.owner) ||
            (fixed != nullptr && *fixed != requested.address))
        status = 1;
    else if (requested.validUntil == simTime())
        removeRegistration(key);
    else if (existing == registrations.end() && int(registrations.size()) >= maxRegistrations)
        status = 2;
    else {
        if (!requested.domain->installDynamicNeighbor(requested.interfaceModuleId, key.second,
                requested.address, requested.validUntil, this))
            status = 1;
        else {
            bool created = existing == registrations.end();
            registrations[key] = requested;
            removeRegistrationRoute(key);
            auto route = new Ipv6Route(key.second, 128, IRoute::UNKNOWN);
            route->setSource(this);
            route->setInterface(ift->getInterfaceById(key.first));
            route->setExpiryTime(requested.validUntil);
            route->setMetric(0);
            route->setAdminDist(Ipv6Route::dDirectlyConnected);
            rt6->addRoutingProtocolRoute(route);
            if (created)
                emit(registrationCountChangedSignal, long(registrations.size()));
        }
    }
    return status;
}

void LowpanIpv6NeighbourDiscovery::sendRegistrationAdvertisement(const RegistrationKey& key,
        const Registration& requested, const Ipv6NdOption& aro, int status, const Ipv6Address& routerAddress)
{
    auto domain = requested.domain.get();
    auto interface = ift->getInterfaceById(key.first);
    auto reply = makeShared<Ipv6NeighbourAdvertisement>();
    reply->setRouterFlag(true);
    reply->setSolicitedFlag(true);
    reply->setOverrideFlag(true);
    reply->setTargetAddress(routerAddress);
    auto response = new Ipv6NdOption(aro);
    response->setPaddingBytes(0, status);
    for (int i = 1; i <= 3; i++)
        response->setPaddingBytes(i, 0);
    reply->getOptionsForUpdate().appendOption(response);
    reply->setChunkLength(B(40));
    auto output = new Packet("native-registration-advertisement");
    // Native frames carry literal bytes, so declared checksum markers cannot cross the link.
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, reply, output);
    output->insertAtFront(reply);
    auto request = output->addTag<Ieee802154AddressReq>();
    request->setSrcAddress(domain->getPeer(interface).address);
    request->setDestAddress(status == 0 ? requested.address : requested.owner);
    request->setSrcPanId(domain->getPanId());
    request->setDestPanId(domain->getPanId());
    uint64_t iid = requested.owner.getInt() ^ UINT64_C(0x0200000000000000);
    Ipv6Address destination = status == 0 ? key.second : Ipv6Address(0xfe800000, 0, iid >> 32, uint32_t(iid));
    sendPacketToIpv6Module(output, destination, routerAddress, key.first);
}

void LowpanIpv6NeighbourDiscovery::processRegistration(Packet *packet, const Ipv6NeighbourSolicitation *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    Registration requested;
    const Ipv6NdOption *aro = nullptr;
    if (!readRegistration(packet, message, interface, domain, requested, aro)) {
        LowpanPacketDropDetails details;
        details.setReason(OTHER_PACKET_DROP);
        details.setLowpanReason(LOWPAN_MALFORMED);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }
    expireRegistrations();
    RegistrationKey key{interface->getInterfaceId(), packet->getTag<L3AddressInd>()->getSrcAddress().toIpv6()};
    int status = applyRegistration(key, requested);
    scheduleRegistrationExpiry();
    sendRegistrationAdvertisement(key, requested, *aro, status, message->getTargetAddress());
    delete packet;
}

void LowpanIpv6NeighbourDiscovery::start()
{
    if (registrationRouter && !rt6->isRouter())
        throw cRuntimeError("Native ND router role requires IPv6 forwarding to be enabled");
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
    auto boundary = interface != nullptr ? dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) : nullptr;
    if (registrationRouter && boundary != nullptr) {
        if (auto solicitation = dynamic_cast<const Ipv6NeighbourSolicitation *>(message)) {
            processRegistration(packet, solicitation, interface, boundary->getLinkDomain());
            return;
        }
    }
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
