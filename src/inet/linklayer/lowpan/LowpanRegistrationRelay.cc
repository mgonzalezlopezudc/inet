// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRegistrationRelay.h"

#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6RoutingTable.h"
#include <set>

namespace inet { namespace lowpan {
namespace {
simtime_t deadline(int seconds)
{
    return double(seconds) >= (SIMTIME_MAX - simTime()).dbl() ? SIMTIME_MAX : simTime() + SimTime(seconds);
}
}

LowpanRegistrationRelay::LowpanRegistrationRelay(LowpanIpv6NeighbourDiscovery& owner) : owner(owner),
    authorityAddress(owner.par("registrationAuthorityAddress").stringValue()),
    sourceAddress(owner.par("registrationSourceAddress").stringValue())
{
    if (!authorityAddress.isUnicast() || authorityAddress.isUnspecified() || authorityAddress.isLinkLocal() ||
            !sourceAddress.isUnicast() || sourceAddress.isUnspecified() || sourceAddress.isLinkLocal() || sourceAddress.isLoopback())
        throw cRuntimeError("Routed native registration requires a non-loopback non-link-local unicast source and a non-link-local unicast authority");
    timer = new cMessage("registrationRelayTimer");
}

LowpanRegistrationRelay::~LowpanRegistrationRelay()
{
    clear();
    owner.cancelAndDelete(timer);
}

bool LowpanRegistrationRelay::hasValidSource() const
{
    for (int i = 0; i < owner.ift->getNumInterfaces(); i++) {
        auto interface = owner.ift->getInterface(i);
        auto data = interface->findProtocolData<Ipv6InterfaceData>();
        if (interface->isUp() && data && data->hasValidAddress(sourceAddress, simTime()) && !data->isTentativeAddress(sourceAddress))
            return true;
    }
    return false;
}

void LowpanRegistrationRelay::start()
{
    if (!hasValidSource())
        throw cRuntimeError("Routed registration source must be currently assigned, non-tentative and valid on an up interface");
}

void LowpanRegistrationRelay::clear()
{
    owner.cancelEvent(timer);
    pending.clear();
}

bool LowpanRegistrationRelay::hasValidContext(const Key& key, const Pending& entry) const
{
    if (!hasValidSource() || entry.expiresAt <= simTime() || !entry.requested.domain)
        return false;
    auto interface = owner.ift->getInterfaceById(key.first);
    auto boundary = interface ? dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) : nullptr;
    if (!boundary || !interface->isUp() || interface->getId() != entry.requested.interfaceModuleId ||
            boundary->getLinkDomain() != entry.requested.domain.get())
        return false;
    auto domain = entry.requested.domain.get();
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    return domain->contains(boundary->getLinkAddress()) && domain->contains(entry.requested.owner) &&
        domain->contains(entry.requested.address) && data->hasValidAddress(entry.replyAddress, simTime()) &&
        !data->isTentativeAddress(entry.replyAddress);
}

size_t LowpanRegistrationRelay::occupiedKeys() const
{
    std::set<Key> keys;
    for (const auto& entry : owner.registrations)
        if (entry.second.validUntil > simTime())
            keys.insert(entry.first);
    for (const auto& entry : pending)
        if (entry.second.expiresAt > simTime())
            keys.insert(entry.first);
    return keys.size();
}

void LowpanRegistrationRelay::scheduleTimer()
{
    owner.cancelEvent(timer);
    auto next = SIMTIME_MAX;
    for (const auto& entry : pending)
        next = std::min({next, entry.second.expiresAt, entry.second.retryAt});
    if (next != SIMTIME_MAX)
        owner.scheduleAt(next, timer);
}

void LowpanRegistrationRelay::removeMatchingRegistration(const Key& key, const Ieee802154Address& requestedOwner)
{
    auto existing = owner.registrations.find(key);
    if (existing != owner.registrations.end() && existing->second.owner == requestedOwner)
        owner.removeRegistration(key);
}

void LowpanRegistrationRelay::sendRequest(const Key& key)
{
    auto found = pending.find(key);
    if (found == pending.end())
        return;
    if (!hasValidContext(key, found->second)) {
        pending.erase(found);
        return;
    }
    auto& entry = found->second;
    auto request = makeShared<Ipv6DuplicateAddressHeader>();
    request->setType(ICMPv6_DUPLICATE_ADDRESS_REQUEST);
    request->setRegisteredAddress(key.second);
    request->setRegistrationLifetime(entry.lifetime);
    request->setRegistrationOwnershipVerifierArraySize(8);
    for (int i = 0; i < 8; i++)
        request->setRegistrationOwnershipVerifier(i, entry.requested.owner.getInt() >> (8 * (7 - i)));
    auto packet = new Packet("native-duplicate-address-request");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, request, packet);
    packet->insertAtFront(request);
    auto addresses = packet->addTag<L3AddressReq>();
    addresses->setSrcAddress(sourceAddress);
    addresses->setDestAddress(authorityAddress);
    packet->addTag<HopLimitReq>()->setHopLimit(64);
    packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::icmpv6);
    entry.attempts++;
    entry.retryAt = deadline(1);
    owner.numSent++;
    owner.send(packet, "ipv6Out");
}

void LowpanRegistrationRelay::processRegistration(const Key& key, const Registration& requested,
        const Ipv6NdOption& aro, const Ipv6Address& replyAddress)
{
    int lifetime = (uint8_t(aro.getPaddingBytes(4)) << 8) | uint8_t(aro.getPaddingBytes(5));
    Pending incoming{requested, aro, replyAddress, lifetime, 0, deadline(20), simTime()};
    if (!hasValidContext(key, incoming))
        return;
    for (auto it = pending.begin(); it != pending.end();)
        if (it->second.expiresAt <= simTime()) it = pending.erase(it);
        else ++it;
    auto existing = owner.registrations.find(key);
    auto fixed = requested.domain->resolveStaticNeighbor(requested.interfaceModuleId, key.second);
    if (owner.rt6->isLocalAddress(key.second) || (existing != owner.registrations.end() && existing->second.owner != requested.owner) ||
            (fixed && *fixed != requested.address)) {
        owner.sendRegistrationAdvertisement(key, requested, aro, 1, replyAddress);
        return;
    }
    auto found = pending.find(key);
    if (found != pending.end()) {
        auto& entry = found->second;
        if (entry.requested.owner != requested.owner || (entry.lifetime == 0 && lifetime != 0))
            return;
        bool supersede = entry.lifetime != 0 && lifetime == 0;
        entry.requested = requested;
        entry.aro = aro;
        entry.replyAddress = replyAddress;
        entry.lifetime = lifetime;
        if (supersede) {
            removeMatchingRegistration(key, requested.owner);
            entry.attempts = 0; // A one-way removal phase, still within the original tentative birth.
            sendRequest(key);
        }
    }
    else {
        if (existing == owner.registrations.end() && occupiedKeys() >= size_t(owner.maxRegistrations)) {
            owner.sendRegistrationAdvertisement(key, requested, aro, 2, replyAddress);
            return;
        }
        pending.emplace(key, incoming);
        if (lifetime == 0)
            removeMatchingRegistration(key, requested.owner);
        sendRequest(key);
    }
    scheduleTimer();
}

void LowpanRegistrationRelay::complete(const Key& key, int status)
{
    auto found = pending.find(key);
    if (found == pending.end())
        return;
    auto entry = found->second;
    pending.erase(found); // Detach before route/counter/reply notifications.
    if (!hasValidContext(key, entry))
        return;
    if (status != 0)
        removeMatchingRegistration(key, entry.requested.owner);
    else {
        entry.requested.validUntil = deadline(60 * entry.lifetime);
        status = owner.applyRegistration(key, entry.requested);
    }
    owner.scheduleRegistrationExpiry();
    owner.sendRegistrationAdvertisement(key, entry.requested, entry.aro, status, entry.replyAddress);
}

void LowpanRegistrationRelay::handleTimer()
{
    for (auto it = pending.begin(); it != pending.end();) {
        auto current = it++;
        auto key = current->first;
        if (!hasValidContext(key, current->second))
            pending.erase(current);
        else if (current->second.retryAt <= simTime()) {
            if (current->second.attempts >= 3)
                complete(key, 0); // RFC 6775 section 8.2.6: success without confirmed uniqueness.
            else
                sendRequest(key);
        }
    }
    scheduleTimer();
}

bool LowpanRegistrationRelay::hasValidOptions(const Packet *packet, b offset) const
{
    auto remaining = packet->getDataLength() - offset;
    if (remaining < b(0)) return false;
    if (remaining == b(0)) return true;
    auto bytes = packet->peekDataAt<BytesChunk>(offset, remaining)->getBytes();
    for (size_t i = 0; i < bytes.size();) {
        if (bytes.size() - i < 2 || bytes[i + 1] == 0 || size_t(bytes[i + 1]) * 8 > bytes.size() - i) return false;
        i += size_t(bytes[i + 1]) * 8;
    }
    return true;
}

bool LowpanRegistrationRelay::processPacket(Packet *packet, const Icmpv6Header *message)
{
    auto reply = dynamic_cast<const Ipv6DuplicateAddressHeader *>(message);
    if (!reply || reply->getType() != ICMPv6_DUPLICATE_ADDRESS_CONFIRMATION)
        return false;
    auto addresses = packet->findTag<L3AddressInd>();
    if (!addresses || addresses->getSrcAddress() != L3Address(authorityAddress) ||
            addresses->getDestAddress() != L3Address(sourceAddress)) {
        delete packet;
        return true;
    }
    bool valid = !reply->isIncorrect() && !reply->isIncomplete() &&
        (reply->getCode() == 0 || reply->getCode() == 1) && reply->getRegistrationOwnershipVerifierArraySize() == 8 &&
        reply->getChunkLength() == B(32) && !reply->getRegisteredAddress().isMulticast() &&
        hasValidOptions(packet, B(32)) && Icmpv6::verifyChecksum(packet);
    if (valid) {
        uint64_t requestedOwner = 0;
        for (int i = 0; i < 8; i++) requestedOwner = (requestedOwner << 8) | uint8_t(reply->getRegistrationOwnershipVerifier(i));
        std::vector<Key> matches;
        for (const auto& entry : pending)
            if (entry.first.second == reply->getRegisteredAddress() && entry.second.requested.owner.getInt() == requestedOwner &&
                    entry.second.lifetime == reply->getRegistrationLifetime())
                matches.push_back(entry.first);
        for (const auto& key : matches)
            complete(key, reply->getStatus());
    }
    else {
        LowpanPacketDropDetails details;
        details.setReason(OTHER_PACKET_DROP);
        details.setLowpanReason((reply->getCode() != 0 && reply->getCode() != 1) ||
            reply->getRegistrationOwnershipVerifierArraySize() != 8 ? LOWPAN_ND_UNSUPPORTED : LOWPAN_MALFORMED);
        owner.emit(packetDroppedSignal, packet, &details);
    }
    delete packet;
    scheduleTimer();
    return true;
}

} }
