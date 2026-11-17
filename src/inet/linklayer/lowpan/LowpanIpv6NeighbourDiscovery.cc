// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"

#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanHostRegistration.h"
#include "inet/linklayer/lowpan/LowpanRegistrationAuthority.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6RoutingTable.h"
#include "inet/networklayer/ipv6/Ipv6Header_m.h"

namespace inet { namespace lowpan {

Define_Module(LowpanIpv6NeighbourDiscovery);
simsignal_t LowpanIpv6NeighbourDiscovery::registrationCountChangedSignal = registerSignal("registrationCountChanged");
simsignal_t LowpanIpv6NeighbourDiscovery::nativeRouterDiscoveredSignal = registerSignal("nativeRouterDiscovered");
simsignal_t LowpanIpv6NeighbourDiscovery::nativeHostRegistrationSucceededSignal = registerSignal("nativeHostRegistrationSucceeded");
simsignal_t LowpanIpv6NeighbourDiscovery::duplicateAddressEntryCountChangedSignal = registerSignal("duplicateAddressEntryCountChanged");

namespace {
uint32_t prefixLifetime(simtime_t configured)
{
    // Ipv6InterfaceData::AdvPrefix uses zero for infinity, negative for a duration.
    if (configured == SIMTIME_ZERO)
        return UINT32_MAX;
    auto remaining = configured < SIMTIME_ZERO ? -configured : std::max(SIMTIME_ZERO, configured - simTime());
    return remaining.dbl() >= UINT32_MAX ? UINT32_MAX : uint32_t(remaining.dbl());
}
}

LowpanIpv6NeighbourDiscovery::~LowpanIpv6NeighbourDiscovery()
{
    updateHostAddressSubscription(false);
    hostRegistration.reset();
    registrationAuthority.reset();
    cancelAndDelete(routerAdvertisementTimer);
    routerAdvertisementTimer = nullptr;
    cancelAndDelete(registrationExpiryTimer);
    registrationExpiryTimer = nullptr;
    clearRegistrations(false);
}

void LowpanIpv6NeighbourDiscovery::initialize(int stage)
{
    Ipv6NeighbourDiscovery::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        std::string role = par("nativeNdRole").stdstringValue();
        if (role != "static" && role != "router" && role != "host" && role != "borderRouter")
            throw cRuntimeError("Invalid native LoWPAN ND role");
        registrationRouter = role == "router" || role == "borderRouter";
        if (role == "host")
            hostRegistration = std::make_unique<LowpanHostRegistration>(*this);
        if (role == "borderRouter")
            registrationAuthority = std::make_unique<LowpanRegistrationAuthority>(*this);
        maxRegistrations = par("maxRegistrations");
        if (maxRegistrations <= 0)
            throw cRuntimeError("Native registration capacity must be positive");
        registrationExpiryTimer = new cMessage("registrationExpiryTimer");
        routerAdvertisementTimer = new cMessage("routerAdvertisementTimer");
        maxPendingRouterAdvertisements = par("maxPendingRouterAdvertisements");
        if (maxPendingRouterAdvertisements <= 0)
            throw cRuntimeError("Native pending RA capacity must be positive");
        WATCH_EXPR("numNativeRegistrations", registrations.size());
        WATCH_EXPR("numPendingRouterAdvertisements", pendingRouterAdvertisements.size());
        WATCH_EXPR("numNativeRouters", hostRegistration ? hostRegistration->getNumRouters() : 0);
        WATCH_EXPR("numNativeHostAddresses", hostRegistration ? hostRegistration->getNumAddresses() : 0);
        WATCH_EXPR("numDuplicateAddressEntries", registrationAuthority ? registrationAuthority->getNumEntries() : 0);
    }
}

void LowpanIpv6NeighbourDiscovery::handleMessageWhenUp(cMessage *message)
{
    if (registrationAuthority && registrationAuthority->ownsTimer(message))
        registrationAuthority->handleTimer();
    else if (hostRegistration && hostRegistration->ownsTimer(message))
        hostRegistration->handleTimer();
    else if (message == routerAdvertisementTimer) {
        for (auto it = pendingRouterAdvertisements.begin(); it != pendingRouterAdvertisements.end();) {
            if (it->second <= simTime()) {
                auto key = it->first;
                it = pendingRouterAdvertisements.erase(it);
                sendNativeRouterAdvertisement(key);
            }
            else
                ++it;
        }
        scheduleRouterAdvertisements();
    }
    else if (message == registrationExpiryTimer) {
        expireRegistrations();
        scheduleRegistrationExpiry();
    }
    else
        Ipv6NeighbourDiscovery::handleMessageWhenUp(message);
}

void LowpanIpv6NeighbourDiscovery::stop()
{
    updateHostAddressSubscription(false);
    if (hostRegistration)
        hostRegistration->clear();
    if (registrationAuthority)
        registrationAuthority->clear();
    clearRouterAdvertisements();
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
        data->hasValidAddress(message->getTargetAddress(), simTime()) &&
        !data->isTentativeAddress(message->getTargetAddress()) &&
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
    int seconds = 60 * lifetime;
    auto deadline = double(seconds) >= (SIMTIME_MAX - simTime()).dbl() ?
        SIMTIME_MAX : simTime() + SimTime(seconds);
    requested = {Ieee802154Address(Ieee802154Address::EXTENDED, ownerValue), native->getSrcAddress(),
        domain, interface->getId(), deadline};
    return true;
}

int LowpanIpv6NeighbourDiscovery::applyRegistration(const RegistrationKey& key, const Registration& requested)
{
    auto existing = registrations.find(key);
    auto fixed = requested.domain->resolveStaticNeighbor(requested.interfaceModuleId, key.second);
    if (rt6->isLocalAddress(key.second) || (existing != registrations.end() && existing->second.owner != requested.owner) ||
            (fixed != nullptr && *fixed != requested.address))
        return 1;
    bool remove = requested.validUntil == simTime();
    std::array<uint8_t, 8> authorityOwner;
    for (int i = 0; i < 8; i++)
        authorityOwner[i] = requested.owner.getInt() >> (8 * (7 - i));
    if (registrationAuthority) {
        int status = registrationAuthority->checkRegistration(key.second, authorityOwner, remove);
        if (status != 0)
            return status;
    }
    if (!remove && existing == registrations.end() && int(registrations.size()) >= maxRegistrations)
        return 2;
    // Domain refusal is non-mutating and does not publish an authority reservation.
    if (!remove && !requested.domain->installDynamicNeighbor(requested.interfaceModuleId, key.second,
            requested.address, requested.validUntil, this))
        return 1;
    auto oldSize = registrations.size();
    if (remove)
        removeRegistration(key, false);
    else
        registrations[key] = requested;
    bool authorityChanged = registrationAuthority &&
        registrationAuthority->commitRegistration(key.second, authorityOwner, requested.validUntil);
    if (!remove) {
        removeRegistrationRoute(key);
        auto route = new Ipv6Route(key.second, 128, IRoute::UNKNOWN);
        route->setSource(this);
        route->setInterface(ift->getInterfaceById(key.first));
        route->setExpiryTime(requested.validUntil);
        route->setMetric(0);
        route->setAdminDist(Ipv6Route::dDirectlyConnected);
        rt6->addRoutingProtocolRoute(route);
    }
    if (authorityChanged)
        registrationAuthority->publishEntryCount();
    if (registrations.size() != oldSize)
        emit(registrationCountChangedSignal, long(registrations.size()));
    return 0;
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

bool LowpanIpv6NeighbourDiscovery::readNativeSource(const Packet *packet, const Ipv6NdOptions& options,
        LowpanNativeLinkDomain *domain, Ieee802154Address& address) const
{
    auto source = packet->findTag<L3AddressInd>();
    auto hop = packet->findTag<HopLimitInd>();
    auto native = packet->findTag<Ieee802154AddressInd>();
    const Ipv6NdOption *sllao = nullptr;
    int count = 0;
    for (size_t i = 0; i < options.getOptionArraySize(); i++)
        if (options.getOption(i)->getType() == IPv6ND_SOURCE_LINK_LAYER_ADDR_OPTION) {
            sllao = options.getOption(i);
            count++;
        }
    if (!source || source->getSrcAddress().getType() != L3Address::IPv6 ||
            !source->getSrcAddress().toIpv6().isUnicast() || source->getSrcAddress().toIpv6().isUnspecified() ||
            !hop || hop->getHopLimit() != 255 || !native || count != 1 || sllao->getOptionLength() != 2 ||
            sllao->getPaddingBytesArraySize() != 14)
        return false;
    uint64_t value = 0;
    for (int i = 0; i < 8; i++)
        value = (value << 8) | uint8_t(sllao->getPaddingBytes(i));
    address = Ieee802154Address(Ieee802154Address::EXTENDED, value);
    return address == native->getSrcAddress() && domain->contains(address) &&
        native->getSrcPanId() == domain->getPanId() && native->getDestPanId() == domain->getPanId() &&
        Icmpv6::verifyChecksum(packet);
}

Ipv6NdOption *LowpanIpv6NeighbourDiscovery::createNativeLinkOption(const Ieee802154Address& address) const
{
    auto option = new Ipv6NdOption();
    option->setType(IPv6ND_SOURCE_LINK_LAYER_ADDR_OPTION);
    option->setOptionLength(2);
    option->setPaddingBytesArraySize(14);
    for (int i = 0; i < 14; i++)
        option->setPaddingBytes(i, i < 8 ? uint8_t(address.getInt() >> (8 * (7 - i))) : 0);
    return option;
}

void LowpanIpv6NeighbourDiscovery::processNativeRouterSolicitation(Packet *packet, const Ipv6RouterSolicitation *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    Ieee802154Address requester;
    if (message->isIncorrect() || message->isIncomplete() || message->getCode() != 0 ||
            message->getChunkLength() != packet->getDataLength() ||
            !readNativeSource(packet, message->getOptions(), domain, requester)) {
        LowpanPacketDropDetails details;
        details.setReason(OTHER_PACKET_DROP);
        details.setLowpanReason(LOWPAN_MALFORMED);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    if (!data->getAdvSendAdvertisements()) {
        delete packet;
        return;
    }
    RouterAdvertisementKey key{interface->getInterfaceId(), packet->getTag<L3AddressInd>()->getSrcAddress().toIpv6(), requester};
    if (pendingRouterAdvertisements.count(key) == 0) {
        if (int(pendingRouterAdvertisements.size()) >= maxPendingRouterAdvertisements) {
            LowpanPacketDropDetails details;
            details.setReason(OTHER_PACKET_DROP);
            details.setLowpanReason(LOWPAN_RESOURCE_LIMIT);
            emit(packetDroppedSignal, packet, &details);
            delete packet;
            return;
        }
        // RFC 4861 section 6.2.6 and RFC 6775 section 9: random delay up to two seconds.
        pendingRouterAdvertisements.emplace(key, simTime() + SimTime(uniform(0, 2)));
        scheduleRouterAdvertisements();
    }
    delete packet;
}

void LowpanIpv6NeighbourDiscovery::scheduleRouterAdvertisements()
{
    cancelEvent(routerAdvertisementTimer);
    if (pendingRouterAdvertisements.empty())
        return;
    auto earliest = SIMTIME_MAX;
    for (const auto& response : pendingRouterAdvertisements)
        earliest = std::min(earliest, response.second);
    scheduleAt(earliest, routerAdvertisementTimer);
}

void LowpanIpv6NeighbourDiscovery::clearRouterAdvertisements()
{
    cancelEvent(routerAdvertisementTimer);
    pendingRouterAdvertisements.clear();
}

void LowpanIpv6NeighbourDiscovery::sendNativeRouterAdvertisement(const RouterAdvertisementKey& key)
{
    auto interface = ift->getInterfaceById(std::get<0>(key));
    auto boundary = interface ? dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) : nullptr;
    if (!boundary || !interface->isUp())
        return;
    auto domain = boundary->getLinkDomain();
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    if (!data->getAdvSendAdvertisements() || data->getLinkLocalAddress().isUnspecified())
        return;
    auto lifetime = data->getAdvDefaultLifetime();
    if (lifetime < SIMTIME_ZERO || lifetime > SimTime(65535))
        throw cRuntimeError("Native RA router lifetime must fit its 16-bit seconds field");
    auto advertisement = makeShared<Ipv6RouterAdvertisement>();
    advertisement->setCurHopLimit(data->getAdvCurHopLimit());
    advertisement->setManagedAddrConfFlag(data->getAdvManagedFlag());
    advertisement->setOtherStatefulConfFlag(data->getAdvOtherConfigFlag());
    advertisement->setRouterLifetime(lifetime.dbl());
    advertisement->setReachableTime(data->getAdvReachableTime());
    advertisement->setRetransTimer(data->getAdvRetransTimer());
    advertisement->getOptionsForUpdate().appendOption(createNativeLinkOption(domain->getPeer(interface).address));
    advertisement->addChunkLength(B(16));
    for (int i = 0; i < data->getNumAdvPrefixes(); i++) {
        auto prefix = data->getAdvPrefix(i);
        auto option = new Ipv6NdPrefixInformation();
        option->setPrefix(prefix.prefix);
        option->setPrefixLength(prefix.prefixLength);
        option->setOnlinkFlag(false); // RFC 6775 section 6.3: 6LR prefixes are off-link.
        option->setAutoAddressConfFlag(prefix.advAutonomousFlag);
        auto valid = prefixLifetime(prefix.advValidLifetime);
        auto preferred = prefixLifetime(prefix.advPreferredLifetime);
        if (preferred > valid)
            throw cRuntimeError("Native RA prefix preferred lifetime exceeds its valid lifetime");
        option->setValidLifetime(valid);
        option->setPreferredLifetime(preferred);
        advertisement->getOptionsForUpdate().appendOption(option);
        advertisement->addChunkLength(B(32));
    }
    auto output = new Packet("native-router-advertisement");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, advertisement, output);
    output->insertAtFront(advertisement);
    auto request = output->addTag<Ieee802154AddressReq>();
    request->setSrcAddress(domain->getPeer(interface).address);
    request->setDestAddress(std::get<2>(key));
    request->setSrcPanId(domain->getPanId());
    request->setDestPanId(domain->getPanId());
    sendPacketToIpv6Module(output, std::get<1>(key), data->getLinkLocalAddress(), interface->getInterfaceId());
}

void LowpanIpv6NeighbourDiscovery::start()
{
    if ((registrationRouter || registrationAuthority) && !rt6->isRouter())
        throw cRuntimeError("Native ND router and borderRouter roles require IPv6 forwarding to be enabled");
    Ipv6NeighbourDiscovery::start();
    if (hostRegistration)
        updateHostAddressSubscription(true);
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
        if (hostRegistration && dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) &&
                !data->getLinkLocalAddress().isUnspecified())
            startRouterDiscovery(interface);
    }
}

void LowpanIpv6NeighbourDiscovery::updateHostAddressSubscription(bool enabled)
{
    if (enabled == hostAddressSubscription)
        return;
    if (ift) {
        auto component = check_and_cast<cComponent *>(ift.get());
        if (enabled) component->subscribe(ipv6AddressAssignedSignal, this);
        else component->unsubscribe(ipv6AddressAssignedSignal, this);
    }
    hostAddressSubscription = enabled;
}

void LowpanIpv6NeighbourDiscovery::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    if (signal == ipv6AddressAssignedSignal && hostRegistration) {
        Enter_Method_Silent();
        auto info = check_and_cast<Ipv6AddressInfo *>(object);
        if (isUp() && info->ie && dynamic_cast<LowpanNativeBoundary *>(info->ie->getSubmodule("boundary")))
            hostRegistration->addressAssigned(info->ie);
    }
    else
        Ipv6NeighbourDiscovery::receiveSignal(source, signal, object, details);
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
    if (hostRegistration && dynamic_cast<LowpanNativeBoundary *>(ie->getSubmodule("boundary")))
        hostRegistration->startDiscovery(ie);
    else if (ie->getProtocol() != &lowpanProtocol)
        Ipv6NeighbourDiscovery::startRouterDiscovery(ie);
}

void LowpanIpv6NeighbourDiscovery::createRaTimer(NetworkInterface *ie)
{
    if (ie->getProtocol() != &lowpanProtocol)
        Ipv6NeighbourDiscovery::createRaTimer(ie);
}

void LowpanIpv6NeighbourDiscovery::processNativeNeighbourSolicitation(Packet *packet,
        const Ipv6NeighbourSolicitation *message, NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    auto addresses = packet->findTag<L3AddressInd>();
    auto hop = packet->findTag<HopLimitInd>();
    auto native = packet->findTag<Ieee802154AddressInd>();
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    auto target = message->getTargetAddress();
    bool valid = !message->isIncorrect() && !message->isIncomplete() && message->getCode() == 0 &&
        message->getChunkLength() == packet->getDataLength() && addresses &&
        addresses->getSrcAddress().getType() == L3Address::IPv6 &&
        addresses->getDestAddress().getType() == L3Address::IPv6 && hop && hop->getHopLimit() == 255 &&
        (addresses->getSrcAddress().toIpv6().isUnicast() || addresses->getSrcAddress().toIpv6().isUnspecified()) &&
        target.isUnicast() && !target.isUnspecified() && data->hasValidAddress(target, simTime()) &&
        !data->isTentativeAddress(target) && native && domain->contains(native->getSrcAddress()) &&
        native->getSrcPanId() == domain->getPanId() && native->getDestPanId() == domain->getPanId() &&
        Icmpv6::verifyChecksum(packet);
    int sourceOptions = 0;
    for (size_t i = 0; i < message->getOptions().getOptionArraySize(); i++) {
        auto option = message->getOptions().getOption(i);
        valid &= option->getOptionLength() != 0 && int(option->getType()) != 33;
        sourceOptions += option->getType() == IPv6ND_SOURCE_LINK_LAYER_ADDR_OPTION;
    }
    Ieee802154Address source;
    if (sourceOptions != 0)
        valid &= readNativeSource(packet, message->getOptions(), domain, source);
    bool dad = addresses && addresses->getSrcAddress().getType() == L3Address::IPv6 &&
        addresses->getSrcAddress().toIpv6().isUnspecified();
    if (dad)
        valid &= sourceOptions == 0 && addresses->getDestAddress().toIpv6() == target.formSolicitedNodeMulticastAddress();
    if (!valid) {
        LowpanPacketDropDetails details;
        details.setReason(OTHER_PACKET_DROP);
        details.setLowpanReason(LOWPAN_MALFORMED);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }
    bool multicast = addresses->getDestAddress().toIpv6().isMulticast();
    auto reply = makeShared<Ipv6NeighbourAdvertisement>();
    reply->setTargetAddress(target);
    reply->setRouterFlag(registrationRouter);
    reply->setSolicitedFlag(!dad);
    reply->setOverrideFlag(multicast);
    if (multicast) {
        auto option = createNativeLinkOption(domain->getPeer(interface).address);
        option->setType(IPv6ND_TARGET_LINK_LAYER_ADDR_OPTION);
        reply->getOptionsForUpdate().appendOption(option);
        reply->addChunkLength(B(16));
    }
    auto output = new Packet("native-neighbour-advertisement");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, reply, output);
    output->insertAtFront(reply);
    auto request = output->addTag<Ieee802154AddressReq>();
    request->setSrcAddress(domain->getPeer(interface).address);
    request->setDestAddress(dad ? Ieee802154Address("ffff") : native->getSrcAddress());
    request->setSrcPanId(domain->getPanId());
    request->setDestPanId(domain->getPanId());
    sendPacketToIpv6Module(output, dad ? Ipv6Address::ALL_NODES_2 : addresses->getSrcAddress().toIpv6(), target, interface->getInterfaceId());
    delete packet;
}

void LowpanIpv6NeighbourDiscovery::processNDMessage(Packet *packet, const Icmpv6Header *message)
{
    if (registrationAuthority && registrationAuthority->processPacket(packet, message))
        return;
    auto indication = packet->findTag<InterfaceInd>();
    auto interface = indication != nullptr ? ift->getInterfaceById(indication->getInterfaceId()) : nullptr;
    auto boundary = interface != nullptr ? dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) : nullptr;
    if ((hostRegistration || registrationRouter) && boundary)
        if (auto solicitation = dynamic_cast<const Ipv6NeighbourSolicitation *>(message)) {
            bool registration = false;
            for (size_t i = 0; i < solicitation->getOptions().getOptionArraySize(); i++)
                registration |= int(solicitation->getOptions().getOption(i)->getType()) == 33;
            if (registrationRouter && registration)
                processRegistration(packet, solicitation, interface, boundary->getLinkDomain());
            else
                processNativeNeighbourSolicitation(packet, solicitation, interface, boundary->getLinkDomain());
            return;
        }
    if (hostRegistration && boundary && hostRegistration->processPacket(packet, message, interface, boundary->getLinkDomain()))
        return;
    if (registrationRouter && boundary != nullptr) {
        if (auto solicitation = dynamic_cast<const Ipv6RouterSolicitation *>(message)) {
            processNativeRouterSolicitation(packet, solicitation, interface, boundary->getLinkDomain());
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

bool LowpanIpv6NeighbourDiscovery::isNativeSourceUsable(NetworkInterface *interface, const Ipv6Address& address) const
{
    Enter_Method_Silent();
    return !hostRegistration || !dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) ||
        hostRegistration->isSourceUsable(interface, address);
}

void LowpanIpv6NeighbourDiscovery::processIpv6Datagram(Packet *packet)
{
    if (hostRegistration) {
        auto source = packet->peekAtFront<Ipv6Header>()->getSrcAddress();
        auto control = dynamic_cast<const Ipv6NdControlInfo *>(packet->getControlInfo());
        bool managed = false, anyNative = false, allNative = true;
        for (int i = 0; i < ift->getNumInterfaces(); i++) {
            auto interface = ift->getInterface(i);
            if (interface->isLoopback()) continue;
            bool native = dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) != nullptr;
            anyNative |= native;
            allNative &= native;
            auto data = interface->findProtocolData<Ipv6InterfaceData>();
            if (native && ((control && control->getInterfaceId() == interface->getInterfaceId()) ||
                    (data && data->hasAddress(source))))
                managed = true;
        }
        if (managed || (source.isUnspecified() && anyNative && allNative)) {
            PacketDropDetails details;
            details.setReason(ADDRESS_RESOLUTION_FAILED);
            emit(packetDroppedSignal, packet, &details);
            delete packet;
            return;
        }
    }
    Ipv6NeighbourDiscovery::processIpv6Datagram(packet);
}

void LowpanIpv6NeighbourDiscovery::sendRedirect(Packet *redirectedPacket, const Ipv6Address& targetAddr,
        const Ipv6Address& destAddr, NetworkInterface *ie)
{
    // redirectedPacket is borrowed from IPv6, including on the suppressed path.
    if (ie->getProtocol() != &lowpanProtocol)
        Ipv6NeighbourDiscovery::sendRedirect(redirectedPacket, targetAddr, destAddr, ie);
}

} } // namespace inet::lowpan
