// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanFragmentForwarding.h"
#include "inet/linklayer/lowpan/LowpanLayer.h"
#include "inet/linklayer/lowpan/LowpanSourceFragmentation.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanIpv6.h"
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"
#include "inet/linklayer/lowpan/LowpanNhcCodec.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Simsignals.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/ipv6/Ipv6ExtensionHeaders.h"
#include "inet/networklayer/icmpv6/Icmpv6Header_m.h"
#include <cmath>

namespace inet { namespace lowpan {
namespace {
simtime_t quantizeDuration(double seconds)
{
    if (!std::isfinite(seconds) || seconds < SimTime::fromRaw(1).dbl())
        throw cRuntimeError("Forwarding durations must be finite and at least one tick");
    return seconds >= SimTime::getMaxTime().dbl() ? SimTime::getMaxTime() : SimTime(seconds);
}
simtime_t addTime(simtime_t first, simtime_t second)
{
    auto maximum = SimTime::getMaxTime().raw();
    return SimTime::fromRaw(second.raw() > maximum - first.raw() ? maximum : first.raw() + second.raw());
}
bool isNdType(uint8_t type) { return (type >= 133 && type <= 137) || type == 157 || type == 158; }
constexpr int ALLOW_MALFORMED = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
}

bool LowpanFragmentForwarding::Path::operator==(const Path& other) const
{
    return interfaceModuleId == other.interfaceModuleId && layerModuleId == other.layerModuleId && nextHop == other.nextHop &&
        peer.getSrcAddress() == other.peer.getSrcAddress() && peer.getDestAddress() == other.peer.getDestAddress() &&
        peer.getSrcPanId() == other.peer.getSrcPanId() && peer.getDestPanId() == other.peer.getDestPanId();
}
LowpanFragmentForwarding::LowpanFragmentForwarding(LowpanLayer& owner) : owner(owner)
{
    routes.reference(&owner, "routingTableModule", true);
    icmp.reference(&owner, "icmpv6Module", true);
    ipv6.reference(&owner, "ipv6Module", true);
    if (getContainingNode(routes.get()) != getContainingNode(&owner) ||
            getContainingNode(icmp.get()) != getContainingNode(&owner) || getContainingNode(ipv6.get()) != getContainingNode(&owner))
        throw cRuntimeError("Fragment forwarding requires same-node IPv6 routing, ICMP and LowpanIpv6 modules");
    maxRecords = owner.par("maxForwardingRecords"); maxRanges = owner.par("maxForwardingRanges");
    maxPendingBytes = owner.par("maxForwardingPendingBytes"); maxCarry = owner.par("maxForwardingCarryBytes");
    maxBytes = owner.par("maxForwardingBytes"); emissionCap = owner.par("forwardingPayloadLimit"); maxHops = owner.par("forwardingMaxHops");
    residence = quantizeDuration(owner.par("forwardingResidence")); gap = quantizeDuration(owner.par("forwardingGap"));
    activeLifetime = quantizeDuration(owner.par("forwardingActiveLifetime")); historyLifetime = quantizeDuration(owner.par("forwardingHistoryLifetime"));
    if (maxRecords < 1 || maxRecords > 64 || maxRanges < 1 || maxRanges > 4 || maxPendingBytes < 1 || maxPendingBytes > 512 ||
            maxCarry < 0 || maxCarry > 128 || maxBytes < 1 || maxBytes > 32768 || maxHops < 1 || maxHops > 8 ||
            (emissionCap != 0 && (emissionCap < 13 || emissionCap > 127)) || residence > quantizeDuration(1) ||
            activeLifetime > quantizeDuration(90) || historyLifetime > quantizeDuration(120) || historyLifetime <= activeLifetime ||
            owner.sourceFragments->getResidence() > quantizeDuration(20) || owner.sourceFragments->getTagProtection() < quantizeDuration(150))
        throw cRuntimeError("Invalid native fragment forwarding profile limits");
    auto path = owner.sourceFragments->getResidence();
    for (int i = 0; i < maxHops; i++) path = addTime(path, residence);
    if (activeLifetime <= addTime(path, quantizeDuration(60)) || owner.sourceFragments->getTagProtection() < addTime(path, historyLifetime))
        throw cRuntimeError("Incompatible effective source/path/active/history/tag forwarding bounds");
    for (auto element : owner.par("fragmentForwardingPeers").xmlValue()->getChildren()) {
        auto address = element->getAttribute("native");
        if (strcmp(element->getTagName(), "peer") != 0 || !address || !peers.insert(Ieee802154Address(address)).second)
            throw cRuntimeError("Invalid or duplicate fragment forwarding peer declaration");
    }
    notifications = check_and_cast<cModule *>(owner.networkInterface->getInterfaceTable());
    notifications->subscribe(interfaceStateChangedSignal, this);
    notifications->subscribe(interfaceDeletedSignal, this);
    timer = new cMessage("lowpanFragmentForwarding");
}
LowpanFragmentForwarding::~LowpanFragmentForwarding()
{
    if (notifications) {
        notifications->unsubscribe(interfaceStateChangedSignal, this);
        notifications->unsubscribe(interfaceDeletedSignal, this);
    }
    owner.cancelAndDelete(timer);
}
void LowpanFragmentForwarding::validatePeers()
{
    auto native = check_and_cast<LowpanNativeBoundary *>(owner.link);
    for (const auto& peer : peers)
        if (peer.isUnspecified() || peer.isBroadcast() || peer == native->getLinkAddress() || !native->getLinkDomain()->contains(peer))
            throw cRuntimeError("Fragment forwarding peers must be known other native unicast identities");
}
void LowpanFragmentForwarding::receiveSignal(cComponent *, simsignal_t signal, cObject *object, cObject *)
{
    auto nic = signal == interfaceDeletedSignal ? check_and_cast<NetworkInterface *>(object) :
        check_and_cast<NetworkInterfaceChangeDetails *>(object)->getNetworkInterface();
    if (signal == interfaceDeletedSignal || nic->getState() == NetworkInterface::DOWN) {
        handleInterfaceUnavailable(nic->getId());
    }
}
std::optional<LowpanFragmentForwarding::Key> LowpanFragmentForwarding::findToken(int64_t token) const
{
    for (const auto& item : records) if (item.second.token == token) return item.first;
    return std::nullopt;
}
std::optional<LowpanFragmentForwarding::Key> LowpanFragmentForwarding::findIdentity(const Key& key) const
{
    for (const auto& item : records) {
        const auto& k = item.first;
        if (k.interfaceId == key.interfaceId && k.sourcePan == key.sourcePan && k.destinationPan == key.destinationPan &&
                k.source == key.source && k.destination == key.destination && k.tag == key.tag) return k;
    }
    return std::nullopt;
}
int LowpanFragmentForwarding::computeBufferedBytes() const
{
    int result = 0;
    for (const auto& item : records) for (const auto& range : item.second.pending) result += range.data->getDataLength().get<B>();
    return result;
}
bool LowpanFragmentForwarding::findPath(const Ipv6Header& header, Path& path) const
{
    const auto& source = header.getSrcAddress();
    const auto& destination = header.getDestAddress();
    if (!routes || !ipv6 || ipv6->hasPolicyHooks() || !routes->isForwardingEnabled() || routes->isLocalAddress(destination)) return false;
    auto route = routes->doLongestPrefixMatch(destination);
    if (!route || (route->getExpiryTime() != SIMTIME_ZERO && route->getExpiryTime() <= simTime())) return false;
    auto nic = route->getInterface();
    if (!nic || !nic->isUp() || B(40) + header.getPayloadLength() > B(nic->getMtu()) ||
            getContainingNode(nic) != getContainingNode(&owner) || !ipv6->isNativeSourceUsable(nic, source)) return false;
    LowpanLayer *layer = nullptr;
    for (cModule::SubmoduleIterator it(nic); !it.end(); ++it)
        if (auto candidate = dynamic_cast<LowpanLayer *>(*it)) layer = candidate;
    if (!layer || !layer->fragmentForwarding || !layer->isNativeTransmissionOperational()) return false;
    auto nextHop = route->getNextHop().isUnspecified() ? destination : route->getNextHop();
    auto neighbor = layer->link->resolveNeighbor(nextHop);
    if (!neighbor || neighbor->isBroadcast() || neighbor->isUnspecified()) return false;
    path.interfaceModuleId = nic->getId(); path.layerModuleId = layer->getId(); path.nextHop = nextHop;
    path.peer.setSrcAddress(layer->link->getLinkAddress()); path.peer.setDestAddress(*neighbor);
    path.peer.setSrcPanId(layer->link->getPanId()); path.peer.setDestPanId(layer->link->getPanId());
    return true;
}
bool LowpanFragmentForwarding::isPathValid(const Record& record) const
{
    Path current;
    return owner.isNativeTransmissionOperational() && record.header && findPath(*record.header, current) && current == record.path;
}
bool LowpanFragmentForwarding::inspect(const Packet& packet, bool& control) const
{
    control = false;
    try {
        auto header = packet.peekAtFront<Ipv6Header>(B(40));
        auto next = header->getProtocolId(); b offset = B(40);
        while (isIpv6ExtensionHeader(next)) {
            if (packet.getDataLength() - offset < B(8)) return false;
            auto extension = peekIpv6ExtensionHeaderAt(&packet, offset, next);
            if (extension->isIncorrect() || extension->isIncomplete() || extension->getChunkLength() < B(8) ||
                    extension->getChunkLength() > packet.getDataLength() - offset) return false;
            if (next == IP_PROT_IPv6EXT_FRAGMENT) return false;
            if (next == IP_PROT_IPv6EXT_ROUTING && staticPtrCast<const Ipv6RoutingHeader>(extension)->getSegmentsLeft() != 0) return false;
            if (next == IP_PROT_IPv6EXT_HOP) {
                auto hop = staticPtrCast<const Ipv6HopByHopOptionsHeader>(extension);
                for (size_t i = 0; i < hop->getTlvOptions().getTlvOptionArraySize(); i++)
                    if (hop->getTlvOptions().getTlvOption(i)->getType() > 1) return false;
            }
            next = extension->getNextHeaderProtocol(); offset += extension->getChunkLength();
        }
        if (next == IP_PROT_IPv6EXT_AUTH || next == IP_PROT_IPv6EXT_ESP) return false;
        if (next == IP_PROT_IPv6_ICMP) {
            if (offset >= packet.getDataLength()) return false;
            control = isNdType(packet.peekDataAt<BytesChunk>(offset, B(1))->getBytes()[0]);
        }
        return true;
    }
    catch (const cRuntimeError&) { return false; }
}
bool LowpanFragmentForwarding::decodeFirst(const Packet& packet, int size, const Ieee802154Address& source,
        const Ieee802154Address& destination, LowpanDispatchPrefix::State state, std::unique_ptr<Packet>& original) const
{
    auto input = std::unique_ptr<Packet>(packet.dup());
    input->popAtFront(B(4));
    auto prefix = LowpanDispatchPrefix::parse(*input, 0, state);
    if (prefix.pagingLength) input->popAtFront(B(prefix.pagingLength));
    original = std::make_unique<Packet>(packet.getName());
    if (prefix.kind == LowpanDispatchPrefix::IPV6) {
        input->popAtFront<LowpanHeader>(B(1));
        original->insertAtBack(input->peekData());
        original->copyRegionTags(*input, input->getFrontOffset(), b(0), input->getDataLength());
    }
    else if (prefix.kind == LowpanDispatchPrefix::IPHC) {
        auto decoded = LowpanNhcCodec::decode(*input, size, source, destination, &owner.compressionContexts, simTime());
        if (!decoded.header) return false;
        original->insertAtBack(decoded.header);
        int tail = input->getDataLength().get<B>() - decoded.encodedLength;
        if (tail) {
            original->insertAtBack(input->peekDataAt(B(decoded.encodedLength), B(tail)));
            original->copyRegionTags(*input, input->getFrontOffset() + B(decoded.encodedLength), B(decoded.originalLength), B(tail));
        }
    }
    else return false;
    if (original->getDataLength() < B(40) || original->getDataLength() > B(size)) return false;
    auto header = original->peekAtFront<Ipv6Header>(B(40), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE);
    return header->isCorrect() && header->getVersion() == 6 && header->getPayloadLength() == B(size - 40);
}
void LowpanFragmentForwarding::sendError(const Packet& original, int type, int code) const
{
    try {
        bool control;
        if (!inspect(original, control)) return;
        auto header = original.peekAtFront<Ipv6Header>(B(40));
        if (header->getSrcAddress().isUnspecified() || header->getSrcAddress().isMulticast() || header->getSrcAddress().isLoopback() || header->getDestAddress().isMulticast()) return;
        auto next = header->getProtocolId(); b offset = B(40);
        while (isIpv6ExtensionHeader(next)) {
            auto extension = peekIpv6ExtensionHeaderAt(&original, offset, next);
            next = extension->getNextHeaderProtocol(); offset += extension->getChunkLength();
        }
        if (next == IP_PROT_IPv6_ICMP) {
            if (original.getDataLength() - offset < B(8) || original.peekDataAt<BytesChunk>(offset, B(1))->getBytes()[0] < 128) return;
            if (offset == B(40)) {
                auto icmpHeader = original.peekDataAt<Icmpv6Header>(offset, b(-1), ALLOW_MALFORMED);
                if (!icmpHeader->isCorrect() || icmpHeader->isImproperlyRepresented()) return;
            }
        }
        auto quote = std::unique_ptr<Packet>(original.dup());
        quote->addTagIfAbsent<InterfaceInd>()->setInterfaceId(owner.networkInterface->getInterfaceId());
        icmp->sendErrorMessage(quote.get(), static_cast<Icmpv6Type>(type), code);
    }
    catch (const cRuntimeError&) { /* Insufficient quote must not become a checked-peek failure. */ }
}
void LowpanFragmentForwarding::close(const Key& key, std::optional<LowpanDropReason> failure)
{
    auto found = records.find(key); if (found == records.end()) return;
    auto mode = found->second.mode;
    if (mode == CLOSED) return;
    auto egressId = found->second.path.layerModuleId;
    auto frame = found->second.inFlight;
    if (auto layer = dynamic_cast<LowpanLayer *>(owner.getSimulation()->getModule(egressId))) layer->forwardedTransmissions.erase(frame);
    owner.reassembly->remove(key);
    if (mode == ORDINARY) { records.erase(found); return; }
    auto wire = failure ? std::move(found->second.firstWire) : std::vector<uint8_t>();
    auto historyEnd = found->second.historyEnd;
    auto token = found->second.token;
    found->second = Record(); found->second.mode = CLOSED; found->second.historyEnd = historyEnd; found->second.token = token;
    if (!wire.empty()) {
        auto notice = new Packet("lowpan-forwarding-abort", makeShared<BytesChunk>(wire));
        auto indication = notice->addTag<Ieee802154AddressInd>();
        indication->setSrcAddress(key.source); indication->setDestAddress(key.destination);
        indication->setSrcPanId(key.sourcePan); indication->setDestPanId(key.destinationPan);
        owner.dropLowpanPacket(notice, *failure); // Local snapshot survives cancellation by observers.
    }
}
void LowpanFragmentForwarding::expire()
{
    owner.expireReassembly(); // The Layer owns the sole expiry count, independent of timer order.
    std::vector<Key> keys; for (const auto& item : records) keys.push_back(item.first);
    for (const auto& key : keys) {
        auto it = records.find(key); if (it == records.end()) continue;
        if (it->second.mode == CLOSED) { if (it->second.historyEnd <= simTime()) records.erase(it); continue; }
        if (it->second.mode == ORDINARY || it->second.mode == LOCAL) {
            auto expiry = owner.reassembly->getExpiry(key);
            if (!expiry || *expiry <= simTime() || (it->second.mode == LOCAL && it->second.activeEnd <= simTime()))
                close(key, LOWPAN_LOWER_LIFETIME_EXPIRED);
        }
        else {
            bool expired = it->second.activeEnd <= simTime();
            for (const auto& range : it->second.pending) expired |= range.deadline <= simTime();
            if (expired) close(key, LOWPAN_LOWER_LIFETIME_EXPIRED);
        }
    }
}
void LowpanFragmentForwarding::schedule()
{
    owner.cancelEvent(timer);
    if (records.empty()) return;
    auto next = SimTime::getMaxTime();
    for (const auto& item : records) {
        const auto& record = item.second;
        if (record.mode == CLOSED) next = std::min(next, record.historyEnd);
        else if (record.mode == ORDINARY || record.mode == LOCAL) {
            auto expiry = owner.reassembly->getExpiry(item.first);
            next = std::min(next, expiry.value_or(simTime()));
            if (record.mode == LOCAL) next = std::min(next, record.activeEnd);
        }
        else {
            next = std::min(next, record.activeEnd);
            for (const auto& range : record.pending) next = std::min(next, range.deadline);
            if (record.inFlight < 0 && !record.pending.empty()) next = std::min(next, record.nextSend);
        }
    }
    owner.scheduleAt(std::max(next, simTime()), timer);
}
bool LowpanFragmentForwarding::append(const Key& key, int offset, std::unique_ptr<Packet> data)
{
    auto& record = records.at(key);
    int length = data->getDataLength().get<B>();
    auto duplicate = record.shapes.find(offset);
    if (duplicate != record.shapes.end()) {
        if (duplicate->second != length) close(key, LOWPAN_REASSEMBLY_REJECTED);
        return false;
    }
    if (offset != record.inputEnd || length <= 0 || offset + length > key.size ||
            (offset + length < key.size && length % 8 != 0)) { close(key, LOWPAN_REASSEMBLY_REJECTED); return false; }
    int count = 0, bytes = 0;
    for (const auto& range : record.pending) if (!range.carry) { count++; bytes += range.data->getDataLength().get<B>(); }
    if (count >= maxRanges || length > maxPendingBytes - bytes || length > maxBytes - computeBufferedBytes()) { close(key, LOWPAN_RESOURCE_LIMIT); return false; }
    record.shapes.emplace(offset, length); record.inputEnd = offset + length;
    record.pending.push_back(Range{offset, std::move(data), addTime(simTime(), residence), false});
    return true;
}
bool LowpanFragmentForwarding::process(Packet *packet, bool first, const Ieee802154Address& source,
        const Ieee802154Address& destination, LowpanDispatchPrefix::State state)
{
    if (destination.isBroadcast() || !peers.count(source)) return false;
    expire();
    if (!owner.isNativeTransmissionOperational() || packet->getDataLength() <= B(first ? 4 : 5)) {
        owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); schedule(); return true;
    }
    auto indication = packet->getTag<Ieee802154AddressInd>();
    uint16_t size, tag;
    int offset = 0;
    if (first) {
        auto header = packet->peekAtFront<LowpanFrag1Header>(B(4), ALLOW_MALFORMED);
        if (!header->isCorrect()) { owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return true; }
        size = header->getDatagramSize(); tag = header->getDatagramTag();
    }
    else {
        auto header = packet->peekAtFront<LowpanFragnHeader>(B(5), ALLOW_MALFORMED);
        if (!header->isCorrect()) { owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return true; }
        size = header->getDatagramSize(); tag = header->getDatagramTag(); offset = header->getDatagramOffset() * 8;
    }
    Key key{owner.networkInterface->getId(), indication->getSrcPanId(), indication->getDestPanId(), source, destination, size, tag};
    auto identity = findIdentity(key);
    if (identity) {
        if (identity->size != size) { close(*identity, LOWPAN_REASSEMBLY_REJECTED); owner.dropLowpanPacket(packet, LOWPAN_REASSEMBLY_REJECTED); schedule(); return true; }
        auto& record = records.at(key);
        if (record.mode == CLOSED) { owner.dropLowpanPacket(packet, LOWPAN_REASSEMBLY_REJECTED); return true; }
        if (record.mode == ORDINARY) return false;
        if (first) {
            bool same = record.firstWire == packet->peekDataAsBytes()->getBytes();
            if (!same) close(key, LOWPAN_REASSEMBLY_REJECTED);
            delete packet; schedule(); return true;
        }
        if (record.mode == LOCAL) return false;
        auto data = std::unique_ptr<Packet>(packet->dup()); data->popAtFront(B(5)); data->trimFront();
        append(key, offset, std::move(data)); delete packet; schedule(); return true;
    }
    if (!first) { owner.dropLowpanPacket(packet, LOWPAN_REASSEMBLY_REJECTED); return true; }
    if ((int)records.size() >= maxRecords) { owner.dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT); return true; }
    Record record; record.token = packet->getId(); record.activeEnd = addTime(simTime(), activeLifetime);
    record.historyEnd = addTime(simTime(), historyLifetime); record.nextSend = simTime();
    std::unique_ptr<Packet> original;
    bool control = false;
    bool decoded = decodeFirst(*packet, size, source, destination, state, original);
    if (!decoded || !inspect(*original, control) || control) { records.emplace(key, std::move(record)); schedule(); return false; }
    auto header = original->peekAtFront<Ipv6Header>(B(40));
    if (header->getSrcAddress().isUnspecified() || header->getSrcAddress().isMulticast() || header->getSrcAddress().isLinkLocal() ||
            header->getSrcAddress().isLoopback() || header->getDestAddress().isLinkLocal() || header->getDestAddress().isMulticast() || header->getDestAddress().isLoopback()) {
        records.emplace(key, std::move(record)); schedule(); return false;
    }
    record.firstWire = packet->peekDataAsBytes()->getBytes();
    if (routes->isLocalAddress(header->getDestAddress())) { record.mode = LOCAL; records.emplace(key, std::move(record)); schedule(); return false; }
    if (ipv6->hasPolicyHooks() || !routes->isForwardingEnabled()) { records.emplace(key, std::move(record)); schedule(); return false; }
    auto route = routes->doLongestPrefixMatch(header->getDestAddress());
    bool noRoute = !route || (route->getExpiryTime() != SIMTIME_ZERO && route->getExpiryTime() <= simTime());
    if (header->getHopLimit() <= 1 || noRoute) {
        record.mode = CLOSED; records.emplace(key, std::move(record));
        sendError(*original, header->getHopLimit() <= 1 ? ICMPv6_TIME_EXCEEDED : ICMPv6_DESTINATION_UNREACHABLE, 0);
        delete packet; schedule(); return true;
    }
    if (!findPath(*header, record.path)) { records.emplace(key, std::move(record)); schedule(); return false; }
    auto layer = check_and_cast<LowpanLayer *>(owner.getSimulation()->getModule(record.path.layerModuleId));
    auto envelope = layer->link->prepareTransmission(record.path.peer);
    int budget = emissionCap ? std::min(emissionCap, envelope->getPayloadLimit()) : envelope->getPayloadLimit();
    record.header = staticPtrCast<Ipv6Header>(header->dupShared()); record.header->setHopLimit(header->getHopLimit() - 1);
    if (layer->useIphc)
        record.prefix = LowpanIphcCodec::encode(*record.header, record.path.peer.getSrcAddress(), record.path.peer.getDestAddress(), false, &layer->compressionContexts, simTime());
    else {
        Packet prefix;
        prefix.insertAtBack(makeShared<LowpanHeader>());
        prefix.insertAtBack(record.header);
        record.prefix = prefix.peekData();
    }
    int length = original->getDataLength().get<B>();
    int represented = record.prefix ? std::min(length, 40 + budget - 4 - int(record.prefix->getChunkLength().get<B>())) : 0;
    if (represented < key.size) represented = (represented / 8) * 8;
    if (!record.prefix || record.prefix->getChunkLength() + B(4) > B(budget) || represented < 40 ||
            (length < key.size && length % 8 != 0) || length - represented > maxCarry || length > maxPendingBytes || length > maxBytes - computeBufferedBytes()) {
        record = Record(); record.token = packet->getId(); records.emplace(key, std::move(record)); schedule(); return false;
    }
    auto tagResult = layer->allocateDatagramTag(simTime(), addTime(simTime(), layer->sourceFragments->getTagProtection()));
    if (!tagResult) { owner.dropLowpanPacket(packet, LOWPAN_TAG_EXHAUSTED); return true; }
    record.outgoingTag = *tagResult; record.mode = FORWARD;
    records.emplace(key, std::move(record));
    append(key, 0, std::move(original)); delete packet; schedule(); return true;
}
void LowpanFragmentForwarding::handleReassemblyChanged(const Key& key)
{
    auto it = records.find(key); if (it == records.end()) return;
    if ((it->second.mode == ORDINARY || it->second.mode == LOCAL) && !owner.reassembly->getExpiry(key)) close(key);
    schedule();
}
void LowpanFragmentForwarding::sendNext(const Key& key)
{
    auto it = records.find(key); if (it == records.end() || it->second.pending.empty()) return;
    auto& record = it->second;
    if (!isPathValid(record)) { close(key, LOWPAN_FORWARDING_ABORTED); return; }
    auto layer = check_and_cast<LowpanLayer *>(owner.getSimulation()->getModule(record.path.layerModuleId));
    auto envelope = makeShared<LowpanTransmissionReq>(*layer->link->prepareTransmission(record.path.peer));
    auto& range = record.pending.front();
    envelope->setDeliveryDeadline(std::min(envelope->getDeliveryDeadline(), range.deadline));
    int budget = emissionCap ? std::min(emissionCap, envelope->getPayloadLimit()) : envelope->getPayloadLimit();
    bool first = record.outputEnd == 0;
    int available = range.data->getDataLength().get<B>();
    int capacity = first ? 40 + budget - 4 - record.prefix->getChunkLength().get<B>() : budget - 5;
    int count = std::min(available, capacity);
    if (range.offset + count < key.size) count = (count / 8) * 8;
    if (count <= 0 || (first && count < 40) || available - count > maxCarry) { close(key, LOWPAN_RESOURCE_LIMIT); return; }
    auto frame = std::make_unique<Packet>("lowpan-forwarded-fragment");
    int skip = first ? 40 : 0;
    if (count > skip) {
        frame->insertAtBack(range.data->peekDataAt(B(skip), B(count - skip)));
        frame->copyRegionTags(*range.data, range.data->getFrontOffset() + B(skip), b(0), B(count - skip));
    }
    if (first) {
        frame->insertAtFront(record.prefix);
        auto header = makeShared<LowpanFrag1Header>(); header->setDatagramSize(key.size); header->setDatagramTag(record.outgoingTag); frame->insertAtFront(header);
    }
    else {
        auto header = makeShared<LowpanFragnHeader>(); header->setDatagramSize(key.size); header->setDatagramTag(record.outgoingTag); header->setDatagramOffset(range.offset / 8); frame->insertAtFront(header);
    }
    *frame->addTag<LowpanTransmissionReq>() = *envelope;
    frame->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    auto id = frame->getId(), token = record.token;
    owner.emit(LowpanLayer::fragmentSentSignal, frame.get());
    // Observers may stop either interface, remove the route or cancel the record.
    it = records.find(key);
    if (it == records.end() || it->second.token != token || it->second.mode != FORWARD) return;
    if (!isPathValid(it->second) || it->second.pending.front().deadline <= simTime()) { close(key, LOWPAN_FORWARDING_ABORTED); return; }
    it->second.inFlight = id; it->second.stagedCount = count;
    // Resolve the copied identity again after observations; do not reuse a borrowed target pointer.
    auto target = dynamic_cast<LowpanLayer *>(owner.getSimulation()->getModule(it->second.path.layerModuleId));
    if (!target || !target->pushForwardedPacket(frame.release(), owner.getId(), token)) close(key, LOWPAN_FORWARDING_ABORTED);
}
void LowpanFragmentForwarding::handleTransmissionCompleted(int64_t token, int64_t frameId, bool successful)
{
    cMethodCallContextSwitcher context(&owner); context.methodCallSilent();
    auto key = findToken(token); if (!key) return;
    auto it = records.find(*key);
    if (it->second.mode != FORWARD || it->second.inFlight != frameId) return;
    auto& record = it->second;
    record.inFlight = -1;
    if (!successful || !isPathValid(record) || record.pending.front().deadline <= simTime() || record.activeEnd <= simTime()) close(*key, LOWPAN_FORWARDING_ABORTED);
    else {
        int count = record.stagedCount;
        auto& range = record.pending.front();
        range.data->popAtFront(B(count)); range.data->trimFront(); range.offset += count; range.carry = true;
        record.outputEnd += count; record.nextSend = addTime(simTime(), gap);
        if (range.data->getDataLength() == b(0)) record.pending.pop_front();
        if (record.outputEnd == key->size) close(*key);
    }
    schedule();
}
void LowpanFragmentForwarding::handleTimer()
{
    if (!owner.isNativeTransmissionOperational()) handleInterfaceUnavailable(owner.networkInterface->getId());
    expire();
    std::vector<Key> keys; for (const auto& item : records) keys.push_back(item.first);
    for (const auto& key : keys) {
        auto it = records.find(key);
        if (it != records.end() && it->second.mode == FORWARD && it->second.inFlight < 0 &&
                !it->second.pending.empty() && it->second.nextSend <= simTime()) sendNext(key);
    }
    schedule();
}
void LowpanFragmentForwarding::handleInterfaceUnavailable(int moduleId)
{
    cMethodCallContextSwitcher context(&owner); context.methodCallSilent();
    std::vector<Key> keys;
    for (const auto& item : records)
        if (moduleId == owner.networkInterface->getId() || item.second.path.interfaceModuleId == moduleId) keys.push_back(item.first);
    for (const auto& key : keys) close(key);
    schedule();
}
} }
