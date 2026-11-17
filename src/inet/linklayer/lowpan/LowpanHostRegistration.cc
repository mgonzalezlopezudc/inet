// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanHostRegistration.h"
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"
#include "inet/linklayer/lowpan/LowpanContextAdvertisements.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6RoutingTable.h"
#include <cmath>

namespace inet { namespace lowpan {
namespace {
simtime_t deadlineAfter(simtime_t reference, double seconds, int multiplier = 1)
{
    // Compare before constructing a duration or doubling a simulation time.
    return seconds >= (SIMTIME_MAX - reference).dbl() / multiplier ? SIMTIME_MAX :
        reference + SimTime(seconds * multiplier);
}

simtime_t finiteDeadline(uint32_t seconds)
{
    return deadlineAfter(simTime(), seconds);
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
    retryIntervalSeconds = owner.par("nativeRegistrationRetryInterval").doubleValue();
    refreshFraction = owner.par("nativeRegistrationRefreshFraction");
    if (maxRouters <= 0 || maxAddresses <= 0 || registrationLifetime < 1 || registrationLifetime > 65535 ||
            !std::isfinite(retryIntervalSeconds) || retryIntervalSeconds < 1 || refreshFraction <= 0 || refreshFraction >= 1)
        throw cRuntimeError("Invalid native host registration limits, lifetime, retry interval or refresh fraction");
    if (owner.extendedRegistration) {
        std::string configured = owner.par("nativeRegistrationOwnershipVerifier").stdstringValue();
        auto nibble = [](char c) { return c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        if (!configured.empty() && configured.size() != 16 && configured.size() != 32 && configured.size() != 48 && configured.size() != 64)
            throw cRuntimeError("nativeRegistrationOwnershipVerifier must be empty or contiguous hexadecimal encoding of 8, 16, 24 or 32 bytes");
        for (size_t i = 0; i < configured.size(); i += 2) {
            int high = nibble(configured[i]), low = nibble(configured[i + 1]);
            if (high < 0 || low < 0) throw cRuntimeError("nativeRegistrationOwnershipVerifier contains a non-hexadecimal character");
            configuredRovr.push_back((high << 4) | low);
        }
    }
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
        discovery.blockedUntil = finiteDeadline(10);
        discovery.active = false;
    }
    if (!discovery.active) {
        discovery.active = true;
        discovery.attempts = 0;
        auto earliest = discovery.lastSent < SIMTIME_ZERO ? simTime() : deadlineAfter(discovery.lastSent, 10);
        discovery.next = std::max({simTime(), discovery.blockedUntil, earliest});
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
    owner.appendCapabilities(message->getOptionsForUpdate(), *message, true);
    auto packet = new Packet("native-router-solicitation");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, message, packet);
    packet->insertAtFront(message);
    owner.sendPacketToIpv6Module(packet, Ipv6Address::ALL_ROUTERS_2, source, interface->getInterfaceId());
    discovery.lastSent = simTime();
    discovery.attempts++;
    // RFC 6775 section 5.3: three initial ten-second intervals, then bounded backoff.
    int interval = discovery.attempts < 3 ? 10 : discovery.attempts == 3 ? 20 : discovery.attempts == 4 ? 40 : 60;
    discovery.next = finiteDeadline(interval);
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
    if (owner.extendedRegistration)
        for (auto& history : histories) {
            if (history.second.transaction) {
                history.second.transaction->copies.erase(key);
                if (history.second.transaction->copies.empty()) history.second.transaction.reset();
            }
            auto& removals = history.second.removals;
            removals.erase(std::remove_if(removals.begin(), removals.end(), [&](const auto& copy) {return copy.first == key;}), removals.end());
        }
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
    for (auto& history : histories) { history.second.transaction.reset(); history.second.removals.clear(); }
    while (!routers.empty())
        removeRouter(routers.begin()->first, false);
    updatingAddresses = true;
    if (owner.ift)
        for (const auto& entry : addresses)
            if (entry.second.autonomous)
                if (auto interface = owner.ift->getInterfaceById(entry.first.first)) {
                    auto data = interface->findProtocolDataForUpdate<Ipv6InterfaceData>();
                    if (data && data->hasAddress(entry.first.second))
                        data->removeAddress(entry.first.second);
                }
    addresses.clear();
    updatingAddresses = false;
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
            if (owner.extendedRegistration ? enrollAddress(interface, address) == nullptr : int(addresses.size()) >= maxAddresses)
                continue;
            addresses[{interface->getInterfaceId(), address}] = {false, false, entry.second};
        }
        else if (!existing->second.autonomous)
            existing->second.validUntil = entry.second;
        if (!owner.extendedRegistration && data->isTentativeAddress(address))
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
        auto history = histories.find(historyKey(key));
        if (owner.extendedRegistration && history != histories.end() && history->second.rejected) continue;
        auto existing = addresses.find(key);
        if (existing != addresses.end() && (existing->second.rejected || !existing->second.autonomous))
            continue;
        if (existing == addresses.end() && (data->hasAddress(address) || (!owner.extendedRegistration && int(addresses.size()) >= maxAddresses) ||
                prefix->getValidLifetime() == 0))
            continue;
        if (owner.extendedRegistration && !enrollAddress(interface, address)) continue;
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

void LowpanHostRegistration::addressAssigned(NetworkInterface *interface, const Ipv6Address& address)
{
    if (updatingPrefixes || updatingAddresses)
        return;
    if (owner.extendedRegistration) {
        auto history = histories.find(historyKey({interface->getInterfaceId(), address}));
        if (history != histories.end()) history->second.rejected = false;
    }
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

LowpanHostRegistration::AddressKey LowpanHostRegistration::historyKey(const AddressKey& key) const
{
    return {key.second.isLinkLocal() ? key.first : -1, key.second};
}

LowpanHostRegistration::History *LowpanHostRegistration::enrollAddress(NetworkInterface *interface, const Ipv6Address& address)
{
    auto key = historyKey({interface->getInterfaceId(), address});
    auto existing = histories.find(key);
    if (existing != histories.end()) return &existing->second;
    if (histories.size() >= size_t(maxAddresses)) {
        Ipv6AddressInfo info(interface, address);
        owner.emit(owner.nativeHostRegistrationHistoryFullSignal, &info);
        return nullptr;
    }
    auto& history = histories[key];
    history.rovr = configuredRovr;
    if (history.rovr.empty()) {
        auto boundary = check_and_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary"));
        auto value = boundary->getLinkAddress().getInt();
        for (int i = 7; i >= 0; i--) history.rovr.push_back(value >> (8 * i));
    }
    return &history;
}

bool LowpanHostRegistration::readAuthority(const Ipv6NdOptions& options, std::optional<Ipv6Address>& authority, simtime_t& until) const
{
    authority.reset(); until = SIMTIME_MAX;
    for (size_t i = 0; i < options.getOptionArraySize(); i++) {
        const auto *option = options.getOption(i);
        if (int(option->getType()) != 35) continue;
        if (authority || option->getOptionLength() != 3 || option->getPaddingBytesArraySize() != 22) return false;
        uint32_t words[4]{};
        for (int j = 0; j < 16; j++) words[j / 4] = (words[j / 4] << 8) | uint8_t(option->getPaddingBytes(6 + j));
        Ipv6Address value(words[0], words[1], words[2], words[3]);
        if (!value.isUnicast() || value.isUnspecified() || value.isLoopback()) return false;
        authority = value;
        unsigned minutes = (uint8_t(option->getPaddingBytes(4)) << 8) | uint8_t(option->getPaddingBytes(5));
        until = deadlineAfter(simTime(), 60.0 * (minutes == 0 ? 10000 : minutes));
    }
    return true;
}

bool LowpanHostRegistration::validCopy(const RouterKey& key, const RegistrationCopy& copy) const
{
    auto found = routers.find(key);
    auto interface = owner.ift ? owner.ift->getInterfaceById(key.first) : nullptr;
    auto boundary = interface ? dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) : nullptr;
    if (found == routers.end() || !interface || !interface->isUp() || !boundary || !copy.domain ||
            interface->getId() != copy.interfaceModuleId || boundary->getLinkDomain() != copy.domain.get()) return false;
    const auto& router = found->second;
    if (router.validUntil <= simTime() || router.capabilityUntil <= simTime() || router.updated != copy.updated ||
            router.address != copy.peer || router.interfaceModuleId != copy.interfaceModuleId ||
            router.domain.get() != copy.domain.get() || boundary->getLinkAddress() != copy.localNative ||
            !copy.domain->contains(copy.peer)) return false;
    if (!copy.assignment.second.isLinkLocal() && (router.authority != copy.authority ||
            (copy.authority && router.authorityUntil <= simTime()))) return false;
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    if (!data->hasValidAddress(copy.source, simTime()) || data->isTentativeAddress(copy.source)) return false;
    if (copy.lifetime != 0 && (!data->hasValidAddress(copy.assignment.second, simTime()) ||
            data->isTentativeAddress(copy.assignment.second))) return false;
    if (copy.updated && copy.assignment.second != copy.source) {
        auto source = router.registrations.find(copy.source);
        if (!copy.source.isLinkLocal() || source == router.registrations.end() || source->second.validUntil <= simTime() ||
                !source->second.copy || !source->second.copy->updated || source->second.copy->peer != copy.peer ||
                source->second.copy->localNative != copy.localNative || source->second.copy->interfaceModuleId != copy.interfaceModuleId) return false;
    }
    return true;
}

bool LowpanHostRegistration::blocked(History& history, const Router& router)
{
    for (auto it = history.authorityCooldowns.begin(); it != history.authorityCooldowns.end();)
        if (it->second <= simTime()) it = history.authorityCooldowns.erase(it); else ++it;
    if (history.blockedUntil > simTime()) return true;
    auto group = router.authority ? history.authorityCooldowns.find(*router.authority) : history.authorityCooldowns.end();
    return group != history.authorityCooldowns.end();
}

void LowpanHostRegistration::coolDown(History& history, const std::optional<Ipv6Address>& authority, bool group)
{
    if (history.blockedUntil > simTime()) return;
    if (group && authority) {
        auto existing = history.authorityCooldowns.find(*authority);
        if (existing != history.authorityCooldowns.end() && existing->second > simTime()) return;
    }
    int seconds = history.failures == 0 ? 10 : history.failures == 1 ? 20 : history.failures == 2 ? 40 : 60;
    if (history.failures < 3) history.failures++;
    auto until = finiteDeadline(seconds);
    for (auto it = history.authorityCooldowns.begin(); it != history.authorityCooldowns.end();)
        if (it->second <= simTime()) it = history.authorityCooldowns.erase(it); else ++it;
    if (group && authority && (history.authorityCooldowns.count(*authority) || history.authorityCooldowns.size() < size_t(maxRouters)))
        history.authorityCooldowns[*authority] = std::max(history.authorityCooldowns[*authority], until);
    else history.blockedUntil = std::max(history.blockedUntil, until);
}

void LowpanHostRegistration::beginTransaction(const AddressKey& key, bool removal,
        const std::vector<std::pair<RouterKey, RegistrationCopy>>& saved, std::optional<RouterKey> initiatingRouter)
{
    auto found = histories.find(key);
    if (found == histories.end() || found->second.transaction) return;
    auto& history = found->second;
    Transaction transaction;
    transaction.removal = removal;
    if (removal) {
        for (const auto& entry : saved) {
            auto copy = entry.second;
            copy.lifetime = 0;
            if (validCopy(entry.first, copy)) transaction.copies[entry.first] = copy;
        }
    }
    else {
        for (const auto& assignment : addresses) {
            if (historyKey(assignment.first) != key || assignment.second.rejected) continue;
            auto interface = owner.ift->getInterfaceById(assignment.first.first);
            auto boundary = interface ? dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) : nullptr;
            if (!boundary) continue;
            for (const auto& peer : routers) {
                if (peer.first.first != assignment.first.first || blocked(history, peer.second)) continue;
                RegistrationCopy copy;
                copy.assignment = assignment.first; copy.updated = peer.second.updated;
                copy.source = copy.updated ? (assignment.first.second.isLinkLocal() ? assignment.first.second :
                    interface->getProtocolData<Ipv6InterfaceData>()->getLinkLocalAddress()) : assignment.first.second;
                copy.localNative = boundary->getLinkAddress(); copy.peer = peer.second.address;
                copy.domain = peer.second.domain; copy.interfaceModuleId = peer.second.interfaceModuleId;
                if (!assignment.first.second.isLinkLocal()) copy.authority = peer.second.authority;
                copy.lifetime = registrationLifetime;
                auto accepted = peer.second.registrations.find(assignment.first.second);
                if (accepted != peer.second.registrations.end() && accepted->second.copy) {
                    const auto& previous = *accepted->second.copy;
                    copy.previous = AcceptanceCorrelation{previous.full, previous.source, previous.localNative,
                        previous.peer, previous.updated, previous.registrarRecencyKnown, accepted->second.validUntil};
                }
                copy.full.rovr = history.rovr; copy.full.tidKnown = true;
                copy.wire = copy.full;
                if (!copy.updated) {
                    copy.wire.rovr.clear();
                    for (int i = 7; i >= 0; i--) copy.wire.rovr.push_back(copy.localNative.getInt() >> (8 * i));
                }
                if (validCopy(peer.first, copy)) transaction.copies.emplace(peer.first, copy);
            }
        }
    }
    if (initiatingRouter && !transaction.copies.count(*initiatingRouter)) return;
    bool ready = false;
    for (const auto& copy : transaction.copies) {
        auto router = routers.find(copy.first);
        if (router == routers.end() || !router->second.pending.isUnspecified() || router->second.blockedUntil > simTime()) continue;
        auto source = router->second.registrations.find(copy.second.source);
        if (copy.second.updated && !copy.second.assignment.second.isLinkLocal() && source != router->second.registrations.end() &&
                source->second.refreshAt <= simTime()) continue;
        ready = true;
    }
    if (!ready) {
        if (removal) {
            history.removals.clear();
            for (const auto& copy : transaction.copies) history.removals.push_back(copy);
        }
        return;
    }
    history.tid = !history.allocated ? 240 : history.tid == 255 || history.tid == 127 ? 0 : history.tid + 1;
    history.allocated = true;
    for (auto& copy : transaction.copies) {
        copy.second.full.tid = history.tid; copy.second.full.tidKnown = true;
        copy.second.wire.tid = history.tid; copy.second.wire.tidKnown = true;
    }
    history.transaction = std::move(transaction);
    if (removal) history.removals.clear();
}

void LowpanHostRegistration::finishCopy(const RouterKey& key, Ipv6Address address)
{
    auto found = histories.find(historyKey({key.first, address}));
    if (found != histories.end() && found->second.transaction) {
        found->second.transaction->copies.erase(key);
        if (found->second.transaction->copies.empty()) found->second.transaction.reset();
    }
    auto router = routers.find(key);
    if (router != routers.end() && router->second.pending == address) {
        router->second.pending = Ipv6Address::UNSPECIFIED_ADDRESS;
        router->second.retryAt = SIMTIME_MAX;
    }
}

void LowpanHostRegistration::sendExtendedRegistration(const RouterKey& key, const RegistrationCopy& copy)
{
    if (!validCopy(key, copy)) return;
    auto message = makeShared<Ipv6NeighbourSolicitation>();
    message->setTargetAddress(copy.updated ? copy.assignment.second : key.second);
    message->getOptionsForUpdate().appendOption(owner.createNativeLinkOption(copy.localNative));
    auto aro = copy.wire.makeOption(0, copy.lifetime, true);
    message->getOptionsForUpdate().appendOption(aro);
    message->addChunkLength(B(16 + aro->getOptionLength() * 8));
    auto packet = new Packet("native-host-extended-registration");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, message, packet); packet->insertAtFront(message);
    auto request = packet->addTag<Ieee802154AddressReq>();
    request->setSrcAddress(copy.localNative); request->setDestAddress(copy.peer);
    request->setSrcPanId(copy.domain->getPanId()); request->setDestPanId(copy.domain->getPanId());
    owner.sendPacketToIpv6Module(packet, key.second, copy.source, key.first);
}

void LowpanHostRegistration::startExtendedRegistrations()
{
    std::vector<RouterKey> keys;
    for (const auto& router : routers) keys.push_back(router.first);
    for (const auto& key : keys) startNextExtendedRegistration(key);
}

void LowpanHostRegistration::startNextExtendedRegistration(const RouterKey& key)
{
    auto interface = owner.ift->getInterfaceById(key.first);
    if (!interface) return;
    collectAddresses(interface);
    auto peer = routers.find(key);
    if (peer == routers.end() || !peer->second.pending.isUnspecified() || peer->second.blockedUntil > simTime()) return;
    auto linkLocal = interface->getProtocolData<Ipv6InterfaceData>()->getLinkLocalAddress();
    auto sourceLease = peer->second.registrations.find(linkLocal);
    bool linkLocalFirst = peer->second.updated && (sourceLease == peer->second.registrations.end() ||
        sourceLease->second.validUntil <= simTime() || sourceLease->second.refreshAt <= simTime());
    for (auto& history : histories) {
        if (linkLocalFirst && history.first != historyKey({key.first, linkLocal})) continue;
        if (!history.second.transaction && !history.second.removals.empty()) {
            auto saved = history.second.removals;
            beginTransaction(history.first, true, saved);
        }
        if (!history.second.transaction) continue;
        auto copy = history.second.transaction->copies.find(key);
        if (copy == history.second.transaction->copies.end()) continue;
        if (!validCopy(key, copy->second)) { finishCopy(key, copy->second.assignment.second); continue; }
        if (blocked(history.second, peer->second)) continue;
        peer = routers.find(key);
        if (peer == routers.end()) return;
        peer->second.pending = copy->second.assignment.second;
        peer->second.attempts = 1;
        peer->second.retryAt = registrationRetryDeadline(interface, false);
        sendExtendedRegistration(key, copy->second);
        return;
    }
    std::vector<Ipv6Address> candidates;
    for (const auto& address : addresses)
        if (address.first.first == key.first && !address.second.rejected) candidates.push_back(address.first.second);
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {return a.isLinkLocal() && !b.isLinkLocal();});
    for (const auto& address : candidates) {
        if (linkLocalFirst && address != linkLocal) continue;
        auto history = histories.find(historyKey({key.first, address}));
        if (history == histories.end() || history->second.transaction || !history->second.removals.empty() || blocked(history->second, peer->second)) continue;
        auto accepted = peer->second.registrations.find(address);
        auto addressDeadline = addresses.at({key.first, address}).validUntil;
        if (accepted != peer->second.registrations.end() && accepted->second.validUntil > simTime() &&
                accepted->second.refreshAt == SIMTIME_MAX && (addressDeadline == SIMTIME_ZERO || addressDeadline > accepted->second.validUntil))
            accepted->second.refreshAt = simTime();
        if (accepted != peer->second.registrations.end() && accepted->second.validUntil > simTime() && accepted->second.refreshAt > simTime()) continue;
        beginTransaction(history->first, false, {}, key);
        if (history->second.transaction) { startNextExtendedRegistration(key); return; }
    }
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
    int capabilities = 0;
    std::optional<Ipv6Address> authority;
    simtime_t authorityUntil = SIMTIME_MAX;
    if (owner.extendedRegistration && (!owner.readCapabilities(message->getOptions(), false, capabilities) ||
            !readAuthority(message->getOptions(), authority, authorityUntil))) {
        drop(packet);
        return;
    }
    if (owner.contextAdvertisements && !owner.contextAdvertisements->accept(*message, interface, source->getSrcAddress().toIpv6())) {
        drop(packet, LOWPAN_ND_UNSUPPORTED);
        return;
    }
    if (message->getRetransTimer() != 0)
        interface->getProtocolDataForUpdate<Ipv6InterfaceData>()->setRetransTimer(message->getRetransTimer());
    if (owner.contextAdvertisements) {
        expireAddresses();
        collectAddresses(interface);
        processPrefixes(interface, message->getOptions());
    }
    auto& discovery = discoveries[interface->getInterfaceId()];
    if (discovery.blockedUntil > simTime()) {
        delete packet;
        scheduleTimer();
        return;
    }
    RouterKey key{interface->getInterfaceId(), source->getSrcAddress().toIpv6()};
    if (routers.count(key) == 0 && int(routers.size()) >= maxRouters) {
        drop(packet, LOWPAN_RESOURCE_LIMIT);
        scheduleTimer();
        return;
    }
    expireAddresses();
    collectAddresses(interface);
    if (!owner.contextAdvertisements)
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
    if (owner.extendedRegistration) {
        auto old = routers.find(key);
        if (old != routers.end() && (old->second.address != native || old->second.domain.get() != domain ||
                old->second.interfaceModuleId != interface->getId())) removeRouter(key, false);
    }
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
    if (owner.extendedRegistration) {
        router.updated = capabilities & 2;
        router.capabilityUntil = until;
        router.authority = authority;
        router.authorityUntil = std::min(until, authorityUntil);
        for (auto& history : histories)
            if (history.second.transaction) {
                auto pending = history.second.transaction->copies.find(key);
                if (pending != history.second.transaction->copies.end() && !validCopy(key, pending->second))
                    finishCopy(key, pending->second.assignment.second);
            }
    }
    uint32_t informationLifetime = message->getRouterLifetime();
    for (size_t i = 0; i < message->getOptions().getOptionArraySize(); i++)
        if (auto prefix = dynamic_cast<const Ipv6NdPrefixInformation *>(message->getOptions().getOption(i)))
            if (!prefix->getOnlinkFlag() && prefix->getValidLifetime() != 0 && prefix->getValidLifetime() != UINT32_MAX &&
                    prefix->getValidLifetime() < informationLifetime)
                informationLifetime = prefix->getValidLifetime();
    router.informationRefreshAt = deadlineAfter(simTime(), double(informationLifetime) * refreshFraction);
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
    if (owner.extendedRegistration) { startExtendedRegistrations(); return; }
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
            router.retryAt = registrationRetryDeadline(interface, false);
            sendRegistration(key, address, registrationLifetime);
            break;
        }
    }
}

simtime_t LowpanHostRegistration::registrationRetryDeadline(NetworkInterface *interface, bool finalAttempt) const
{
    auto advertised = interface->getProtocolData<Ipv6InterfaceData>()->getRetransTimer();
    double interval = std::max({1.0, retryIntervalSeconds, double(advertised) / 1000});
    return deadlineAfter(simTime(), interval, finalAttempt ? 2 : 1);
}

void LowpanHostRegistration::rejectAddress(const AddressKey& key)
{
    if (owner.extendedRegistration) { removeExtendedAddress(key, true); return; }
    auto interface = owner.ift->getInterfaceById(key.first);
    for (auto& entry : routers)
        if (entry.first.first == key.first) {
            auto registered = entry.second.registrations.find(key.second);
            if (registered != entry.second.registrations.end() && registered->second.validUntil > simTime())
                sendRegistration(entry.first, key.second, 0);
            entry.second.registrations.erase(key.second);
            entry.second.pending = Ipv6Address::UNSPECIFIED_ADDRESS;
            entry.second.retryAt = SIMTIME_MAX;
            entry.second.blockedUntil = finiteDeadline(10);
        }
    auto existing = addresses.find(key);
    if (existing != addresses.end()) existing->second.rejected = true;
    if (interface && interface->getProtocolData<Ipv6InterfaceData>()->hasAddress(key.second))
        interface->getProtocolDataForUpdate<Ipv6InterfaceData>()->removeAddress(key.second);
}

void LowpanHostRegistration::removeExtendedAddress(const AddressKey& key, bool reject)
{
    auto scoped = historyKey(key);
    auto history = histories.find(scoped);
    if (history == histories.end()) return;
    if (reject) history->second.rejected = true;
    std::vector<AddressKey> removed;
    for (const auto& entry : addresses)
        if (reject ? historyKey(entry.first) == scoped : entry.first == key) removed.push_back(entry.first);
    if (removed.empty()) return;
    if (history->second.transaction && !history->second.transaction->removal) {
        std::vector<RouterKey> pending;
        for (const auto& copy : history->second.transaction->copies) pending.push_back(copy.first);
        for (const auto& peer : pending) finishCopy(peer, key.second);
    }
    for (const auto& assignment : removed) {
        for (auto& peer : routers) {
            if (peer.first.first != assignment.first) continue;
            auto accepted = peer.second.registrations.find(assignment.second);
            if (accepted != peer.second.registrations.end()) {
                if (accepted->second.validUntil > simTime() && accepted->second.copy)
                    history->second.removals.emplace_back(peer.first, *accepted->second.copy);
                peer.second.registrations.erase(accepted);
            }
        }
        addresses.erase(assignment);
        if (assignment.second.isLinkLocal()) {
            std::vector<std::pair<RouterKey, Ipv6Address>> dependent;
            for (const auto& item : histories)
                if (item.second.transaction)
                    for (const auto& copy : item.second.transaction->copies)
                        if (copy.first.first == assignment.first && copy.second.source == assignment.second)
                            dependent.emplace_back(copy.first, copy.second.assignment.second);
            for (const auto& copy : dependent) finishCopy(copy.first, copy.second);
        }
        auto interface = owner.ift->getInterfaceById(assignment.first);
        auto data = interface ? interface->findProtocolDataForUpdate<Ipv6InterfaceData>() : nullptr;
        if (data && data->hasAddress(assignment.second)) {
            updatingAddresses = true;
            data->removeAddress(assignment.second);
            updatingAddresses = false;
        }
    }
    auto saved = history->second.removals;
    if (!history->second.transaction && !saved.empty()) beginTransaction(scoped, true, saved);
}

void LowpanHostRegistration::addressRemoved(NetworkInterface *interface, const Ipv6Address& address)
{
    if (!owner.extendedRegistration || updatingAddresses || updatingPrefixes) return;
    removeExtendedAddress({interface->getInterfaceId(), address}, false);
    startExtendedRegistrations();
    scheduleTimer();
}

void LowpanHostRegistration::processExtendedReply(Packet *packet, const Ipv6NeighbourAdvertisement *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    expireAddresses();
    auto source = packet->findTag<L3AddressInd>();
    auto hop = packet->findTag<HopLimitInd>();
    auto native = packet->findTag<Ieee802154AddressInd>();
    if (!source || !hop || hop->getHopLimit() != 255 || !native || message->getCode() != 0 ||
            message->isIncorrect() || message->isIncomplete() || message->getChunkLength() != packet->getDataLength() ||
            !message->getRouterFlag() || !Icmpv6::verifyChecksum(packet)) {
        drop(packet); return;
    }
    RouterKey key{interface->getInterfaceId(), source->getSrcAddress().toIpv6()};
    auto router = routers.find(key);
    const Ipv6NdOption *aro = nullptr;
    int count = 0;
    for (size_t i = 0; i < message->getOptions().getOptionArraySize(); i++)
        if (int(message->getOptions().getOption(i)->getType()) == 33) {aro = message->getOptions().getOption(i); count++;}
    LowpanRegistrationInfo returned;
    if (router == routers.end() || router->second.validUntil <= simTime() || count != 1 ||
            native->getSrcAddress() != router->second.address || native->getSrcPanId() != domain->getPanId() ||
            native->getDestPanId() != domain->getPanId() || LowpanRegistrationInfo::parse(*aro, true, returned) != LowpanRegistrationInfo::VALID) {
        delete packet; return;
    }
    int status = uint8_t(aro->getPaddingBytes(0));
    int lifetime = (uint8_t(aro->getPaddingBytes(4)) << 8) | uint8_t(aro->getPaddingBytes(5));
    if (!message->getSolicitedFlag() && (status != 3 || !returned.tidKnown)) {delete packet; return;}
    auto matches = [&](const RegistrationCopy& copy, bool known) {
        uint64_t iid = copy.localNative.getInt() ^ UINT64_C(0x0200000000000000);
        auto destination = copy.updated ? copy.source : status == 0 ? copy.assignment.second :
            Ipv6Address(0xfe800000, 0, iid >> 32, uint32_t(iid));
        return native->getDestAddress() == copy.localNative && native->getSrcAddress() == copy.peer &&
            source->getDestAddress().toIpv6() == destination &&
            message->getTargetAddress() == (copy.updated ? copy.assignment.second : key.second) &&
            returned.rovr == copy.wire.rovr && (!known || (returned.tidKnown && returned.tid == copy.wire.tid)) &&
            (!copy.updated || returned.sameParameters(copy.wire));
    };
    if (status == 3 && returned.tidKnown && !message->getSolicitedFlag()) {
        auto accepted = router->second.registrations.find(message->getTargetAddress());
        if (accepted != router->second.registrations.end() && accepted->second.copy &&
                accepted->second.copy->registrarRecencyKnown && matches(*accepted->second.copy, true)) {
            auto saved = *accepted->second.copy;
            router->second.registrations.erase(accepted);
            auto history = histories.find(historyKey(saved.assignment));
            if (history != histories.end() && history->second.transaction) {
                auto pending = history->second.transaction->copies.find(key);
                if (pending != history->second.transaction->copies.end() && pending->second.wire.sameTransaction(saved.wire))
                    finishCopy(key, saved.assignment.second);
            }
            if (history != histories.end() && history->second.tid == saved.full.tid)
                coolDown(history->second, std::nullopt, false);
            delete packet; startExtendedRegistrations(); scheduleTimer(); return;
        }
    }
    auto address = router->second.pending;
    auto history = histories.find(historyKey({key.first, address}));
    if (address.isUnspecified() || history == histories.end() || !history->second.transaction) {delete packet; return;}
    auto found = history->second.transaction->copies.find(key);
    if (found == history->second.transaction->copies.end()) {delete packet; return;}
    auto copy = found->second;
    if (!message->getSolicitedFlag() && !copy.updated) {delete packet; return;}
    if (!matches(copy, copy.updated) || lifetime != copy.lifetime) {delete packet; return;}
    if (!validCopy(key, copy)) {
        finishCopy(key, address); delete packet; startExtendedRegistrations(); scheduleTimer(); return;
    }
    finishCopy(key, address);
    if (status == 0 && lifetime != 0) {
        auto until = finiteDeadline(60 * lifetime);
        auto assignment = addresses.find(copy.assignment);
        if (assignment == addresses.end()) {delete packet; scheduleTimer(); return;}
        if (assignment->second.validUntil != SIMTIME_ZERO) until = std::min(until, assignment->second.validUntil);
        auto refresh = simTime() + (finiteDeadline(60 * lifetime) - simTime()) * refreshFraction;
        copy.registrarRecencyKnown = copy.updated && returned.tidKnown;
        copy.previous.reset();
        routers.at(key).registrations[address] = {until, refresh < until ? refresh : SIMTIME_MAX, copy};
        history->second.failures = 0;
        if (copy.authority) history->second.authorityCooldowns.erase(*copy.authority);
        owner.emit(owner.nativeHostRegistrationSucceededSignal, packet);
    }
    else if (status == 0) routers.at(key).registrations.erase(address);
    else if (status == 1) removeExtendedAddress(copy.assignment, true);
    else if (status == 3 || status == 9) {
        if (status == 3 && message->getSolicitedFlag() && copy.previous) {
            auto accepted = routers.at(key).registrations.find(address);
            const auto& saved = *copy.previous;
            if (accepted != routers.at(key).registrations.end() && accepted->second.copy) {
                const auto& current = *accepted->second.copy;
                if (current.full.sameTransaction(saved.full) && current.source == saved.source &&
                        current.localNative == saved.localNative && current.peer == saved.peer && current.updated == saved.updated &&
                        current.registrarRecencyKnown == saved.registrarRecencyKnown && accepted->second.validUntil == saved.validUntil)
                    routers.at(key).registrations.erase(accepted);
            }
        }
        coolDown(history->second, copy.authority, status == 9);
    }
    else {
        coolDown(history->second, std::nullopt, false);
        removeRouter(key, true);
    }
    delete packet;
    startExtendedRegistrations();
    scheduleTimer();
}

void LowpanHostRegistration::processRegistrationReply(Packet *packet, const Ipv6NeighbourAdvertisement *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    if (owner.extendedRegistration) { processExtendedReply(packet, message, interface, domain); return; }
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
    if (owner.extendedRegistration) {
        std::vector<AddressKey> expired;
        for (const auto& address : addresses) {
            auto interface = owner.ift ? owner.ift->getInterfaceById(address.first.first) : nullptr;
            if (!interface || !interface->getProtocolData<Ipv6InterfaceData>()->hasValidAddress(address.first.second, simTime()))
                expired.push_back(address.first);
        }
        for (const auto& key : expired) removeExtendedAddress(key, false);
        return;
    }
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
                    router.second.blockedUntil = finiteDeadline(10);
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
    if (owner.contextAdvertisements)
        for (int id : owner.contextAdvertisements->consumeHostRefresh())
            if (auto interface = owner.ift->getInterfaceById(id)) startDiscovery(interface);
    if (owner.extendedRegistration) {
        std::vector<RouterKey> keys;
        for (const auto& router : routers) keys.push_back(router.first);
        for (const auto& key : keys) {
            auto current = routers.find(key);
            if (current == routers.end()) continue;
            auto interface = owner.ift->getInterfaceById(key.first);
            if (!interface || !interface->isUp() || current->second.validUntil <= simTime() || current->second.capabilityUntil <= simTime()) {
                removeRouter(key, true); continue;
            }
            auto address = current->second.pending;
            if (!address.isUnspecified()) {
                auto history = histories.find(historyKey({key.first, address}));
                if (history == histories.end() || !history->second.transaction || !history->second.transaction->copies.count(key))
                    finishCopy(key, address);
                else {
                    auto copy = history->second.transaction->copies.at(key);
                    auto source = current->second.registrations.find(copy.source);
                    bool refreshSource = copy.updated && copy.assignment.second != copy.source &&
                        source != current->second.registrations.end() && source->second.refreshAt <= simTime();
                    if (!validCopy(key, copy) || refreshSource) finishCopy(key, address);
                    else if (current->second.retryAt <= simTime()) {
                        if (current->second.attempts >= 3) {
                            coolDown(history->second, std::nullopt, false);
                            removeRouter(key, true); continue;
                        }
                        current->second.attempts++;
                        current->second.retryAt = registrationRetryDeadline(interface, current->second.attempts == 3);
                        sendExtendedRegistration(key, copy);
                    }
                }
            }
            current = routers.find(key);
            if (current != routers.end() && (current->second.informationRefreshAt <= simTime() ||
                    (current->second.authority && current->second.authorityUntil <= simTime()))) {
                current->second.informationRefreshAt = SIMTIME_MAX;
                startDiscovery(interface);
            }
        }
        startExtendedRegistrations();
        for (auto& discovery : discoveries)
            if (discovery.second.active && discovery.second.next <= simTime())
                if (auto interface = owner.ift->getInterfaceById(discovery.first)) sendRouterSolicitation(interface, discovery.second);
        scheduleTimer();
        return;
    }
    for (auto it = routers.begin(); it != routers.end();) {
        auto current = it++;
        auto key = current->first;
        auto& router = current->second;
        auto interface = owner.ift->getInterfaceById(key.first);
        if (!interface || router.validUntil <= simTime()) { removeRouter(key, true); continue; }
        if (!router.pending.isUnspecified() && router.retryAt <= simTime()) {
            if (router.attempts >= 3) { removeRouter(key, true); continue; }
            router.attempts++;
            router.retryAt = registrationRetryDeadline(interface, router.attempts == 3);
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
    if (owner.contextAdvertisements) {
        auto refresh = owner.contextAdvertisements->nextHostRefresh();
        if (refresh != SIMTIME_MAX) next = std::min(next, std::max(simTime(), refresh));
    }
    for (const auto& entry : discoveries) if (entry.second.active) include(entry.second.next);
    for (const auto& entry : routers) {
        include(entry.second.validUntil);
        include(entry.second.informationRefreshAt);
        if (owner.extendedRegistration) {
            include(entry.second.capabilityUntil);
            if (entry.second.authority) include(entry.second.authorityUntil);
        }
        include(entry.second.blockedUntil);
        if (!entry.second.pending.isUnspecified()) include(entry.second.retryAt);
        else for (const auto& address : entry.second.registrations) include(address.second.refreshAt);
        for (const auto& address : entry.second.registrations) {
            include(address.second.validUntil);
            if (owner.extendedRegistration && address.first.isLinkLocal()) include(address.second.refreshAt);
        }
    }
    for (const auto& address : addresses) if (!address.second.rejected) include(address.second.validUntil);
    if (owner.extendedRegistration)
        for (const auto& history : histories) {
            include(history.second.blockedUntil);
            for (const auto& group : history.second.authorityCooldowns) include(group.second);
        }
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
