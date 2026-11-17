// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanHostRegistration.h"
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6RoutingTable.h"

namespace inet { namespace lowpan {
namespace {
simtime_t finiteDeadline(uint32_t seconds)
{
    auto horizon = SIMTIME_MAX - simTime();
    return double(seconds) >= horizon.dbl() ? SIMTIME_MAX : simTime() + SimTime(seconds);
}

simtime_t prefixDeadline(uint32_t seconds)
{
    // A wire lifetime beyond the simulation clock's horizon cannot expire during this simulation.
    return seconds == UINT32_MAX || double(seconds) > (SIMTIME_MAX - simTime()).dbl() ?
        SIMTIME_ZERO : finiteDeadline(seconds);
}
}

LowpanHostRegistration::LowpanHostRegistration(LowpanIpv6NeighbourDiscovery& owner) : owner(owner)
{
    maxRouters = owner.par("maxCandidateRouters");
    maxAddresses = owner.par("maxHostAddresses");
    registrationLifetime = owner.par("nativeRegistrationLifetime");
    retryInterval = owner.par("nativeRegistrationRetryInterval");
    refreshFraction = owner.par("nativeRegistrationRefreshFraction");
    if (maxRouters <= 0 || maxAddresses <= 0 || registrationLifetime < 1 || registrationLifetime > 65535 ||
            retryInterval < SimTime(1) || refreshFraction <= 0 || refreshFraction >= 1)
        throw cRuntimeError("Invalid native host registration limits, lifetime, retry interval or refresh fraction");
    hostTimer = new cMessage("nativeHostTimer");
}

LowpanHostRegistration::~LowpanHostRegistration()
{
    clear();
    owner.cancelAndDelete(hostTimer);
}

void LowpanHostRegistration::drop(Packet *packet, LowpanDropReason reason)
{
    LowpanPacketDropDetails details;
    details.setReason(OTHER_PACKET_DROP);
    details.setLowpanReason(reason);
    owner.emit(packetDroppedSignal, packet, &details);
    delete packet;
}

void LowpanHostRegistration::startDiscovery(NetworkInterface *interface, bool backoff)
{
    auto& discovery = discoveries[interface->getInterfaceId()];
    if (backoff) {
        discovery.blockedUntil = simTime() + SimTime(10);
        discovery.active = false;
    }
    if (!discovery.active) {
        discovery.active = true;
        discovery.attempts = 0;
        discovery.next = std::max({simTime(), discovery.blockedUntil, discovery.lastSent + SimTime(10)});
        if (discovery.next <= simTime())
            sendRouterSolicitation(interface, discovery);
    }
    scheduleTimer();
}

void LowpanHostRegistration::sendRouterSolicitation(NetworkInterface *interface, Discovery& discovery)
{
    auto boundary = dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary"));
    auto source = interface->getProtocolData<Ipv6InterfaceData>()->getLinkLocalAddress();
    if (!boundary || source.isUnspecified() || !interface->isUp()) {
        discovery.active = false;
        discovery.next = SIMTIME_MAX;
        return;
    }
    auto message = makeShared<Ipv6RouterSolicitation>();
    message->getOptionsForUpdate().appendOption(owner.createNativeLinkOption(boundary->getLinkAddress()));
    message->addChunkLength(B(16));
    auto packet = new Packet("native-router-solicitation");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, message, packet);
    packet->insertAtFront(message);
    owner.sendPacketToIpv6Module(packet, Ipv6Address::ALL_ROUTERS_2, source, interface->getInterfaceId());
    discovery.lastSent = simTime();
    discovery.attempts++;
    // RFC 6775 section 5.3: three initial ten-second intervals, then bounded backoff.
    int interval = discovery.attempts < 3 ? 10 : discovery.attempts == 3 ? 20 : discovery.attempts == 4 ? 40 : 60;
    discovery.next = simTime() + SimTime(interval);
}

void LowpanHostRegistration::removeRouterRoute(const RouterKey& key)
{
    if (!owner.rt6)
        return;
    for (int i = owner.rt6->getNumRoutes() - 1; i >= 0; i--) {
        auto route = owner.rt6->getRoute(i);
        if (route->getSource() == &owner && route->getPrefixLength() == 0 && route->getNextHop() == key.second &&
                route->getInterface() && route->getInterface()->getInterfaceId() == key.first)
            owner.rt6->deleteRoute(route);
    }
    owner.rt6->purgeDestCacheEntriesToNeighbour(key.second, key.first);
}

void LowpanHostRegistration::removeRouter(const RouterKey& key, bool rediscover)
{
    auto it = routers.find(key);
    if (it == routers.end())
        return;
    if (it->second.domain)
        it->second.domain->removeDynamicNeighbor(it->second.interfaceModuleId, key.second, &owner);
    removeRouterRoute(key);
    routers.erase(it);
    if (rediscover && owner.ift)
        if (auto interface = owner.ift->getInterfaceById(key.first))
            startDiscovery(interface, true);
}

void LowpanHostRegistration::clear()
{
    owner.cancelEvent(hostTimer);
    while (!routers.empty())
        removeRouter(routers.begin()->first, false);
    if (owner.ift)
        for (const auto& entry : addresses)
            if (entry.second.autonomous)
                if (auto interface = owner.ift->getInterfaceById(entry.first.first)) {
                    auto data = interface->findProtocolDataForUpdate<Ipv6InterfaceData>();
                    if (data && data->hasAddress(entry.first.second))
                        data->removeAddress(entry.first.second);
                }
    addresses.clear();
    discoveries.clear();
}

void LowpanHostRegistration::collectAddresses(NetworkInterface *interface)
{
    auto data = interface->getProtocolDataForUpdate<Ipv6InterfaceData>();
    std::vector<std::pair<Ipv6Address, simtime_t>> configured;
    for (int i = 0; i < data->getNumAddresses(); i++)
        configured.emplace_back(data->getAddress(i), data->getAddressExpiryTime(i));
    for (const auto& entry : configured) {
        auto address = entry.first;
        if (!address.isUnicast() || address.isUnspecified() || !data->hasValidAddress(address, simTime()))
            continue;
        auto existing = addresses.find({interface->getInterfaceId(), address});
        if (existing == addresses.end()) {
            if (int(addresses.size()) >= maxAddresses)
                continue;
            addresses[{interface->getInterfaceId(), address}] = {false, false, entry.second};
        }
        else if (!existing->second.autonomous)
            existing->second.validUntil = entry.second;
        if (data->isTentativeAddress(address))
            data->permanentlyAssign(address); // Registration determines data usability on a native host.
    }
}

void LowpanHostRegistration::processPrefixes(NetworkInterface *interface, const Ipv6NdOptions& options)
{
    updatingPrefixes = true;
    auto data = interface->getProtocolDataForUpdate<Ipv6InterfaceData>();
    auto linkLocal = data->getLinkLocalAddress();
    for (size_t i = 0; i < options.getOptionArraySize(); i++) {
        auto prefix = dynamic_cast<const Ipv6NdPrefixInformation *>(options.getOption(i));
        if (!prefix || prefix->getOnlinkFlag() || !prefix->getAutoAddressConfFlag() || prefix->getPrefixLength() != 64 ||
                prefix->getPrefix().isMulticast() || prefix->getPrefix().isLinkLocal() ||
                prefix->getPreferredLifetime() > prefix->getValidLifetime() || linkLocal.isUnspecified())
            continue;
        Ipv6Address address(prefix->getPrefix().words()[0], prefix->getPrefix().words()[1],
            linkLocal.words()[2], linkLocal.words()[3]);
        AddressKey key{interface->getInterfaceId(), address};
        auto existing = addresses.find(key);
        if (existing != addresses.end() && (existing->second.rejected || !existing->second.autonomous))
            continue;
        if (existing == addresses.end() && (data->hasAddress(address) || int(addresses.size()) >= maxAddresses ||
                prefix->getValidLifetime() == 0))
            continue;
        auto validUntil = prefixDeadline(prefix->getValidLifetime());
        if (existing != addresses.end()) {
            auto remaining = existing->second.validUntil == SIMTIME_ZERO ? SIMTIME_MAX :
                std::max(SIMTIME_ZERO, existing->second.validUntil - simTime());
            // RFC 4862 section 5.5.3(e): unauthenticated PIO valid-lifetime reduction.
            if (prefix->getValidLifetime() <= 7200 && SimTime(prefix->getValidLifetime()) <= remaining)
                validUntil = remaining <= SimTime(7200) ? existing->second.validUntil : finiteDeadline(7200);
        }
        auto preferredUntil = prefixDeadline(prefix->getPreferredLifetime());
        if (prefix->getPreferredLifetime() == 0 && preferredUntil == SIMTIME_ZERO)
            preferredUntil = SimTime::fromRaw(-1); // Zero in Ipv6InterfaceData denotes infinity, not an expired deadline.
        addresses[key] = {true, false, validUntil};
        if (data->hasAddress(address))
            data->updateMatchingAddressExpiryTimes(address, 128, validUntil, preferredUntil);
        else
            data->assignAddress(address, false, validUntil, preferredUntil);
    }
    updatingPrefixes = false;
}

void LowpanHostRegistration::addressAssigned(NetworkInterface *interface)
{
    if (updatingPrefixes)
        return;
    collectAddresses(interface);
    bool knownRouter = false;
    for (const auto& router : routers)
        if (router.first.first == interface->getInterfaceId() && router.second.validUntil > simTime()) {
            knownRouter = true;
            startNextRegistration(router.first);
        }
    if (!knownRouter)
        startDiscovery(interface);
    scheduleTimer();
}

void LowpanHostRegistration::processAdvertisement(Packet *packet, const Ipv6RouterAdvertisement *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    Ieee802154Address native;
    auto source = packet->findTag<L3AddressInd>();
    if (message->isIncorrect() || message->isIncomplete() || message->getCode() != 0 ||
            message->getChunkLength() != packet->getDataLength() || !source ||
            !source->getSrcAddress().toIpv6().isLinkLocal() ||
            !owner.readNativeSource(packet, message->getOptions(), domain, native)) {
        drop(packet);
        return;
    }
    if (message->getManagedAddrConfFlag()) {
        drop(packet, LOWPAN_ND_UNSUPPORTED); // Selected native host profile uses M=0 address assignment.
        return;
    }
    auto& discovery = discoveries[interface->getInterfaceId()];
    if (discovery.blockedUntil > simTime()) {
        delete packet;
        return;
    }
    RouterKey key{interface->getInterfaceId(), source->getSrcAddress().toIpv6()};
    if (routers.count(key) == 0 && int(routers.size()) >= maxRouters) {
        drop(packet, LOWPAN_RESOURCE_LIMIT);
        return;
    }
    expireAddresses();
    collectAddresses(interface);
    processPrefixes(interface, message->getOptions());
    discovery.active = false;
    discovery.next = SIMTIME_MAX;
    if (message->getRouterLifetime() == 0) {
        removeRouter(key, false);
        bool knownRouter = false;
        for (const auto& router : routers)
            knownRouter |= router.first.first == interface->getInterfaceId() && router.second.validUntil > simTime();
        if (!knownRouter) startDiscovery(interface, true);
        delete packet;
        scheduleTimer();
        return;
    }
    auto until = finiteDeadline(message->getRouterLifetime());
    if (!domain->installDynamicNeighbor(interface->getId(), key.second, native, until, &owner)) {
        drop(packet, LOWPAN_ND_UNSUPPORTED);
        startDiscovery(interface, true);
        return;
    }
    bool created = routers.count(key) == 0;
    auto& router = routers[key];
    router.address = native;
    router.domain = domain;
    router.interfaceModuleId = interface->getId();
    router.validUntil = until;
    simtime_t informationLifetime(message->getRouterLifetime());
    for (size_t i = 0; i < message->getOptions().getOptionArraySize(); i++)
        if (auto prefix = dynamic_cast<const Ipv6NdPrefixInformation *>(message->getOptions().getOption(i)))
            if (!prefix->getOnlinkFlag() && prefix->getValidLifetime() != 0 && prefix->getValidLifetime() != UINT32_MAX &&
                    double(prefix->getValidLifetime()) < informationLifetime.dbl())
                informationLifetime = std::min(informationLifetime, SimTime(prefix->getValidLifetime()));
    router.informationRefreshAt = simTime() + informationLifetime * refreshFraction;
    removeRouterRoute(key);
    auto route = new Ipv6Route(Ipv6Address::UNSPECIFIED_ADDRESS, 0, IRoute::ROUTER_ADVERTISEMENT);
    route->setSource(&owner);
    route->setInterface(interface);
    route->setNextHop(key.second);
    route->setExpiryTime(until);
    route->setMetric(1);
    owner.rt6->addRoutingProtocolRoute(route);
    if (message->getCurHopLimit() != 0)
        interface->getProtocolDataForUpdate<Ipv6InterfaceData>()->setCurHopLimit(message->getCurHopLimit());
    if (created)
        owner.emit(owner.nativeRouterDiscoveredSignal, packet);
    startNextRegistration(key);
    delete packet;
    scheduleTimer();
}

void LowpanHostRegistration::sendRegistration(const RouterKey& key, const Ipv6Address& address, int lifetime)
{
    auto it = routers.find(key);
    auto interface = owner.ift->getInterfaceById(key.first);
    if (it == routers.end() || !interface || !it->second.domain)
        return;
    auto& router = it->second;
    auto native = router.domain->getPeer(interface).address;
    auto message = makeShared<Ipv6NeighbourSolicitation>();
    message->setTargetAddress(key.second);
    message->getOptionsForUpdate().appendOption(owner.createNativeLinkOption(native));
    auto aro = new Ipv6NdOption();
    aro->setType(static_cast<Ipv6NdOptionTypes>(33));
    aro->setOptionLength(2);
    aro->setPaddingBytesArraySize(14);
    for (int i = 0; i < 14; i++) aro->setPaddingBytes(i, 0);
    aro->setPaddingBytes(4, lifetime >> 8);
    aro->setPaddingBytes(5, lifetime);
    for (int i = 0; i < 8; i++) aro->setPaddingBytes(i + 6, native.getInt() >> (8 * (7 - i)));
    message->getOptionsForUpdate().appendOption(aro);
    message->addChunkLength(B(32));
    auto packet = new Packet("native-host-registration");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, message, packet);
    packet->insertAtFront(message);
    auto request = packet->addTag<Ieee802154AddressReq>();
    request->setSrcAddress(native);
    request->setDestAddress(router.address);
    request->setSrcPanId(router.domain->getPanId());
    request->setDestPanId(router.domain->getPanId());
    owner.sendPacketToIpv6Module(packet, key.second, address, key.first);
}

void LowpanHostRegistration::startNextRegistration(const RouterKey& key)
{
    auto found = routers.find(key);
    auto interface = owner.ift->getInterfaceById(key.first);
    if (found == routers.end() || !interface)
        return;
    auto& router = found->second;
    if (!router.pending.isUnspecified() || router.blockedUntil > simTime())
        return;
    collectAddresses(interface);
    std::vector<Ipv6Address> candidates;
    for (const auto& entry : addresses)
        if (entry.first.first == key.first && !entry.second.rejected &&
                interface->getProtocolData<Ipv6InterfaceData>()->hasValidAddress(entry.first.second, simTime()))
            candidates.push_back(entry.first.second);
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.isLinkLocal() && !b.isLinkLocal();
    });
    for (const auto& address : candidates) {
        auto& accepted = router.registrations[address];
        auto deadline = addresses.at({key.first, address}).validUntil;
        if (accepted.validUntil > simTime() && accepted.refreshAt == SIMTIME_MAX &&
                (deadline == SIMTIME_ZERO || deadline > accepted.validUntil))
            accepted.refreshAt = simTime();
        if (accepted.validUntil <= simTime() || accepted.refreshAt <= simTime()) {
            router.pending = address;
            router.attempts = 1;
            router.retryAt = simTime() + retryInterval;
            sendRegistration(key, address, registrationLifetime);
            break;
        }
    }
}

void LowpanHostRegistration::rejectAddress(const AddressKey& key)
{
    auto interface = owner.ift->getInterfaceById(key.first);
    for (auto& entry : routers)
        if (entry.first.first == key.first) {
            auto registered = entry.second.registrations.find(key.second);
            if (registered != entry.second.registrations.end() && registered->second.validUntil > simTime())
                sendRegistration(entry.first, key.second, 0);
            entry.second.registrations.erase(key.second);
            entry.second.pending = Ipv6Address::UNSPECIFIED_ADDRESS;
            entry.second.retryAt = SIMTIME_MAX;
            entry.second.blockedUntil = simTime() + SimTime(10);
        }
    auto existing = addresses.find(key);
    if (existing != addresses.end()) existing->second.rejected = true;
    if (interface && interface->getProtocolData<Ipv6InterfaceData>()->hasAddress(key.second))
        interface->getProtocolDataForUpdate<Ipv6InterfaceData>()->removeAddress(key.second);
}

void LowpanHostRegistration::processRegistrationReply(Packet *packet, const Ipv6NeighbourAdvertisement *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    expireAddresses();
    auto source = packet->findTag<L3AddressInd>();
    auto hop = packet->findTag<HopLimitInd>();
    auto native = packet->findTag<Ieee802154AddressInd>();
    if (!source || !hop || hop->getHopLimit() != 255 || !native || message->getCode() != 0 ||
            message->isIncorrect() || message->isIncomplete() || message->getChunkLength() != packet->getDataLength() ||
            !message->getRouterFlag() || !message->getSolicitedFlag() || !Icmpv6::verifyChecksum(packet)) {
        drop(packet);
        return;
    }
    RouterKey key{interface->getInterfaceId(), source->getSrcAddress().toIpv6()};
    auto found = routers.find(key);
    const Ipv6NdOption *aro = nullptr;
    int count = 0;
    for (size_t i = 0; i < message->getOptions().getOptionArraySize(); i++)
        if (int(message->getOptions().getOption(i)->getType()) == 33) { aro = message->getOptions().getOption(i); count++; }
    if (found == routers.end() || found->second.pending.isUnspecified() || found->second.validUntil <= simTime() ||
            native->getSrcAddress() != found->second.address || message->getTargetAddress() != key.second ||
            native->getSrcPanId() != domain->getPanId() || native->getDestPanId() != domain->getPanId() ||
            count != 1 || aro->getOptionLength() != 2 || aro->getPaddingBytesArraySize() != 14) {
        delete packet;
        return;
    }
    uint64_t ownerValue = 0;
    for (int i = 0; i < 8; i++) ownerValue = (ownerValue << 8) | uint8_t(aro->getPaddingBytes(i + 6));
    int status = uint8_t(aro->getPaddingBytes(0));
    auto& router = found->second;
    auto pending = router.pending;
    auto expectedDestination = status == 0 ? pending : interface->getProtocolData<Ipv6InterfaceData>()->getLinkLocalAddress();
    if (ownerValue != domain->getPeer(interface).address.getInt() || source->getDestAddress().toIpv6() != expectedDestination) {
        delete packet;
        return;
    }
    int lifetime = (uint8_t(aro->getPaddingBytes(4)) << 8) | uint8_t(aro->getPaddingBytes(5));
    if (status != 0 || lifetime == 0) {
        if (status == 1) rejectAddress({key.first, pending});
        removeRouter(key, true);
    }
    else {
        auto peerDeadline = finiteDeadline(60 * lifetime);
        auto addressDeadline = addresses.at({key.first, pending}).validUntil;
        auto until = addressDeadline == SIMTIME_ZERO ? peerDeadline : std::min(peerDeadline, addressDeadline);
        auto refresh = simTime() + (peerDeadline - simTime()) * refreshFraction;
        router.registrations[pending] = {until, refresh < until ? refresh : SIMTIME_MAX};
        router.pending = Ipv6Address::UNSPECIFIED_ADDRESS;
        router.retryAt = SIMTIME_MAX;
        owner.emit(owner.nativeHostRegistrationSucceededSignal, packet);
        startNextRegistration(key);
    }
    delete packet;
    scheduleTimer();
}

void LowpanHostRegistration::expireAddresses()
{
    for (auto it = addresses.begin(); it != addresses.end();) {
        auto current = it++;
        auto interface = owner.ift->getInterfaceById(current->first.first);
        if (current->second.rejected || (interface &&
                interface->getProtocolData<Ipv6InterfaceData>()->hasValidAddress(current->first.second, simTime())))
            continue;
        for (auto& router : routers)
            if (router.first.first == current->first.first) {
                router.second.registrations.erase(current->first.second);
                if (router.second.pending == current->first.second) {
                    router.second.pending = Ipv6Address::UNSPECIFIED_ADDRESS;
                    router.second.retryAt = SIMTIME_MAX;
                    // Base ARO errors identify no address. Let replies to the expired request drain
                    // before another address can become the outstanding registration on this router.
                    router.second.blockedUntil = simTime() + SimTime(10);
                }
            }
        if (interface && interface->getProtocolData<Ipv6InterfaceData>()->hasAddress(current->first.second))
            interface->getProtocolDataForUpdate<Ipv6InterfaceData>()->removeAddress(current->first.second);
        addresses.erase(current);
    }
}

void LowpanHostRegistration::handleTimer()
{
    expireAddresses();
    for (auto it = routers.begin(); it != routers.end();) {
        auto current = it++;
        auto key = current->first;
        auto& router = current->second;
        auto interface = owner.ift->getInterfaceById(key.first);
        if (!interface || router.validUntil <= simTime()) { removeRouter(key, true); continue; }
        if (!router.pending.isUnspecified() && router.retryAt <= simTime()) {
            if (router.attempts >= 3) { removeRouter(key, true); continue; }
            router.attempts++;
            router.retryAt = simTime() + retryInterval;
            sendRegistration(key, router.pending, registrationLifetime);
        }
        if (router.informationRefreshAt <= simTime()) {
            router.informationRefreshAt = SIMTIME_MAX;
            startDiscovery(interface);
        }
        startNextRegistration(key);
    }
    for (auto& entry : discoveries)
        if (entry.second.active && entry.second.next <= simTime())
            if (auto interface = owner.ift->getInterfaceById(entry.first)) sendRouterSolicitation(interface, entry.second);
    scheduleTimer();
}

void LowpanHostRegistration::scheduleTimer()
{
    owner.cancelEvent(hostTimer);
    auto next = SIMTIME_MAX;
    auto include = [&](simtime_t time) { if (time > simTime()) next = std::min(next, time); };
    for (const auto& entry : discoveries) if (entry.second.active) include(entry.second.next);
    for (const auto& entry : routers) {
        include(entry.second.validUntil);
        include(entry.second.informationRefreshAt);
        include(entry.second.blockedUntil);
        if (!entry.second.pending.isUnspecified()) include(entry.second.retryAt);
        else for (const auto& address : entry.second.registrations) include(address.second.refreshAt);
        for (const auto& address : entry.second.registrations) include(address.second.validUntil);
    }
    for (const auto& address : addresses) if (!address.second.rejected) include(address.second.validUntil);
    if (next != SIMTIME_MAX) owner.scheduleAt(next, hostTimer);
}

bool LowpanHostRegistration::processPacket(Packet *packet, const Icmpv6Header *message, NetworkInterface *interface,
        LowpanNativeLinkDomain *domain)
{
    if (auto advertisement = dynamic_cast<const Ipv6RouterAdvertisement *>(message))
        processAdvertisement(packet, advertisement, interface, domain);
    else if (auto advertisement = dynamic_cast<const Ipv6NeighbourAdvertisement *>(message))
        processRegistrationReply(packet, advertisement, interface, domain);
    else
        return false;
    return true;
}

bool LowpanHostRegistration::isSourceUsable(NetworkInterface *interface, const Ipv6Address& address) const
{
    auto tracked = addresses.find({interface->getInterfaceId(), address});
    if (tracked == addresses.end() || tracked->second.rejected ||
            !interface->getProtocolData<Ipv6InterfaceData>()->hasValidAddress(address, simTime()) ||
            interface->getProtocolData<Ipv6InterfaceData>()->isTentativeAddress(address))
        return false;
    for (const auto& router : routers)
        if (router.first.first == interface->getInterfaceId() && router.second.validUntil > simTime()) {
            auto accepted = router.second.registrations.find(address);
            if (accepted != router.second.registrations.end() && accepted->second.validUntil > simTime())
                return true;
        }
    return false;
}
} }
