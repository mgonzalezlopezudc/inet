// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanContextAdvertisements.h"

#include "inet/linklayer/lowpan/LowpanHostRegistration.h"
#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"
#include "inet/linklayer/lowpan/LowpanLayer.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include <cmath>

namespace inet { namespace lowpan {
namespace {
simtime_t deadline(double seconds)
{
    return seconds >= (SIMTIME_MAX - simTime()).dbl() ? SIMTIME_MAX : simTime() + SimTime(seconds);
}

simtime_t prefixDeadline(uint32_t seconds)
{
    return seconds == UINT32_MAX ? SIMTIME_MAX : deadline(seconds);
}

uint32_t secondsLeft(double seconds, simtime_t reference, bool unlimited)
{
    return unlimited ? UINT32_MAX : uint32_t(std::min(double(UINT32_MAX - 1),
        std::floor(std::max(0.0, seconds - (simTime() - reference).dbl()))));
}

Ipv6Address readAddress(const Ipv6NdOption& option, int offset, int bytes)
{
    uint32_t words[4]{};
    for (int i = 0; i < bytes; i++)
        words[i / 4] |= uint32_t(uint8_t(option.getPaddingBytes(offset + i))) << (24 - 8 * (i % 4));
    return Ipv6Address(words[0], words[1], words[2], words[3]);
}

void writeAddress(Ipv6NdOption& option, int offset, const Ipv6Address& address, int bytes)
{
    for (int i = 0; i < bytes; i++)
        option.setPaddingBytes(offset + i, address.words()[i / 4] >> (24 - 8 * (i % 4)));
}

Ipv6NdOption *rawOption(int type, int length)
{
    auto option = new Ipv6NdOption();
    option->setType(static_cast<Ipv6NdOptionTypes>(type));
    option->setOptionLength(length);
    option->setPaddingBytesArraySize(length * 8 - 2);
    for (size_t i = 0; i < option->getPaddingBytesArraySize(); i++) option->setPaddingBytes(i, 0);
    return option;
}

void snapshotNumber(std::vector<uint8_t>& snapshot, uint64_t value)
{
    for (int i = 7; i >= 0; i--) snapshot.push_back(value >> (8 * i));
}

void snapshotAddress(std::vector<uint8_t>& snapshot, const Ipv6Address& address)
{
    for (int i = 0; i < 4; i++) snapshotNumber(snapshot, address.words()[i]);
}
}

LowpanContextAdvertisements::LowpanContextAdvertisements(LowpanIpv6NeighbourDiscovery& owner) : owner(owner)
{
    auto role = owner.par("nativeNdRole").stdstringValue();
    host = role == "host";
    origin = role == "borderRouter";
    if (!host && !owner.registrationRouter)
        throw cRuntimeError("Context distribution requires native host, router or borderRouter role");
    maxGroups = owner.par("maxContextAuthorityGroups");
    if (maxGroups <= 0)
        throw cRuntimeError("Context authority group capacity must be positive");
    refreshFraction = owner.par("nativeRegistrationRefreshFraction");
    if (!(refreshFraction > 0 && refreshFraction < 1))
        throw cRuntimeError("Context information refresh fraction must be strictly between zero and one");
    if (origin) {
        originAddress = Ipv6Address(owner.par("contextAuthorityAddress").stringValue());
        auto version = owner.par("contextInitialVersion").intValue();
        auto lifetime = owner.par("contextAuthorityLifetime").intValue();
        if (!originAddress.isUnicast() || originAddress.isUnspecified() || originAddress.isLinkLocal() ||
                originAddress.isLoopback() || version < 0 || uint64_t(version) > UINT32_MAX || lifetime < 0 || lifetime > 65535)
            throw cRuntimeError("Invalid context authority address, unsigned version or minute lifetime");
        originVersion = version;
        originLifetime = lifetime;
    }
    maintenanceTimer = new cMessage("nativeContextMaintenance");
}

LowpanContextAdvertisements::~LowpanContextAdvertisements()
{
    clear();
    owner.cancelAndDelete(maintenanceTimer);
}

std::vector<NetworkInterface *> LowpanContextAdvertisements::interfaces() const
{
    std::vector<NetworkInterface *> result;
    for (int i = 0; i < owner.ift->getNumInterfaces(); i++) {
        auto interface = owner.ift->getInterface(i);
        if (interface->getProtocol() == &lowpanProtocol &&
                dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary")))
            result.push_back(interface);
    }
    return result;
}

LowpanLayer *LowpanContextAdvertisements::layer(NetworkInterface *interface) const
{
    return check_and_cast<LowpanLayer *>(interface->getSubmodule("lowpan"));
}

LowpanContextAdvertisements::Group LowpanContextAdvertisements::parse(const Ipv6RouterAdvertisement& message,
        bool& authoritative, Ipv6Address& authority) const
{
    Group result;
    authoritative = false;
    const auto& options = message.getOptions();
    // Parse ABRO first; option ordering has no association semantics.
    for (size_t i = 0; i < options.getOptionArraySize(); i++) {
        auto option = options.getOption(i);
        if (int(option->getType()) != 35) continue;
        if (authoritative || option->getOptionLength() != 3 || option->getPaddingBytesArraySize() != 22)
            throw cRuntimeError("Malformed or duplicate native ABRO");
        authoritative = true;
        for (int j = 0; j < 22; j++) result.abro[j] = option->getPaddingBytes(j);
        result.version = (uint32_t(result.abro[2]) << 24) | (uint32_t(result.abro[3]) << 16) |
            (uint32_t(result.abro[0]) << 8) | result.abro[1];
        unsigned minutes = (unsigned(result.abro[4]) << 8) | result.abro[5];
        double seconds = 60.0 * (minutes == 0 ? 10000 : minutes);
        result.validUntil = deadline(seconds);
        result.refreshAt = deadline(seconds * refreshFraction);
        authority = readAddress(*option, 6, 16);
        if (!authority.isUnicast() || authority.isUnspecified() || authority.isLoopback())
            throw cRuntimeError("Invalid native ABRO authority");
    }
    for (size_t i = 0; i < options.getOptionArraySize(); i++) {
        auto option = options.getOption(i);
        if (int(option->getType()) == 34) {
            int length = option->getOptionLength();
            if ((length != 2 && length != 3) || int(option->getPaddingBytesArraySize()) != length * 8 - 2)
                throw cRuntimeError("Malformed native 6CO");
            int bits = uint8_t(option->getPaddingBytes(0));
            int flags = uint8_t(option->getPaddingBytes(1));
            int id = flags & 15;
            if (bits > 128 || (bits > 64 && length != 3) || result.contexts.count(id))
                throw cRuntimeError("Invalid or duplicate native context ID");
            auto prefix = readAddress(*option, 6, length == 2 ? 8 : 16).getPrefix(bits);
            if (prefix.isMulticast()) throw cRuntimeError("Multicast native context prefix");
            unsigned minutes = (unsigned(uint8_t(option->getPaddingBytes(4))) << 8) | uint8_t(option->getPaddingBytes(5));
            double seconds = 60.0 * minutes;
            auto compressionUntil = minutes == 0 ? simTime() : std::min(result.validUntil, deadline(seconds));
            auto validUntil = minutes == 0 ? simTime() :
                std::min(result.validUntil, deadline(seconds + 2.0 * message.getRouterLifetime()));
            result.contexts[id] = {prefix, bits, bool(flags & 16), compressionUntil, validUntil,
                minutes == 0 ? SIMTIME_MAX : std::min(result.refreshAt, deadline(seconds * refreshFraction))};
            result.contexts[id].withdrawal = minutes == 0;
            result.contexts[id].lifetimeSeconds = seconds;
            result.contexts[id].receivedAt = simTime();
        }
        else if (auto prefix = dynamic_cast<const Ipv6NdPrefixInformation *>(option)) {
            if (prefix->getPrefixLength() > 128 || prefix->getPrefix().isMulticast() ||
                    prefix->getPreferredLifetime() > prefix->getValidLifetime())
                throw cRuntimeError("Invalid authoritative PIO");
            result.prefixes.push_back({prefix->getPrefix().getPrefix(prefix->getPrefixLength()), prefix->getPrefixLength(),
                prefix->getAutoAddressConfFlag(), std::min(result.validUntil, prefixDeadline(prefix->getValidLifetime())),
                std::min(result.validUntil, prefixDeadline(prefix->getPreferredLifetime())),
                prefix->getValidLifetime() == 0 || prefix->getValidLifetime() == UINT32_MAX ? SIMTIME_MAX :
                    deadline(double(prefix->getValidLifetime()) * refreshFraction)});
            auto& stored = result.prefixes.back();
            stored.validSeconds = prefix->getValidLifetime(); stored.preferredSeconds = prefix->getPreferredLifetime();
            stored.receivedAt = simTime();
            stored.validUnlimited = prefix->getValidLifetime() == UINT32_MAX;
            stored.preferredUnlimited = prefix->getPreferredLifetime() == UINT32_MAX;
        }
    }
    return result;
}

bool LowpanContextAdvertisements::preflight(const GroupKey& key, const Group& group, NetworkInterface *ingress) const
{
    auto previous = groups.find(key);
    if (previous != groups.end() && previous->second.validUntil > simTime() && group.version < previous->second.version)
        return false;
    int live = 0;
    for (const auto& entry : groups) live += entry.second.validUntil > simTime();
    bool retainsInformation = std::get<1>(key);
    for (const auto& context : group.contexts) retainsInformation |= context.second.validUntil > simTime();
    for (const auto& prefix : group.prefixes) retainsInformation |= prefix.validUntil > simTime();
    if (retainsInformation && (previous == groups.end() || previous->second.validUntil <= simTime()) && live >= maxGroups)
        return false;
    auto targets = host ? std::vector<NetworkInterface *>{ingress} : interfaces();
    for (auto interface : targets) {
        auto adaptation = layer(interface);
        for (const auto& context : group.contexts) {
            if (adaptation->isConfiguredContext(context.first)) return false;
            if (context.second.withdrawal) continue; // Withdrawal carries no new prefix interpretation.
            for (const auto& other : groups) {
                if (other.first == key || other.second.validUntil <= simTime() ||
                        (host && std::get<0>(other.first) != interface->getInterfaceId())) continue;
                auto existing = other.second.contexts.find(context.first);
                if (existing != other.second.contexts.end() && existing->second.validUntil > simTime() &&
                        (existing->second.prefix != context.second.prefix || existing->second.length != context.second.length))
                    return false;
            }
        }
    }
    return true;
}

void LowpanContextAdvertisements::install(const GroupKey& key, const Group& group, NetworkInterface *ingress)
{
    auto targets = host ? std::vector<NetworkInterface *>{ingress} : interfaces();
    for (auto interface : targets) {
        auto adaptation = layer(interface);
        for (const auto& context : group.contexts) {
            const auto& value = context.second;
            if (value.withdrawal)
                adaptation->removeLearnedContext(context.first);
            else
                adaptation->setLearnedContext(context.first, value.prefix, value.length, value.compress,
                    value.compressionUntil, value.validUntil);
            effectiveOwners[{interface->getInterfaceId(), context.first}] = key;
        }
    }
}

void LowpanContextAdvertisements::removeGroup(const GroupKey& key)
{
    for (auto it = effectiveOwners.begin(); it != effectiveOwners.end();) {
        if (it->second != key) { ++it; continue; }
        if (auto interface = owner.ift->getInterfaceById(it->first.first))
            layer(interface)->removeLearnedContext(it->first.second);
        it = effectiveOwners.erase(it);
    }
    groups.erase(key);
}

void LowpanContextAdvertisements::expire()
{
    for (auto it = groups.begin(); it != groups.end();) {
        auto current = it++;
        if (current->second.validUntil <= simTime()) removeGroup(current->first);
    }
    // The codec enforces both cutoffs directly. Do not reinstall older source snapshots here.
}

bool LowpanContextAdvertisements::accept(const Ipv6RouterAdvertisement& message, NetworkInterface *interface,
        const Ipv6Address& source)
{
    bool authoritative;
    Ipv6Address authority;
    Group received;
    try { received = parse(message, authoritative, authority); }
    catch (const cRuntimeError&) { return false; }
    if (!host && !authoritative) return false;
    if (!authoritative && received.contexts.empty() && received.prefixes.empty()) return true;
    GroupKey key{host ? interface->getInterfaceId() : -1, authoritative, authoritative ? authority : source};
    if (!preflight(key, received, interface)) return false;
    expire();
    auto previous = groups.find(key);
    bool changed = previous == groups.end() || received.version > previous->second.version;
    if (authoritative && previous != groups.end()) {
        // Complete snapshots withdraw omitted CIDs only while this group still owns them.
        for (const auto& context : previous->second.contexts)
            if (!received.contexts.count(context.first))
                for (auto it = effectiveOwners.begin(); it != effectiveOwners.end();) {
                    if (it->first.second == context.first && it->second == key) {
                        if (auto target = owner.ift->getInterfaceById(it->first.first)) layer(target)->removeLearnedContext(context.first);
                        it = effectiveOwners.erase(it);
                    }
                    else ++it;
                }
    }
    Group stored = received;
    if (!authoritative && previous != groups.end()) {
        stored = previous->second;
        for (const auto& context : received.contexts) stored.contexts[context.first] = context.second;
        stored.prefixes = received.prefixes;
    }
    if (!authoritative) {
        stored.validUntil = simTime();
        for (const auto& context : stored.contexts) stored.validUntil = std::max(stored.validUntil, context.second.validUntil);
        for (const auto& prefix : stored.prefixes) stored.validUntil = std::max(stored.validUntil, prefix.validUntil);
    }
    if (stored.validUntil > simTime()) groups[key] = stored;
    install(key, received, interface);
    if (stored.validUntil <= simTime()) removeGroup(key);
    if (!host) {
        auto& discovery = discoveries[interface->getInterfaceId()];
        discovery.active = false;
        discovery.next = SIMTIME_MAX;
        if (changed) trigger();
    }
    scheduleTimer();
    return true;
}

void LowpanContextAdvertisements::processAdvertisement(Packet *packet, const Ipv6RouterAdvertisement *message,
        NetworkInterface *interface, LowpanNativeLinkDomain *domain)
{
    Ieee802154Address native;
    auto addresses = packet->findTag<L3AddressInd>();
    bool valid = !message->isIncorrect() && !message->isIncomplete() && message->getCode() == 0 &&
        message->getChunkLength() == packet->getDataLength() && addresses &&
        addresses->getSrcAddress().toIpv6().isLinkLocal() && !message->getManagedAddrConfFlag() &&
        owner.readNativeSource(packet, message->getOptions(), domain, native);
    if (!valid || !accept(*message, interface, addresses->getSrcAddress().toIpv6())) {
        LowpanPacketDropDetails details;
        details.setReason(OTHER_PACKET_DROP);
        details.setLowpanReason(valid ? LOWPAN_ND_UNSUPPORTED : LOWPAN_MALFORMED);
        owner.emit(packetDroppedSignal, packet, &details);
    }
    delete packet;
}

void LowpanContextAdvertisements::updateOrigin()
{
    if (!origin) return;
    std::vector<uint8_t> snapshot;
    Group group;
    bool first = true;
    for (auto interface : interfaces()) {
        auto data = interface->getProtocolData<Ipv6InterfaceData>();
        if (!data->getAdvSendAdvertisements()) continue;
        std::vector<uint8_t> candidate;
        Group candidateGroup;
        auto adaptation = layer(interface);
        for (int id = 0; id < 16; id++) {
            if (!adaptation->isConfiguredContext(id)) continue;
            auto context = adaptation->getCompressionContext(id);
            snapshotNumber(candidate, id); snapshotAddress(candidate, context->prefix);
            snapshotNumber(candidate, context->prefixLength); snapshotNumber(candidate, context->allowCompression);
            snapshotNumber(candidate, context->validUntil.raw());
            candidateGroup.contexts[id] = {context->prefix, context->prefixLength, context->allowCompression,
                context->validUntil, context->validUntil, SIMTIME_MAX};
            candidateGroup.contexts[id].withdrawal = context->validUntil <= simTime();
            candidateGroup.contexts[id].unlimited = context->validUntil == SIMTIME_MAX;
            candidateGroup.contexts[id].lifetimeSeconds = std::max(0.0, (context->validUntil - simTime()).dbl());
            candidateGroup.contexts[id].receivedAt = simTime();
        }
        // Canonicalize the semantic PIO set; packet construction retains its configured order.
        snapshotNumber(candidate, UINT64_MAX);
        std::vector<std::vector<uint8_t>> prefixSnapshots;
        for (int i = 0; i < data->getNumAdvPrefixes(); i++) {
            auto prefix = data->getAdvPrefix(i);
            if (prefix.prefixLength < 0 || prefix.prefixLength > 128 || prefix.prefix.isMulticast())
                throw cRuntimeError("Invalid native origin advertised prefix");
            std::vector<uint8_t> prefixSnapshot;
            snapshotAddress(prefixSnapshot, prefix.prefix.getPrefix(prefix.prefixLength)); snapshotNumber(prefixSnapshot, prefix.prefixLength);
            snapshotNumber(prefixSnapshot, prefix.advAutonomousFlag); snapshotNumber(prefixSnapshot, prefix.advValidLifetime.raw());
            snapshotNumber(prefixSnapshot, prefix.advPreferredLifetime.raw());
            prefixSnapshots.push_back(prefixSnapshot);
            auto until = [](simtime_t value) {
                return value == SIMTIME_ZERO ? SIMTIME_MAX : value < SIMTIME_ZERO ? deadline(-value.dbl()) : value;
            };
            auto validUntil = until(prefix.advValidLifetime);
            auto preferredUntil = until(prefix.advPreferredLifetime);
            if (preferredUntil > validUntil)
                throw cRuntimeError("Invalid native origin advertised prefix");
            candidateGroup.prefixes.push_back({prefix.prefix.getPrefix(prefix.prefixLength), prefix.prefixLength,
                prefix.advAutonomousFlag, validUntil, preferredUntil, SIMTIME_MAX});
            auto& stored = candidateGroup.prefixes.back();
            stored.validUnlimited = prefix.advValidLifetime == SIMTIME_ZERO;
            stored.preferredUnlimited = prefix.advPreferredLifetime == SIMTIME_ZERO;
            stored.validSeconds = prefix.advValidLifetime < SIMTIME_ZERO ? -prefix.advValidLifetime.dbl() :
                std::max(0.0, (prefix.advValidLifetime - simTime()).dbl());
            stored.preferredSeconds = prefix.advPreferredLifetime < SIMTIME_ZERO ? -prefix.advPreferredLifetime.dbl() :
                std::max(0.0, (prefix.advPreferredLifetime - simTime()).dbl());
            stored.receivedAt = simTime();
            if (!stored.validUnlimited && (stored.preferredUnlimited || stored.preferredSeconds > stored.validSeconds))
                throw cRuntimeError("Native origin prefix preferred lifetime exceeds valid lifetime");
        }
        std::sort(prefixSnapshots.begin(), prefixSnapshots.end());
        prefixSnapshots.erase(std::unique(prefixSnapshots.begin(), prefixSnapshots.end()), prefixSnapshots.end());
        for (const auto& prefixSnapshot : prefixSnapshots)
            candidate.insert(candidate.end(), prefixSnapshot.begin(), prefixSnapshot.end());
        if (!first && candidate != snapshot)
            throw cRuntimeError("Native context origin requires identical configured contexts and prefixes on advertising interfaces");
        if (first) { snapshot = candidate; group = candidateGroup; first = false; }
    }
    if (first) return;
    bool changed = originSnapshotKnown && snapshot != originSnapshot;
    if (changed && originVersion == UINT32_MAX)
        throw cRuntimeError("Native context authority version exhausted; semantic update cannot wrap UINT32_MAX");
    if (changed) originVersion++;
    originSnapshot = snapshot;
    originSnapshotKnown = true;
    group.version = originVersion;
    group.abro[0] = originVersion >> 8; group.abro[1] = originVersion;
    group.abro[2] = originVersion >> 24; group.abro[3] = originVersion >> 16;
    group.abro[4] = originLifetime >> 8; group.abro[5] = originLifetime;
    for (int i = 0; i < 16; i++) group.abro[6 + i] = originAddress.words()[i / 4] >> (24 - 8 * (i % 4));
    groups[{-1, true, originAddress}] = group;
    if (changed) trigger();
}

void LowpanContextAdvertisements::appendOptions(Ipv6RouterAdvertisement& message, const Group& group) const
{
    auto abro = rawOption(35, 3);
    for (int i = 0; i < 22; i++) abro->setPaddingBytes(i, group.abro[i]);
    message.getOptionsForUpdate().appendOption(abro); message.addChunkLength(B(24));
    for (const auto& entry : group.contexts) {
        const auto& context = entry.second;
        if (context.compressionUntil <= simTime() && !context.withdrawal) continue;
        unsigned minutes = context.unlimited ? 65535 :
            std::min(65535u, secondsLeft(context.lifetimeSeconds, context.receivedAt, false) / 60);
        if (context.compressionUntil > simTime() && minutes == 0) continue;
        auto option = rawOption(34, context.length > 64 ? 3 : 2);
        option->setPaddingBytes(0, context.length);
        option->setPaddingBytes(1, (context.compress ? 16 : 0) | entry.first);
        option->setPaddingBytes(4, minutes >> 8); option->setPaddingBytes(5, minutes);
        writeAddress(*option, 6, context.prefix, context.length > 64 ? 16 : 8);
        message.getOptionsForUpdate().appendOption(option); message.addChunkLength(B(option->getOptionLength() * 8));
    }
    for (const auto& prefix : group.prefixes) {
        if (prefix.validUntil <= simTime()) continue;
        auto option = new Ipv6NdPrefixInformation();
        option->setPrefix(prefix.address); option->setPrefixLength(prefix.length);
        option->setOnlinkFlag(false); option->setAutoAddressConfFlag(prefix.autonomous);
        option->setValidLifetime(secondsLeft(prefix.validSeconds, prefix.receivedAt, prefix.validUnlimited));
        option->setPreferredLifetime(secondsLeft(prefix.preferredSeconds, prefix.receivedAt, prefix.preferredUnlimited));
        message.getOptionsForUpdate().appendOption(option); message.addChunkLength(B(32));
    }
}

std::vector<LowpanContextAdvertisements::GroupKey> LowpanContextAdvertisements::liveGroups() const
{
    std::vector<GroupKey> result;
    for (const auto& entry : groups)
        if (std::get<1>(entry.first) && entry.second.validUntil > simTime()) result.push_back(entry.first);
    return result;
}

void LowpanContextAdvertisements::sendAdvertisements(NetworkInterface *interface, const Ipv6Address& destination,
        const Ieee802154Address& nativeDestination)
{
    updateOrigin();
    auto keys = liveGroups();
    if (destination.isMulticast()) {
        auto& state = advertising[interface->getInterfaceId()];
        if (state.remaining.empty()) state.remaining = keys;
        keys.clear();
        while (!state.remaining.empty()) {
            auto key = state.remaining.front(); state.remaining.erase(state.remaining.begin());
            auto found = groups.find(key);
            if (found != groups.end() && found->second.validUntil > simTime()) { keys.push_back(key); break; }
        }
    }
    auto boundary = dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary"));
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    auto source = data->getLinkLocalAddress();
    if (!boundary || !interface->isUp() || !data->getAdvSendAdvertisements() || source.isUnspecified() ||
            !data->hasValidAddress(source, simTime()) || data->isTentativeAddress(source)) return;
    auto domain = boundary->getLinkDomain();
    for (const auto& key : keys) {
        auto message = makeShared<Ipv6RouterAdvertisement>();
        auto lifetime = data->getAdvDefaultLifetime().dbl();
        if (lifetime < 0 || lifetime > 65535) throw cRuntimeError("Native RA router lifetime must fit 16-bit seconds");
        message->setRouterLifetime(lifetime); message->setCurHopLimit(data->getAdvCurHopLimit());
        message->setManagedAddrConfFlag(data->getAdvManagedFlag()); message->setOtherStatefulConfFlag(data->getAdvOtherConfigFlag());
        message->setReachableTime(data->getAdvReachableTime()); message->setRetransTimer(data->getAdvRetransTimer());
        message->getOptionsForUpdate().appendOption(owner.createNativeLinkOption(boundary->getLinkAddress()));
        message->addChunkLength(B(16));
        appendOptions(*message, groups.at(key));
        auto output = new Packet("native-context-advertisement");
        Icmpv6::insertChecksum(CHECKSUM_COMPUTED, message, output); output->insertAtFront(message);
        auto request = output->addTag<Ieee802154AddressReq>();
        request->setSrcAddress(boundary->getLinkAddress()); request->setDestAddress(nativeDestination);
        request->setSrcPanId(domain->getPanId()); request->setDestPanId(domain->getPanId());
        owner.sendPacketToIpv6Module(output, destination, source, interface->getInterfaceId());
        if (destination.isMulticast())
            if (auto entry = owner.fetchAdvIfEntry(interface)) entry->lastMulticastRATime = simTime();
    }
}

void LowpanContextAdvertisements::trigger()
{
    if (host) return;
    for (auto interface : interfaces()) {
        auto entry = owner.fetchAdvIfEntry(interface);
        if (!entry) continue;
        auto& state = advertising[interface->getInterfaceId()];
        state.triggeredRounds = 3;
        state.remaining.clear();
        auto next = deadline(owner.uniform(0, 2));
        if (entry->lastMulticastRATime >= SIMTIME_ZERO)
            next = std::max(next, entry->lastMulticastRATime >= SIMTIME_MAX - SimTime(10) ? SIMTIME_MAX :
                entry->lastMulticastRATime + SimTime(10));
        if (next < entry->nextScheduledRATime) {
            owner.cancelEvent(entry->raTimeoutMsg);
            entry->nextScheduledRATime = next;
            if (next != SIMTIME_MAX) owner.scheduleAt(next, entry->raTimeoutMsg);
        }
    }
}

void LowpanContextAdvertisements::sendPeriodic(cMessage *message)
{
    auto interface = static_cast<NetworkInterface *>(message->getContextPointer());
    auto entry = owner.fetchAdvIfEntry(interface);
    if (!entry) return;
    auto& state = advertising[interface->getInterfaceId()];
    if (entry->lastMulticastRATime >= SIMTIME_ZERO && simTime() - entry->lastMulticastRATime < SimTime(10)) {
        auto next = entry->lastMulticastRATime >= SIMTIME_MAX - SimTime(10) ? SIMTIME_MAX : entry->lastMulticastRATime + SimTime(10);
        entry->nextScheduledRATime = next;
        if (next != SIMTIME_MAX) owner.scheduleAt(next, message);
        return;
    }
    owner.createAndSendRaPacket(Ipv6Address::ALL_NODES_2, interface);
    entry->numRASent++;
    if (state.remaining.empty() && state.triggeredRounds > 0) state.triggeredRounds--;
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    double interval = !state.remaining.empty() || state.triggeredRounds > 0 ? 10 :
        std::max(10.0, owner.uniform(data->getMinRtrAdvInterval().dbl(), data->getMaxRtrAdvInterval().dbl()));
    auto next = deadline(interval);
    entry->nextScheduledRATime = next;
    if (next != SIMTIME_MAX) owner.scheduleAt(next, message);
}

void LowpanContextAdvertisements::addressReady(NetworkInterface *interface)
{
    if (host || origin || !interface->isUp() || !dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary"))) return;
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    auto source = data->getLinkLocalAddress();
    if (source.isUnspecified() || !data->hasValidAddress(source, simTime()) || data->isTentativeAddress(source)) return;
    auto& discovery = discoveries[interface->getInterfaceId()];
    if (discovery.attempts == 0 && !discovery.active) {
        discovery.active = true;
        // RFC 4861 section 6.3.7: desynchronize the initial solicitation.
        discovery.next = deadline(owner.uniform(0, 1));
    }
    scheduleTimer();
}

void LowpanContextAdvertisements::sendSolicitation(NetworkInterface *interface, Discovery& discovery)
{
    auto boundary = dynamic_cast<LowpanNativeBoundary *>(interface->getSubmodule("boundary"));
    auto data = interface->getProtocolData<Ipv6InterfaceData>();
    auto source = data->getLinkLocalAddress();
    if (!boundary || !interface->isUp() || source.isUnspecified() || !data->hasValidAddress(source, simTime()) ||
            data->isTentativeAddress(source)) { discovery.active = false; discovery.next = SIMTIME_MAX; return; }
    auto message = makeShared<Ipv6RouterSolicitation>();
    message->getOptionsForUpdate().appendOption(owner.createNativeLinkOption(boundary->getLinkAddress())); message->addChunkLength(B(16));
    auto packet = new Packet("native-context-solicitation");
    Icmpv6::insertChecksum(CHECKSUM_COMPUTED, message, packet); packet->insertAtFront(message);
    owner.sendPacketToIpv6Module(packet, Ipv6Address::ALL_ROUTERS_2, source, interface->getInterfaceId());
    discovery.attempts++;
    discovery.next = deadline(discovery.attempts < 3 ? 10 : discovery.attempts == 3 ? 20 : discovery.attempts == 4 ? 40 : 60);
}

simtime_t LowpanContextAdvertisements::nextHostRefresh() const
{
    auto next = SIMTIME_MAX;
    for (const auto& entry : groups) {
        if (entry.second.validUntil <= simTime()) continue;
        next = std::min(next, entry.second.refreshAt);
        for (const auto& context : entry.second.contexts) next = std::min(next, context.second.refreshAt);
        for (const auto& prefix : entry.second.prefixes) next = std::min(next, prefix.refreshAt);
    }
    return next;
}

std::vector<int> LowpanContextAdvertisements::consumeHostRefresh()
{
    std::vector<int> result;
    for (auto& entry : groups) {
        if (entry.second.validUntil <= simTime()) continue;
        bool due = entry.second.refreshAt <= simTime();
        if (due) entry.second.refreshAt = SIMTIME_MAX;
        for (auto& context : entry.second.contexts)
            if (context.second.refreshAt <= simTime()) { due = true; context.second.refreshAt = SIMTIME_MAX; }
        for (auto& prefix : entry.second.prefixes)
            if (prefix.refreshAt <= simTime()) { due = true; prefix.refreshAt = SIMTIME_MAX; }
        if (due) result.push_back(std::get<0>(entry.first));
    }
    return result;
}

void LowpanContextAdvertisements::scheduleTimer()
{
    owner.cancelEvent(maintenanceTimer);
    auto next = SIMTIME_MAX;
    for (const auto& entry : groups)
        if (entry.second.validUntil > simTime()) next = std::min(next, entry.second.validUntil);
    if (!host) {
        for (const auto& entry : discoveries) if (entry.second.active) next = std::min(next, entry.second.next);
        auto refresh = nextHostRefresh();
        if (refresh > simTime()) next = std::min(next, refresh);
    }
    if (next != SIMTIME_MAX) owner.scheduleAt(next, maintenanceTimer);
}

void LowpanContextAdvertisements::handleTimer()
{
    expire();
    if (!host) {
        if (!consumeHostRefresh().empty())
            for (auto interface : interfaces()) {
                auto& discovery = discoveries[interface->getInterfaceId()];
                if (!discovery.active) { discovery.active = true; discovery.attempts = 0; discovery.next = simTime(); }
            }
        for (auto& entry : discoveries)
            if (entry.second.active && entry.second.next <= simTime())
                if (auto interface = owner.ift->getInterfaceById(entry.first)) sendSolicitation(interface, entry.second);
    }
    scheduleTimer();
}

void LowpanContextAdvertisements::start()
{
    updateOrigin();
    for (auto interface : interfaces()) addressReady(interface);
    scheduleTimer();
}

void LowpanContextAdvertisements::clear()
{
    owner.cancelEvent(maintenanceTimer);
    while (!effectiveOwners.empty()) {
        auto entry = effectiveOwners.begin();
        if (owner.ift)
            if (auto interface = owner.ift->getInterfaceById(entry->first.first)) layer(interface)->removeLearnedContext(entry->first.second);
        effectiveOwners.erase(entry);
    }
    groups.clear(); discoveries.clear(); advertising.clear();
    // Static adaptation configuration and the origin semantic version survive restart.
}
} }
