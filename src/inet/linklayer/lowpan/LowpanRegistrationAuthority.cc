// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRegistrationAuthority.h"

#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"

namespace inet { namespace lowpan {

LowpanRegistrationAuthority::LowpanRegistrationAuthority(LowpanIpv6NeighbourDiscovery& owner) : owner(owner)
{
    auto configuredCapacity = owner.par("maxDuplicateAddressEntries").intValue();
    if (configuredCapacity <= 0)
        throw cRuntimeError("Duplicate-address authority capacity must be positive");
    capacity = configuredCapacity;
    expiryTimer = new cMessage("duplicateAddressExpiry");
}

LowpanRegistrationAuthority::~LowpanRegistrationAuthority()
{
    clear(false);
    owner.cancelAndDelete(expiryTimer);
}

void LowpanRegistrationAuthority::clear(bool notify)
{
    owner.cancelEvent(expiryTimer);
    bool changed = !entries.empty();
    entries.clear();
    if (notify && changed)
        owner.emit(owner.duplicateAddressEntryCountChangedSignal, 0L);
}

bool LowpanRegistrationAuthority::isLocalAuthorityAddress(const Ipv6Address& address) const
{
    if (!address.isUnicast() || address.isUnspecified() || address.isLinkLocal())
        return false;
    for (int i = 0; i < owner.ift->getNumInterfaces(); i++) {
        auto interface = owner.ift->getInterface(i);
        auto data = interface->findProtocolData<Ipv6InterfaceData>();
        if (interface->isUp() && data && data->hasValidAddress(address, simTime()) && !data->isTentativeAddress(address))
            return true;
    }
    return false;
}

bool LowpanRegistrationAuthority::hasValidOptions(const Packet *packet, b offset) const
{
    auto remaining = packet->getDataLength() - offset;
    if (remaining < b(0))
        return false;
    if (remaining == b(0))
        return true;
    auto bytes = packet->peekDataAt<BytesChunk>(offset, remaining)->getBytes();
    for (size_t i = 0; i < bytes.size();) {
        if (bytes.size() - i < 2 || bytes[i + 1] == 0 || size_t(bytes[i + 1]) * 8 > bytes.size() - i)
            return false;
        i += size_t(bytes[i + 1]) * 8;
    }
    return true;
}

void LowpanRegistrationAuthority::expireEntries(bool notify)
{
    bool changed = false;
    for (auto it = entries.begin(); it != entries.end();) {
        if (owner.extendedRegistration) {
            auto& entry = it->second;
            entry.validUntil = SIMTIME_ZERO;
            for (auto location = entry.locations.begin(); location != entry.locations.end();)
                if (std::max(location->second.validUntil, location->second.removalUntil) <= simTime()) location = entry.locations.erase(location);
                else { entry.validUntil = std::max({entry.validUntil, location->second.validUntil, location->second.removalUntil}); ++location; }
        }
        if (it->second.validUntil <= simTime()) {
            it = entries.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    if (changed && notify)
        publishEntryCount();
}

int LowpanRegistrationAuthority::checkRegistration(const Ipv6Address& address,
        const std::array<uint8_t, 8>& requestedOwner, bool remove) const
{
    uint64_t ownerValue = 0;
    for (auto byte : requestedOwner)
        ownerValue = (ownerValue << 8) | byte;
    auto nativeOwner = Ieee802154Address(Ieee802154Address::EXTENDED, ownerValue);
    // In the combined 6LBR model, a withdrawn DAD lease does not invalidate an
    // independently live local NCE. Consult it without changing it from DAR.
    for (const auto& registration : owner.registrations)
        if (registration.first.second == address && registration.second.validUntil > simTime() &&
                registration.second.owner != nativeOwner)
            return 1;
    auto existing = entries.find(address);
    bool live = existing != entries.end() && existing->second.validUntil > simTime();
    if (live && existing->second.owner != requestedOwner)
        return 1;
    if (!live && !remove) {
        size_t used = 0;
        for (const auto& entry : entries)
            used += entry.second.validUntil > simTime();
        if (used >= capacity)
            return 2;
    }
    return 0;
}

bool LowpanRegistrationAuthority::commitRegistration(const Ipv6Address& address,
        const std::array<uint8_t, 8>& requestedOwner, simtime_t validUntil)
{
    auto oldSize = entries.size();
    expireEntries(false);
    if (validUntil <= simTime())
        entries.erase(address);
    else
        entries[address] = {requestedOwner, validUntil};
    scheduleExpiry();
    return entries.size() != oldSize;
}

void LowpanRegistrationAuthority::publishEntryCount()
{
    owner.emit(owner.duplicateAddressEntryCountChangedSignal, long(entries.size()));
}

void LowpanRegistrationAuthority::scheduleExpiry()
{
    owner.cancelEvent(expiryTimer);
    auto next = SIMTIME_MAX;
    for (const auto& entry : entries)
        next = std::min(next, entry.second.validUntil);
    if (next != SIMTIME_MAX)
        owner.scheduleAt(next, expiryTimer);
}

void LowpanRegistrationAuthority::handleTimer()
{
    expireEntries();
    scheduleExpiry();
}

void LowpanRegistrationAuthority::drop(Packet *packet, bool unsupported)
{
    LowpanPacketDropDetails details;
    details.setReason(OTHER_PACKET_DROP);
    details.setLowpanReason(unsupported ? LOWPAN_ND_UNSUPPORTED : LOWPAN_MALFORMED);
    owner.emit(packetDroppedSignal, packet, &details);
    delete packet;
}

void LowpanRegistrationAuthority::sendConfirmation(const Ipv6DuplicateAddressHeader& request,
        const Ipv6Address& source, const Ipv6Address& destination, int status)
{
    // RFC 6775 sections 4.4 and 8.2.4: base-format reply, with routing rather than single-hop ND pinning.
    auto reply = makeShared<Ipv6DuplicateAddressHeader>();
    reply->setType(ICMPv6_DUPLICATE_ADDRESS_CONFIRMATION);
    reply->setStatus(status);
    reply->setRegistrationLifetime(request.getRegistrationLifetime());
    reply->setRegisteredAddress(request.getRegisteredAddress());
    reply->setRegistrationOwnershipVerifierArraySize(8);
    if (owner.extendedRegistration) {
        reply->setCode(request.getRegistrationOwnershipVerifierArraySize() / 8);
        reply->setTransactionId(request.getCode() == 0 ? 0 : request.getTransactionId());
        reply->setRegistrationOwnershipVerifierArraySize(request.getRegistrationOwnershipVerifierArraySize());
        reply->setChunkLength(B(24 + request.getRegistrationOwnershipVerifierArraySize()));
    }
    for (size_t i = 0; i < reply->getRegistrationOwnershipVerifierArraySize(); i++)
        reply->setRegistrationOwnershipVerifier(i, request.getRegistrationOwnershipVerifier(i));
    auto output = new Packet("duplicate-address-confirmation");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, reply, output);
    output->insertAtFront(reply);
    auto addresses = output->addTag<L3AddressReq>();
    addresses->setSrcAddress(source);
    addresses->setDestAddress(destination);
    output->addTag<HopLimitReq>()->setHopLimit(64);
    output->addTag<PacketProtocolTag>()->setProtocol(&Protocol::icmpv6);
    owner.numSent++;
    owner.send(output, "ipv6Out");
}

bool LowpanRegistrationAuthority::processPacket(Packet *packet, const Icmpv6Header *message)
{
    auto request = dynamic_cast<const Ipv6DuplicateAddressHeader *>(message);
    if (!request || request->getType() != ICMPv6_DUPLICATE_ADDRESS_REQUEST)
        return false;
    auto addresses = packet->findTag<L3AddressInd>();
    bool valid = !request->isIncorrect() && !request->isIncomplete() && addresses &&
        addresses->getSrcAddress().getType() == L3Address::IPv6 && addresses->getDestAddress().getType() == L3Address::IPv6 &&
        addresses->getSrcAddress().toIpv6().isUnicast() && !addresses->getSrcAddress().toIpv6().isUnspecified() &&
        !addresses->getSrcAddress().toIpv6().isLinkLocal() && isLocalAuthorityAddress(addresses->getDestAddress().toIpv6()) &&
        !request->getRegisteredAddress().isMulticast() && Icmpv6::verifyChecksum(packet);
    if (!valid) {
        drop(packet);
        return true;
    }
    if (owner.extendedRegistration) return processExtended(packet, *request);
    // Selected legacy interoperability follows RFC 6775 section 4.4 and RFC 8505 section 6.4.
    // Code Prefix is already ignored by the wire decoder; compatible 64-bit EDAR ignores TID here.
    if (request->getRegistrationOwnershipVerifierArraySize() != 8 || (request->getCode() != 0 && request->getCode() != 1)) {
        drop(packet, true);
        return true;
    }
    if (request->getChunkLength() != B(32) || !hasValidOptions(packet, request->getChunkLength())) {
        drop(packet);
        return true;
    }
    expireEntries();
    auto address = request->getRegisteredAddress();
    std::array<uint8_t, 8> requestedOwner;
    for (int i = 0; i < 8; i++)
        requestedOwner[i] = request->getRegistrationOwnershipVerifier(i);
    // RFC 6775 section 8.2.4: a different owner cannot refresh or remove the incumbent.
    int status = checkRegistration(address, requestedOwner, request->getRegistrationLifetime() == 0);
    if (status == 0) {
        int seconds = 60 * int(request->getRegistrationLifetime());
        auto deadline = double(seconds) >= (SIMTIME_MAX - simTime()).dbl() ? SIMTIME_MAX : simTime() + SimTime(seconds);
        if (commitRegistration(address, requestedOwner, deadline))
            publishEntryCount();
    }
    scheduleExpiry();
    sendConfirmation(*request, addresses->getDestAddress().toIpv6(), addresses->getSrcAddress().toIpv6(), status);
    delete packet;
    return true;
}

int LowpanRegistrationAuthority::checkExtended(const Ipv6Address& address, const LowpanRegistrationInfo& info,
        const LocationKey& location, bool remove) const
{
    auto existing = entries.find(address);
    bool live = existing != entries.end() && existing->second.validUntil > simTime();
    auto local = owner.registrationTransactions.find({-1, address});
    const auto *current = live ? &existing->second.info :
        local != owner.registrationTransactions.end() ? &local->second : nullptr;
    for (const auto& registration : owner.registrations)
        if (registration.first.second == address && registration.second.validUntil > simTime() &&
                !registration.second.info.sameOwner(info)) return 1;
    if (current && !current->sameOwner(info)) return 1;
    for (const auto& registration : owner.registrations)
        if (registration.first.second == address && registration.second.validUntil > simTime() &&
                registration.second.info.tidKnown && !info.tidKnown) return 3;
    if (current) {
        if (current->tidKnown && !info.tidKnown) return 3;
        if (current->tidKnown && info.tidKnown) {
            auto comparison = LowpanRegistrationInfo::compare(info.tid, current->tid);
            if (comparison == LowpanRegistrationInfo::OLDER || comparison == LowpanRegistrationInfo::INCOMPARABLE) return 3;
        }
    }
    if (live) {
        const auto& entry = existing->second;
        auto previous = entry.locations.find(location);
        bool present = previous != entry.locations.end() &&
            std::max(previous->second.validUntil, previous->second.removalUntil) > simTime();
        if (present && previous->second.removalUntil > simTime() && !remove && previous->second.info.sameTransaction(info)) return 3;
        size_t used = 0;
        for (const auto& item : entry.locations)
            used += std::max(item.second.validUntil, item.second.removalUntil) > simTime() &&
                (remove || item.second.info.sameTransaction(info));
        bool retained = present && (remove || previous->second.info.sameTransaction(info));
        if (!retained && used >= size_t(owner.maxRegistrationLocations)) return 9;
    }
    else {
        size_t used = 0;
        for (const auto& entry : entries) used += entry.second.validUntil > simTime();
        if (used >= capacity) return 9;
    }
    return 0;
}

LowpanRegistrationAuthority::Notifications LowpanRegistrationAuthority::commitExtended(const Ipv6Address& address,
        const LowpanRegistrationInfo& info, const LocationKey& location, const Location& requested)
{
    expireEntries(false);
    Notifications superseded;
    auto& entry = entries[address];
    if (requested.lifetimeMinutes != 0) {
        for (auto old = entry.locations.begin(); old != entry.locations.end();)
            if (!old->second.info.sameTransaction(info)) {
                if (old->second.validUntil > simTime() && (old->first != location || old->second.nativePeer != requested.nativePeer))
                    superseded.push_back(*old);
                old = entry.locations.erase(old);
            }
            else ++old;
    }
    auto previous = entry.locations.find(location);
    bool repeatRemoval = previous != entry.locations.end() && previous->second.removalUntil > simTime() &&
        previous->second.info.sameTransaction(info) && requested.lifetimeMinutes == 0;
    entry.info = info;
    if (!repeatRemoval) {
        auto value = requested;
        value.info = info;
        if (requested.lifetimeMinutes == 0) {
            value.validUntil = simTime();
            value.removalUntil = owner.removalDelaySeconds >= (SIMTIME_MAX - simTime()).dbl() ? SIMTIME_MAX :
                simTime() + SimTime(owner.removalDelaySeconds);
        }
        else value.removalUntil = SIMTIME_ZERO;
        entry.locations[location] = value;
    }
    entry.validUntil = SIMTIME_ZERO;
    for (const auto& item : entry.locations)
        entry.validUntil = std::max({entry.validUntil, item.second.validUntil, item.second.removalUntil});
    auto local = owner.registrationTransactions.find({-1, address});
    if (local != owner.registrationTransactions.end()) local->second = info;
    scheduleExpiry();
    return superseded;
}

void LowpanRegistrationAuthority::notifyMoved(const Ipv6Address& address, const Notifications& notifications)
{
    for (const auto& item : notifications) {
        if (item.first.first >= 0) {
            auto interface = owner.ift->getInterfaceById(item.first.first);
            auto boundary = interface ? dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")) : nullptr;
            LowpanIpv6NeighbourDiscovery::Registration saved;
            saved.info = item.second.info; saved.address = item.second.nativePeer; saved.peerSource = item.first.second;
            saved.replySource = item.second.replySource; saved.lifetimeMinutes = item.second.lifetimeMinutes;
            saved.interfaceModuleId = item.second.interfaceModuleId;
            if (boundary && interface->getId() == saved.interfaceModuleId) saved.domain = boundary->getLinkDomain();
            owner.notifyNativeMoved({item.first.first, address}, saved);
            continue;
        }
        // Unknown provenance has no distinct wire correlation from a genuine TID zero.
        if (!item.second.info.tidKnown || !isLocalAuthorityAddress(item.second.replySource)) continue;
        Ipv6DuplicateAddressHeader correlation;
        correlation.setCode(item.second.info.rovr.size() / 8);
        correlation.setTransactionId(item.second.info.tid);
        correlation.setRegisteredAddress(address);
        correlation.setRegistrationLifetime(item.second.lifetimeMinutes);
        correlation.setRegistrationOwnershipVerifierArraySize(item.second.info.rovr.size());
        for (size_t i = 0; i < item.second.info.rovr.size(); i++) correlation.setRegistrationOwnershipVerifier(i, item.second.info.rovr[i]);
        sendConfirmation(correlation, item.second.replySource, item.first.second, 3);
    }
}

bool LowpanRegistrationAuthority::processExtended(Packet *packet, const Ipv6DuplicateAddressHeader& request)
{
    size_t width = request.getRegistrationOwnershipVerifierArraySize();
    bool valid = (request.getCode() == 0 && width == 8) ||
        (request.getCode() >= 1 && request.getCode() <= 4 && width == size_t(request.getCode() * 8));
    if (!valid || request.getChunkLength() != B(24 + width) || !hasValidOptions(packet, request.getChunkLength())) {
        drop(packet, !valid); return true;
    }
    expireEntries();
    owner.pruneRegistrationTransactions();
    auto addresses = packet->getTag<L3AddressInd>();
    LocationKey location{-1, addresses->getSrcAddress().toIpv6()};
    LowpanRegistrationInfo info;
    info.tidKnown = request.getCode() != 0;
    info.tid = info.tidKnown ? request.getTransactionId() : 0;
    for (size_t i = 0; i < width; i++) info.rovr.push_back(request.getRegistrationOwnershipVerifier(i));
    int status = checkExtended(request.getRegisteredAddress(), info, location, request.getRegistrationLifetime() == 0);
    Notifications notifications;
    if (status == 0) {
        Location value;
        value.info = info; value.replySource = addresses->getDestAddress().toIpv6(); value.lifetimeMinutes = request.getRegistrationLifetime();
        double seconds = 60.0 * value.lifetimeMinutes;
        value.validUntil = seconds >= (SIMTIME_MAX - simTime()).dbl() ? SIMTIME_MAX : simTime() + SimTime(seconds);
        notifications = commitExtended(request.getRegisteredAddress(), info, location, value);
        publishEntryCount();
    }
    sendConfirmation(request, addresses->getDestAddress().toIpv6(), addresses->getSrcAddress().toIpv6(), status);
    notifyMoved(request.getRegisteredAddress(), notifications);
    delete packet;
    return true;
}

} }
