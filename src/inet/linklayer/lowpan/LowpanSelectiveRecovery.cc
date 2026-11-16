// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanSelectiveRecovery.h"
#include "inet/linklayer/lowpan/LowpanRecoverySource.h"
#include "inet/linklayer/lowpan/LowpanLayer.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanIpv6.h"
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"
#include "inet/linklayer/lowpan/LowpanRfragAckHeader_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Simsignals.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/ipv6/Ipv6ExtensionHeaders.h"
#include "inet/networklayer/icmpv6/Icmpv6Header_m.h"
#include <cmath>
#include <tuple>

namespace inet { namespace lowpan {
namespace {
constexpr int ALLOW_MALFORMED = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
bool isNdType(uint8_t type) { return (type >= 133 && type <= 137) || type == 157 || type == 158; }
}
simtime_t LowpanSelectiveRecovery::quantizeDuration(double seconds)
{
    if (!std::isfinite(seconds) || seconds < SimTime::fromRaw(1).dbl())
        throw cRuntimeError("Recovery duration must be finite and at least one tick");
    return seconds >= SIMTIME_MAX.dbl() ? SIMTIME_MAX : SimTime(seconds);
}
simtime_t LowpanSelectiveRecovery::addTime(simtime_t first, simtime_t second)
{
    auto maximum = SIMTIME_MAX.raw();
    return SimTime::fromRaw(second.raw() > maximum - first.raw() ? maximum : first.raw() + second.raw());
}
bool LowpanSelectiveRecovery::Key::operator<(const Key& other) const
{
    return std::tie(source, destination, pan, tag) < std::tie(other.source, other.destination, other.pan, other.tag);
}
bool LowpanSelectiveRecovery::Path::operator==(const Path& other) const
{
    return layerId == other.layerId && interfaceId == other.interfaceId && nextHop == other.nextHop &&
        peer.getSrcAddress() == other.peer.getSrcAddress() && peer.getDestAddress() == other.peer.getDestAddress() &&
        peer.getSrcPanId() == other.peer.getSrcPanId() && peer.getDestPanId() == other.peer.getDestPanId();
}
LowpanSelectiveRecovery::LowpanSelectiveRecovery(LowpanLayer& owner) : owner(owner)
{
    routes.reference(&owner, "routingTableModule", true);
    ipv6.reference(&owner, "ipv6Module", true);
    icmp.reference(&owner, "icmpv6Module", true);
    if (getContainingNode(routes.get()) != getContainingNode(&owner) || getContainingNode(ipv6.get()) != getContainingNode(&owner) ||
            getContainingNode(icmp.get()) != getContainingNode(&owner))
        throw cRuntimeError("Recovery requires same-node routing, LowpanIpv6 and ICMP modules");
    maxRecords = owner.par("maxRecoveryRecords"); maxBytes = owner.par("maxRecoveryBytes");
    maxControls = owner.par("maxRecoveryControls"); maxPending = owner.par("maxRecoveryPendingFrames");
    maxPendingBytes = owner.par("maxRecoveryPendingBytes"); payloadLimit = owner.par("recoveryPayloadLimit");
    pathPayloadLimit = owner.par("recoveryPathPayloadLimit");
    residence = quantizeDuration(owner.par("recoveryResidence")); gap = quantizeDuration(owner.par("recoveryGap"));
    incompleteLifetime = quantizeDuration(owner.par("recoveryIncompleteLifetime"));
    fullLifetime = quantizeDuration(owner.par("recoveryFullLifetime"));
    historyLifetime = quantizeDuration(owner.par("recoveryHistoryLifetime"));
    tagProtection = quantizeDuration(owner.par("recoveryTagProtection"));
    if (maxRecords < 1 || maxRecords > 64 || maxBytes < 40 || maxBytes > 32768 || maxControls < 1 || maxControls > 64 ||
            maxPending < 1 || maxPending > 4 || maxPendingBytes < 1 || maxPendingBytes > 512 ||
            pathPayloadLimit < 94 || pathPayloadLimit > 127 || (payloadLimit != 0 && (payloadLimit < 94 || payloadLimit > 127)) ||
            residence > quantizeDuration(.05) || gap < quantizeDuration(.15) || incompleteLifetime > quantizeDuration(60) ||
            fullLifetime > quantizeDuration(30) || historyLifetime < incompleteLifetime || historyLifetime > quantizeDuration(120) ||
            tagProtection < quantizeDuration(166))
        throw cRuntimeError("Invalid selective recovery bounds");
    sources = std::make_unique<LowpanRecoverySource>(*this);
    notifications = check_and_cast<cModule *>(owner.networkInterface->getInterfaceTable());
    notifications->subscribe(interfaceStateChangedSignal, this);
    notifications->subscribe(interfaceDeletedSignal, this);
    timer = new cMessage("lowpanSelectiveRecovery");
}
LowpanSelectiveRecovery::~LowpanSelectiveRecovery()
{
    if (notifications) {
        notifications->unsubscribe(interfaceStateChangedSignal, this);
        notifications->unsubscribe(interfaceDeletedSignal, this);
    }
    owner.cancelAndDelete(timer);
}
void LowpanSelectiveRecovery::receiveSignal(cComponent *, simsignal_t signal, cObject *object, cObject *)
{
    auto nic = signal == interfaceDeletedSignal ? check_and_cast<NetworkInterface *>(object) :
        check_and_cast<NetworkInterfaceChangeDetails *>(object)->getNetworkInterface();
    if (signal == interfaceDeletedSignal || nic->getState() == NetworkInterface::DOWN) {
        cMethodCallContextSwitcher context(&owner); context.methodCallSilent();
        handleInterfaceUnavailable(nic->getId());
    }
}
std::optional<LowpanSelectiveRecovery::Key> LowpanSelectiveRecovery::findToken(int64_t token) const
{
    for (const auto& item : records) if (item.second.token == token) return item.first;
    return std::nullopt;
}
int LowpanSelectiveRecovery::computeRetainedBytes() const
{
    int bytes = sources ? sources->getRetainedBytes() : 0;
    for (const auto& item : records) {
        const auto& record = item.second;
        bytes += record.bytes.size() + record.firstBytes.size() + record.inFlightBytes;
        if (record.outputHeader) bytes += record.outputHeader->getChunkLength().get<B>();
        for (const auto& frame : record.pending) bytes += frame.body->getChunkLength().get<B>();
    }
    for (const auto& control : controls) bytes += control.bytes->getChunkLength().get<B>();
    for (const auto& frame : controlFrames) bytes += frame.second;
    return bytes;
}
bool LowpanSelectiveRecovery::hasCapacity(int bytes, bool newRecord) const
{
    return (!newRecord || records.size() < size_t(maxRecords)) && bytes <= maxBytes - computeRetainedBytes();
}
bool LowpanSelectiveRecovery::findPath(const Ipv6Address& source, const Ipv6Address& destination, Path& path, int originalSize) const
{
    if (!routes->isForwardingEnabled() || ipv6->hasPolicyHooks() || routes->isLocalAddress(destination)) return false;
    auto route = routes->doLongestPrefixMatch(destination);
    if (!route || (route->getExpiryTime() != SIMTIME_ZERO && route->getExpiryTime() <= simTime())) return false;
    auto nic = route->getInterface();
    if (!nic || !nic->isUp() || originalSize > nic->getMtu() || getContainingNode(nic) != getContainingNode(&owner) || !ipv6->isNativeSourceUsable(nic, source)) return false;
    LowpanLayer *layer = nullptr;
    for (cModule::SubmoduleIterator it(nic); !it.end(); ++it)
        if (auto candidate = dynamic_cast<LowpanLayer *>(*it)) layer = candidate;
    if (!layer || !layer->selectiveRecovery || !layer->isNativeTransmissionOperational()) return false;
    auto next = route->getNextHop().isUnspecified() ? destination : route->getNextHop();
    auto peer = layer->link->resolveNeighbor(next);
    if (!peer || peer->isUnspecified() || peer->isBroadcast()) return false;
    path.layerId = layer->getId(); path.interfaceId = nic->getId(); path.nextHop = next;
    path.peer.setSrcAddress(layer->link->getLinkAddress()); path.peer.setDestAddress(*peer);
    path.peer.setSrcPanId(layer->link->getPanId()); path.peer.setDestPanId(layer->link->getPanId());
    return true;
}
bool LowpanSelectiveRecovery::isPathValid(const Record& record) const
{
    Path current;
    return owner.isNativeTransmissionOperational() && record.ipv6 &&
        findPath(record.ipv6->getSrcAddress(), record.ipv6->getDestAddress(), current, record.ipv6->getPayloadLength().get<B>() + 40) && current == record.path;
}
bool LowpanSelectiveRecovery::inspectTransit(const Packet& packet) const
{
    try {
        auto header = packet.peekAtFront<Ipv6Header>(B(40));
        auto source = header->getSrcAddress(), destination = header->getDestAddress();
        if (source.isUnspecified() || source.isMulticast() || source.isLoopback() || source.isLinkLocal() ||
                destination.isMulticast() || destination.isLinkLocal() || destination.isLoopback()) return false;
        auto next = header->getProtocolId(); b offset = B(40);
        while (isIpv6ExtensionHeader(next)) {
            if (packet.getDataLength() - offset < B(8)) return false;
            auto extension = peekIpv6ExtensionHeaderAt(&packet, offset, next);
            if (!extension->isCorrect() || extension->getChunkLength() < B(8) || extension->getChunkLength() > packet.getDataLength() - offset) return false;
            if (next == IP_PROT_IPv6EXT_FRAGMENT) return false;
            if (next == IP_PROT_IPv6EXT_ROUTING && staticPtrCast<const Ipv6RoutingHeader>(extension)->getSegmentsLeft() != 0) return false;
            if (next == IP_PROT_IPv6EXT_HOP || next == IP_PROT_IPv6EXT_DEST) {
                const TlvOptions *options = next == IP_PROT_IPv6EXT_HOP ?
                    &staticPtrCast<const Ipv6HopByHopOptionsHeader>(extension)->getTlvOptions() :
                    &staticPtrCast<const Ipv6DestinationOptionsHeader>(extension)->getTlvOptions();
                for (size_t i = 0; i < options->getTlvOptionArraySize(); i++) if (options->getTlvOption(i)->getType() > 1) return false;
            }
            next = extension->getNextHeaderProtocol(); offset += extension->getChunkLength();
        }
        if (next == IP_PROT_IPv6EXT_AUTH || next == IP_PROT_IPv6EXT_ESP) return false;
        if (next == IP_PROT_IPv6_ICMP) {
            if (offset >= packet.getDataLength()) return false;
            if (isNdType(packet.peekDataAt<BytesChunk>(offset, B(1))->getBytes()[0])) return false;
        }
        return true;
    }
    catch (const cRuntimeError&) { return false; }
}
bool LowpanSelectiveRecovery::decodeFirst(const Packet& packet, int size, const Key& key, Ptr<Ipv6Header>& header, int& length) const
{
    try {
        auto iphc = packet.peekAtFront<LowpanIphcHeader>(b(-1), ALLOW_MALFORMED);
        if (!iphc->isCorrect() || iphc->isImproperlyRepresented() || iphc->getNh() || iphc->getCid() || iphc->getSac() ||
                iphc->getDac() || iphc->getM()) return false;
        length = iphc->getChunkLength().get<B>();
        if (length > packet.getDataLength().get<B>()) return false;
        int originalSize = size < 0 ? 40 : size - length + 40;
        if (originalSize < 40 || originalSize > 65575) return false;
        header = LowpanIphcCodec::decode(*iphc, originalSize, key.source, key.destination);
        return header && header->isCorrect() && header->getVersion() == 6;
    }
    catch (const cRuntimeError&) { return false; }
}
void LowpanSelectiveRecovery::sendError(const Packet& quote, int type, int code, int mtu) const
{
    if (!owner.isNativeTransmissionOperational()) return;
    try {
        if (!inspectTransit(quote)) return;
        auto header = quote.peekAtFront<Ipv6Header>(B(40));
        auto next = header->getProtocolId(); b offset = B(40);
        while (isIpv6ExtensionHeader(next)) {
            auto extension = peekIpv6ExtensionHeaderAt(&quote, offset, next);
            next = extension->getNextHeaderProtocol(); offset += extension->getChunkLength();
        }
        if (next == IP_PROT_IPv6_ICMP && (quote.getDataLength() - offset < B(8) ||
                quote.peekDataAt<BytesChunk>(offset, B(1))->getBytes()[0] < 128)) return;
        auto input = std::unique_ptr<Packet>(quote.dup());
        input->addTagIfAbsent<InterfaceInd>()->setInterfaceId(owner.networkInterface->getInterfaceId());
        icmp->sendErrorMessage(input.get(), static_cast<Icmpv6Type>(type), code, mtu);
    }
    catch (const cRuntimeError&) { }
}
void LowpanSelectiveRecovery::sendMtuError(const Record& record) const
{
    if (!record.ipv6) return;
    auto route = routes->doLongestPrefixMatch(record.ipv6->getDestAddress());
    if (!route || (route->getExpiryTime() != SIMTIME_ZERO && route->getExpiryTime() <= simTime()) || !route->getInterface()) return;
    int mtu = route->getInterface()->getMtu();
    if (record.ipv6->getPayloadLength().get<B>() + 40 <= mtu) return;
    Packet quote("recovery-mtu-prefix", record.ipv6);
    if (int(record.firstBytes.size()) > record.headerLength)
        quote.insertAtBack(makeShared<BytesChunk>(std::vector<uint8_t>(record.firstBytes.begin() + record.headerLength, record.firstBytes.end())));
    sendError(quote, ICMPv6_PACKET_TOO_BIG, 0, mtu);
}
std::optional<uint8_t> LowpanSelectiveRecovery::allocateTag(int ownerId, int64_t token, const Ieee802154AddressReq& peer)
{
    auto tag = tags.allocate(simTime(), addTime(simTime(), tagProtection), uint8_t(owner.intrand(256)));
    if (tag) {
        reverse.erase(*tag); // A deleted ingress owner may have left metadata beyond this tag's guard.
        reverse.emplace(*tag, Reverse{ownerId, token, peer.getDestAddress(), peer.getDestPanId(), false});
    }
    return tag;
}
void LowpanSelectiveRecovery::removeReverse(int layerId, uint8_t tag, int64_t token)
{
    auto layer = dynamic_cast<LowpanLayer *>(owner.getSimulation()->getModule(layerId));
    if (!layer || !layer->selectiveRecovery) return;
    auto& index = layer->selectiveRecovery->reverse;
    auto found = index.find(tag);
    if (found != index.end() && found->second.ownerId == owner.getId() && found->second.token == token) index.erase(found);
}
void LowpanSelectiveRecovery::queueControl(const Ieee802154AddressReq& peer, const Ptr<const Chunk>& bytes)
{
    cMethodCallContextSwitcher context(&owner); context.methodCallSilent();
    if (!owner.isNativeTransmissionOperational()) return;
    if (controls.size() + controlFrames.size() >= size_t(maxControls) || !hasCapacity(bytes->getChunkLength().get<B>(), false)) {
        owner.dropLowpanPacket(new Packet("recovery-control-capacity", bytes), LOWPAN_RESOURCE_LIMIT); return;
    }
    controls.push_back({peer, bytes, addTime(simTime(), residence)});
    sendControls();
}
void LowpanSelectiveRecovery::queueAck(const Key& key, uint32_t bitmap, bool marked)
{
    Ieee802154AddressReq peer;
    peer.setSrcAddress(key.destination); peer.setDestAddress(key.source);
    peer.setSrcPanId(key.pan); peer.setDestPanId(key.pan);
    auto header = makeShared<LowpanRfragAckHeader>(); header->setDatagramTag(key.tag);
    header->setBitmap(bitmap); header->setEcn(marked);
    queueControl(peer, header);
}
void LowpanSelectiveRecovery::sendControls()
{
    // Remove queue ownership before observations/transfer. Physical originals stay bounded/correlated until completion.
    while (!controls.empty() && owner.isNativeTransmissionOperational()) {
        auto control = std::move(controls.front()); controls.pop_front();
        auto packet = std::make_unique<Packet>("lowpan-recovery-control", control.bytes);
        if (control.deadline <= simTime()) { owner.dropLowpanPacket(packet.release(), LOWPAN_LOWER_LIFETIME_EXPIRED); continue; }
        auto envelope = makeShared<LowpanTransmissionReq>(*owner.link->prepareTransmission(control.peer));
        envelope->setDeliveryDeadline(std::min(envelope->getDeliveryDeadline(), control.deadline));
        if (packet->getDataLength() > B(envelope->getPayloadLimit())) { owner.dropLowpanPacket(packet.release(), LOWPAN_RESOURCE_LIMIT); continue; }
        *packet->addTag<LowpanTransmissionReq>() = *envelope;
        packet->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
        auto id = packet->getId(); controlFrames[id] = packet->getDataLength().get<B>();
        auto observation = std::unique_ptr<Packet>(packet->dup());
        owner.emit(owner.fragmentSentSignal, observation.get());
        if (!controlFrames.count(id) || !owner.isNativeTransmissionOperational()) { controlFrames.erase(id); continue; }
        bool accepted = owner.pushForwardedPacket(packet.release(), owner.getId(), -1, true);
        if (!accepted) controlFrames.erase(id);
    }
}
void LowpanSelectiveRecovery::close(const Key& key, bool full, bool sendReset)
{
    auto found = records.find(key); if (found == records.end()) return;
    auto phase = found->second.phase;
    if (phase == ABORT) return;
    auto token = found->second.token;
    auto path = found->second.path;
    auto outTag = found->second.outgoingTag;
    auto flight = found->second.inFlight;
    auto snapshot = found->second.outputHeader;
    auto history = found->second.historyEnd;
    auto bitmap = found->second.bitmap;
    removeReverse(path.layerId, outTag, token);
    if (auto layer = dynamic_cast<LowpanLayer *>(owner.getSimulation()->getModule(path.layerId))) layer->forwardedTransmissions.erase(flight);
    found->second = Record();
    found->second.phase = full ? FULL : ABORT; found->second.token = token;
    found->second.historyEnd = history; found->second.bitmap = bitmap;
    found->second.fullEnd = std::min(history, addTime(simTime(), fullLifetime));
    if (sendReset && phase == RELAY && snapshot) {
        auto header = makeShared<LowpanRfragHeader>(); header->setDatagramTag(outTag);
        header->setFragmentOffset(0); header->setFragmentSize(0);
        auto packet = std::make_unique<Packet>("lowpan-reset", snapshot); packet->insertAtFront(header);
        if (auto layer = dynamic_cast<LowpanLayer *>(owner.getSimulation()->getModule(path.layerId)))
            if (layer->selectiveRecovery && layer->isNativeTransmissionOperational())
                layer->selectiveRecovery->queueControl(path.peer, packet->peekData());
    }
}
void LowpanSelectiveRecovery::finishSource(int64_t token, bool successful)
{
    auto key = findToken(token); if (key) close(*key, successful);
}
void LowpanSelectiveRecovery::protect(const Key& key)
{
    if (records.count(key) || !hasCapacity(0, true)) return;
    Record record; record.token = nextToken++; record.historyEnd = addTime(simTime(), historyLifetime);
    records.emplace(key, std::move(record));
}
void LowpanSelectiveRecovery::expire()
{
    std::vector<int64_t> expiredSources;
    for (const auto& item : records)
        if (item.second.phase == SOURCE && item.second.activeEnd <= simTime()) expiredSources.push_back(item.second.token);
    for (auto token : expiredSources) sources->retire(token, false);
    std::vector<Key> keys;
    for (const auto& item : records) keys.push_back(item.first);
    for (const auto& key : keys) {
        auto found = records.find(key); if (found == records.end()) continue;
        auto phase = found->second.phase;
        if (found->second.historyEnd <= simTime()) { removeReverse(found->second.path.layerId, found->second.outgoingTag, found->second.token); records.erase(found); }
        else if (phase == FULL && found->second.fullEnd <= simTime()) close(key, false);
        else if ((phase == RECEIVER || phase == RELAY) && found->second.activeEnd <= simTime()) close(key, false, true);
    }
}
void LowpanSelectiveRecovery::schedule()
{
    owner.cancelEvent(timer);
    std::optional<simtime_t> next = sources->getNextEvent();
    for (const auto& item : records) {
        const auto& record = item.second;
        auto when = record.historyEnd;
        if (record.phase == FULL) when = std::min(when, record.fullEnd);
        if (record.phase == RECEIVER || record.phase == RELAY) when = std::min(when, record.activeEnd);
        if (record.phase == RELAY && record.inFlight < 0 && !record.pending.empty()) when = std::min(when, record.nextSend);
        if (!next || when < *next) next = when;
    }
    if (next) owner.scheduleAt(std::max(simTime(), *next), timer);
}
void LowpanSelectiveRecovery::handleTimer()
{
    expire();
    if (owner.isNativeTransmissionOperational()) {
        sources->handleTimer();
        std::vector<Key> keys;
        for (const auto& item : records) if (item.second.phase == RELAY) keys.push_back(item.first);
        for (const auto& key : keys) sendNext(key);
        sendControls();
    }
    schedule();
}
void LowpanSelectiveRecovery::admit(Packet *packet, const Ieee802154AddressReq& peer, int nativePayloadLimit)
{
    expire(); sources->admit(packet, peer, nativePayloadLimit); schedule();
}
void LowpanSelectiveRecovery::handleAck(int64_t token, uint32_t bitmap, bool marked)
{
    cMethodCallContextSwitcher context(&owner); context.methodCallSilent();
    expire();
    auto key = findToken(token); if (!key) return;
    auto found = records.find(*key);
    if (found->second.phase == SOURCE) { sources->handleAck(token, bitmap); schedule(); return; }
    if (found->second.phase == ABORT) { if (bitmap == 0) queueAck(*key, 0, marked); return; }
    if (found->second.phase != RELAY) return;
    if (!isPathValid(found->second)) { sendMtuError(found->second); close(*key, false, true); queueAck(*key, 0); schedule(); return; }
    // A downstream FULL is cached only after submitted ranges cover the entire translated datagram.
    bool fullCoverage = true;
    if (bitmap == UINT32_MAX) {
        int position = 0;
        std::vector<Shape> shapes;
        for (const auto& shape : found->second.submittedShapes) if (shape.offset >= 0) shapes.push_back(shape);
        std::sort(shapes.begin(), shapes.end(), [](const Shape& first, const Shape& second) { return first.offset < second.offset; });
        for (const auto& shape : shapes) { if (shape.offset > position) { fullCoverage = false; break; } position = std::max(position, shape.offset + shape.size); }
        fullCoverage &= position >= found->second.size;
    }
    if (bitmap == UINT32_MAX && !fullCoverage) return;
    queueAck(*key, bitmap, marked);
    if (bitmap == 0 || bitmap == UINT32_MAX) close(*key, bitmap == UINT32_MAX);
    schedule();
}
void LowpanSelectiveRecovery::processAck(Packet *packet)
{
    if (packet->getDataLength() != B(6)) { owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return; }
    auto ack = packet->peekAtFront<LowpanRfragAckHeader>(B(6), ALLOW_MALFORMED);
    if (!ack->isCorrect()) { owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return; }
    auto peer = packet->getTag<Ieee802154AddressInd>();
    auto found = reverse.find(ack->getDatagramTag());
    if (found != reverse.end() && found->second.peer == peer->getSrcAddress() && found->second.pan == peer->getSrcPanId() &&
            (!found->second.nullOnly || ack->getBitmap() == 0)) {
        auto target = found->second;
        if (auto layer = dynamic_cast<LowpanLayer *>(owner.getSimulation()->getModule(target.ownerId)))
            if (layer->selectiveRecovery) layer->selectiveRecovery->handleAck(target.token, ack->getBitmap(), ack->getEcn());
    }
    delete packet;
}
void LowpanSelectiveRecovery::processReset(Packet *packet, const Ptr<const LowpanRfragHeader>& header, const Key& key)
{
    auto found = records.find(key);
    if (found != records.end() && found->second.phase == RELAY && isPathValid(found->second)) {
        auto path = found->second.path; auto tag = found->second.outgoingTag; auto token = found->second.token;
        auto layer = check_and_cast<LowpanLayer *>(owner.getSimulation()->getModule(path.layerId));
        auto outgoing = makeShared<LowpanRfragHeader>(*header); outgoing->setDatagramTag(tag);
        packet->insertAtFront(outgoing);
        if (header->getAckRequested()) {
            // Detach lower correlation and all retained data before keeping NULL-only reverse metadata.
            close(key, false);
            auto retained = records.find(key);
            if (retained != records.end()) { retained->second.path = path; retained->second.outgoingTag = tag; }
            layer->selectiveRecovery->reverse.emplace(tag, Reverse{owner.getId(), token,
                path.peer.getDestAddress(), path.peer.getDestPanId(), true});
        }
        else { close(key, false); removeReverse(path.layerId, tag, token); }
        layer->selectiveRecovery->queueControl(path.peer, packet->peekData()); delete packet; return;
    }
    if (found != records.end()) {
        removeReverse(found->second.path.layerId, found->second.outgoingTag, found->second.token);
        close(key, false);
    }
    if (header->getSequence() != 0) { queueAck(key, 0); delete packet; return; }
    Ptr<Ipv6Header> ipv6Header; int length;
    if (decodeFirst(*packet, -1, key, ipv6Header, length)) {
        auto quote = std::make_unique<Packet>("reset-routing-only", ipv6Header);
        if (packet->getDataLength() > B(length)) quote->insertAtBack(packet->peekDataAt(B(length), packet->getDataLength() - B(length)));
        Path path;
        if (inspectTransit(*quote) && ipv6Header->getHopLimit() > 1 && findPath(ipv6Header->getSrcAddress(), ipv6Header->getDestAddress(), path)) {
            auto layer = check_and_cast<LowpanLayer *>(owner.getSimulation()->getModule(path.layerId));
            // No reverse binding or operation allocation for stateless cleanup.
            auto tag = layer->selectiveRecovery->tags.allocate(simTime(), addTime(simTime(), tagProtection), uint8_t(owner.intrand(256)));
            if (tag) {
                layer->selectiveRecovery->reverse.erase(*tag);
                auto routing = makeShared<Ipv6Header>(*ipv6Header); routing->setHopLimit(routing->getHopLimit() - 1);
                auto encoded = LowpanIphcCodec::encode(*routing, path.peer.getSrcAddress(), path.peer.getDestAddress());
                auto outgoing = makeShared<LowpanRfragHeader>(*header); outgoing->setDatagramTag(*tag);
                auto result = std::make_unique<Packet>("stateless-reset", encoded);
                if (packet->getDataLength() > B(length)) result->insertAtBack(packet->peekDataAt(B(length), packet->getDataLength() - B(length)));
                result->insertAtFront(outgoing);
                layer->selectiveRecovery->queueControl(path.peer, result->peekData());
                delete packet; return;
            }
        }
    }
    if (header->getAckRequested()) queueAck(key, 0);
    owner.dropLowpanPacket(packet, LOWPAN_RECOVERY_ABORTED);
}
void LowpanSelectiveRecovery::acceptReceiver(const Key& key, const Ptr<const LowpanRfragHeader>& header, const Packet& packet)
{
    auto found = records.find(key); if (found == records.end()) return;
    auto& record = found->second;
    int sequence = header->getSequence(), offset = sequence == 0 ? 0 : header->getFragmentOffset();
    int size = header->getFragmentSize();
    auto shape = record.shapes[sequence];
    if ((sequence == 0 && header->getFragmentOffset() != record.size) || offset < 0 || offset + size > record.size ||
            (shape.offset >= 0 && (shape.offset != offset || shape.size != size))) {
        close(key, false); queueAck(key, 0); return;
    }
    auto bytes = packet.peekDataAsBytes()->getBytes();
    for (int i = 0; i < size; i++)
        if (record.covered[offset + i] && record.bytes[offset + i] != bytes[i]) { close(key, false); queueAck(key, 0); return; }
    auto taggedInput = std::unique_ptr<Packet>(packet.dup());
    for (int start = 0; start < size;) {
        if (record.covered[offset + start]) { start++; continue; }
        int end = start + 1;
        while (end < size && !record.covered[offset + end]) end++;
        record.regions.copyTags(taggedInput->getRegionTags(), packet.getFrontOffset() + B(start), B(offset + start), B(end - start));
        start = end;
    }
    for (int i = 0; i < size; i++) { record.bytes[offset + i] = bytes[i]; record.covered[offset + i] = true; }
    record.shapes[sequence] = {offset, size}; record.bitmap |= uint32_t(1) << (31 - sequence);
    record.marked |= header->getEcn();
    bool complete = std::find(record.covered.begin(), record.covered.end(), false) == record.covered.end();
    if (complete) {
        auto output = new Packet("recovered-ipv6", record.ipv6);
        if (record.size > record.headerLength)
            output->insertAtBack(makeShared<BytesChunk>(std::vector<uint8_t>(record.bytes.begin() + record.headerLength, record.bytes.end())));
        output->getRegionTags().copyTags(record.regions, B(record.headerLength), B(40), B(record.size - record.headerLength));
        auto indication = output->addTag<Ieee802154AddressInd>();
        indication->setSrcAddress(key.source); indication->setDestAddress(key.destination);
        indication->setSrcPanId(key.pan); indication->setDestPanId(key.pan);
        bool marked = record.marked;
        close(key, true); queueAck(key, UINT32_MAX, marked);
        if (owner.isNativeTransmissionOperational()) {
            owner.emit(owner.reassemblyCompletedSignal, output);
            if (owner.isNativeTransmissionOperational()) owner.deliverIpv6(output);
            else delete output;
        }
        else delete output;
    }
    else if (header->getAckRequested()) { auto bitmap = record.bitmap; auto marked = record.marked; record.marked = false; queueAck(key, bitmap, marked); }
}
void LowpanSelectiveRecovery::acceptRelay(const Key& key, const Ptr<const LowpanRfragHeader>& header, const Packet& packet)
{
    auto found = records.find(key); if (found == records.end()) return;
    auto& record = found->second;
    if (!isPathValid(record)) { sendMtuError(record); close(key, false, true); queueAck(key, 0); return; }
    int sequence = header->getSequence(), offset = sequence == 0 ? 0 : header->getFragmentOffset();
    auto shape = record.shapes[sequence];
    if ((sequence == 0 && header->getFragmentOffset() != record.size) || (sequence != 0 && offset < record.headerLength) ||
            offset + header->getFragmentSize() > record.size ||
            (shape.offset >= 0 && (shape.offset != offset || shape.size != header->getFragmentSize()))) { close(key, false, true); queueAck(key, 0); return; }
    auto bytes = sequence == 0 ? packet.peekDataAsBytes()->getBytes() : std::vector<uint8_t>();
    if (sequence == 0 && bytes != record.firstBytes) { close(key, false, true); queueAck(key, 0); return; }
    record.shapes[sequence] = {offset, header->getFragmentSize()};
    auto outgoing = makeShared<LowpanRfragHeader>(*header);
    outgoing->setDatagramTag(record.outgoingTag);
    outgoing->setFragmentOffset(sequence == 0 ? record.size + record.delta : offset + record.delta);
    outgoing->setFragmentSize(header->getFragmentSize() + (sequence == 0 ? record.delta : 0));
    Ptr<const Chunk> body;
    if (sequence == 0) {
        Packet data("rewritten-first", record.outputHeader);
        int tail = packet.getDataLength().get<B>() - record.headerLength;
        if (tail) data.insertAtBack(packet.peekDataAt(B(record.headerLength), B(tail)));
        body = data.peekData();
    }
    else body = packet.peekData();
    int pendingBytes = 0;
    for (const auto& frame : record.pending) pendingBytes += frame.body->getChunkLength().get<B>();
    if (record.pending.size() >= size_t(maxPending) || body->getChunkLength().get<B>() > maxPendingBytes - pendingBytes ||
            !hasCapacity(body->getChunkLength().get<B>(), false)) {
        owner.dropLowpanPacket(new Packet("recovery-relay-capacity", body), LOWPAN_RESOURCE_LIMIT); return;
    }
    Frame queued{outgoing, body, addTime(simTime(), residence), {}};
    auto taggedInput = std::unique_ptr<Packet>(packet.dup());
    int skip = sequence == 0 ? record.headerLength : 0;
    int outputSkip = sequence == 0 ? record.outputHeader->getChunkLength().get<B>() : 0;
    queued.regions.copyTags(taggedInput->getRegionTags(), packet.getFrontOffset() + B(skip), B(outputSkip), packet.getDataLength() - B(skip));
    record.pending.push_back(std::move(queued));
    sendNext(key);
}
void LowpanSelectiveRecovery::sendNext(const Key& key)
{
    auto found = records.find(key); if (found == records.end() || found->second.phase != RELAY) return;
    auto& record = found->second;
    if (!isPathValid(record)) { sendMtuError(record); close(key, false, true); queueAck(key, 0); return; }
    if (record.inFlight >= 0 || record.nextSend > simTime() || record.pending.empty()) return;
    auto frame = std::move(record.pending.front()); record.pending.pop_front();
    if (frame.deadline <= simTime()) { owner.dropLowpanPacket(new Packet("recovery-ingress-deadline", frame.body), LOWPAN_LOWER_LIFETIME_EXPIRED); return; }
    auto path = record.path;
    auto layer = check_and_cast<LowpanLayer *>(owner.getSimulation()->getModule(path.layerId));
    auto packet = std::make_unique<Packet>("lowpan-recovery-relay", frame.body); packet->insertAtFront(frame.header);
    packet->getRegionTags().copyTags(frame.regions, b(0), B(6), frame.body->getChunkLength());
    auto envelope = makeShared<LowpanTransmissionReq>(*layer->link->prepareTransmission(path.peer));
    envelope->setDeliveryDeadline(std::min(envelope->getDeliveryDeadline(), frame.deadline));
    int limit = std::min(envelope->getPayloadLimit(), pathPayloadLimit);
    if (payloadLimit) limit = std::min(limit, payloadLimit);
    if (layer->selectiveRecovery->payloadLimit) limit = std::min(limit, layer->selectiveRecovery->payloadLimit);
    if (packet->getDataLength() > B(limit)) { close(key, false, true); queueAck(key, 0); return; }
    *packet->addTag<LowpanTransmissionReq>() = *envelope; packet->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    auto id = packet->getId(), token = record.token;
    record.inFlight = id; record.inFlightBytes = frame.body->getChunkLength().get<B>();
    int sequence = frame.header->getSequence();
    // Track submitted coverage in incoming coordinates; first-header delta is frozen separately.
    record.submittedShapes[sequence] = {sequence == 0 ? 0 : int(frame.header->getFragmentOffset()) - record.delta,
        int(frame.header->getFragmentSize()) - (sequence == 0 ? record.delta : 0)};
    auto observation = std::unique_ptr<Packet>(packet->dup());
    owner.emit(owner.fragmentSentSignal, observation.get());
    found = records.find(key);
    if (found == records.end() || found->second.phase != RELAY || found->second.inFlight != id) return;
    if (!isPathValid(found->second)) { sendMtuError(found->second); close(key, false, true); queueAck(key, 0); return; }
    bool accepted = layer->pushForwardedPacket(packet.release(), owner.getId(), token, true);
    if (!accepted) handleTransmissionCompleted(token, id, false);
}
void LowpanSelectiveRecovery::handleTransmissionCompleted(int64_t token, int64_t frameId, bool successful)
{
    cMethodCallContextSwitcher context(&owner); context.methodCallSilent();
    if (token < 0) { controlFrames.erase(frameId); return; }
    auto key = findToken(token); if (!key) return;
    auto found = records.find(*key);
    if (found->second.phase == SOURCE) { sources->handleTransmissionCompleted(token, frameId, successful); schedule(); return; }
    if (found->second.phase != RELAY || found->second.inFlight != frameId) return;
    found->second.inFlight = -1; found->second.inFlightBytes = 0;
    if (!isPathValid(found->second)) { sendMtuError(found->second); close(*key, false, true); queueAck(*key, 0); }
    else found->second.nextSend = addTime(simTime(), gap);
    schedule();
}
void LowpanSelectiveRecovery::handleInterfaceUnavailable(int moduleId)
{
    cMethodCallContextSwitcher context(&owner); context.methodCallSilent();
    if (moduleId == owner.networkInterface->getId()) { controls.clear(); controlFrames.clear(); sources->clear(); }
    std::vector<Key> keys;
    for (const auto& item : records)
        if (moduleId == owner.networkInterface->getId() || item.second.path.interfaceId == moduleId) keys.push_back(item.first);
    for (const auto& key : keys) {
        auto found = records.find(key);
        if (found != records.end()) removeReverse(found->second.path.layerId, found->second.outgoingTag, found->second.token);
        close(key, false);
    }
    schedule();
}
void LowpanSelectiveRecovery::process(Packet *packet, bool acknowledgement)
{
    expire();
    auto observation = std::unique_ptr<Packet>(packet->dup());
    owner.emit(owner.fragmentReceivedSignal, observation.get());
    if (!owner.isNativeTransmissionOperational()) { owner.dropPacket(packet, INTERFACE_DOWN); return; }
    if (acknowledgement) { processAck(packet); schedule(); return; }
    if (packet->getDataLength() < B(6)) { owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return; }
    auto header = packet->popAtFront<LowpanRfragHeader>(B(6), ALLOW_MALFORMED);
    auto peer = packet->getTag<Ieee802154AddressInd>();
    Key key{peer->getSrcAddress(), peer->getDestAddress(), peer->getSrcPanId(), header->getDatagramTag()};
    if (!header->isCorrect() || key.destination.isBroadcast()) { owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return; }
    if (header->getFragmentOffset() == 0) { processReset(packet, header, key); schedule(); return; }
    if (header->getFragmentSize() == 0 || packet->getDataLength() != B(header->getFragmentSize())) {
        owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return;
    }
    auto found = records.find(key);
    if (found != records.end() && (found->second.phase == FULL || found->second.phase == ABORT)) {
        if (header->getAckRequested() || found->second.phase == ABORT) queueAck(key, found->second.phase == FULL ? UINT32_MAX : 0, header->getEcn());
        delete packet; schedule(); return;
    }
    if (found == records.end()) {
        if (header->getSequence() != 0) { queueAck(key, 0); owner.dropLowpanPacket(packet, LOWPAN_RECOVERY_ORPHAN); return; }
        Ptr<Ipv6Header> ipv6Header; int length;
        int size = header->getFragmentOffset();
        if (!decodeFirst(*packet, size, key, ipv6Header, length) || size < header->getFragmentSize()) {
            queueAck(key, 0); owner.dropLowpanPacket(packet, LOWPAN_MALFORMED); return;
        }
        auto quote = std::make_unique<Packet>("recovery-first-quote", ipv6Header);
        if (packet->getDataLength() > B(length)) quote->insertAtBack(packet->peekDataAt(B(length), packet->getDataLength() - B(length)));
        bool local = routes->isLocalAddress(ipv6Header->getDestAddress());
        Path path;
        bool valid = local || (inspectTransit(*quote) && ipv6Header->getHopLimit() > 1 && findPath(ipv6Header->getSrcAddress(), ipv6Header->getDestAddress(), path, ipv6Header->getPayloadLength().get<B>() + 40));
        if (!valid) {
            protect(key);
            queueAck(key, 0);
            if (ipv6Header->getHopLimit() <= 1) sendError(*quote, ICMPv6_TIME_EXCEEDED, 0);
            else {
                auto route = routes->doLongestPrefixMatch(ipv6Header->getDestAddress());
                if (!route || (route->getExpiryTime() != SIMTIME_ZERO && route->getExpiryTime() <= simTime()))
                    sendError(*quote, ICMPv6_DESTINATION_UNREACHABLE, 0);
                else if (route->getInterface() && ipv6Header->getPayloadLength().get<B>() + 40 > route->getInterface()->getMtu())
                    sendError(*quote, ICMPv6_PACKET_TOO_BIG, 0, route->getInterface()->getMtu());
            }
            owner.dropLowpanPacket(packet, LOWPAN_RECOVERY_ABORTED); schedule(); return;
        }
        Record record; record.phase = local ? RECEIVER : RELAY; record.token = nextToken++;
        record.activeEnd = addTime(simTime(), incompleteLifetime); record.historyEnd = addTime(simTime(), historyLifetime);
        record.size = size; record.headerLength = length; record.ipv6 = ipv6Header; record.nextSend = simTime();
        int charge = local ? size : packet->getDataLength().get<B>();
        if (!local) {
            auto routing = makeShared<Ipv6Header>(*ipv6Header); routing->setHopLimit(routing->getHopLimit() - 1);
            record.outputHeader = LowpanIphcCodec::encode(*routing, path.peer.getSrcAddress(), path.peer.getDestAddress());
            record.delta = record.outputHeader->getChunkLength().get<B>() - length;
            int outgoingSize = size + record.delta, outgoingFirst = header->getFragmentSize() + record.delta;
            auto layer = check_and_cast<LowpanLayer *>(owner.getSimulation()->getModule(path.layerId));
            int limit = std::min(pathPayloadLimit, layer->link->prepareTransmission(path.peer)->getPayloadLimit());
            if (payloadLimit) limit = std::min(limit, payloadLimit);
            if (layer->selectiveRecovery->payloadLimit) limit = std::min(limit, layer->selectiveRecovery->payloadLimit);
            if (outgoingSize < 1 || outgoingSize > 65535 || outgoingFirst < record.outputHeader->getChunkLength().get<B>() ||
                    outgoingFirst > 1023 || outgoingFirst + 6 > limit) {
                protect(key); queueAck(key, 0); owner.dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT); schedule(); return;
            }
            charge += record.outputHeader->getChunkLength().get<B>();
            record.path = path;
        }
        if (!hasCapacity(charge, true)) { protect(key); queueAck(key, 0); owner.dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT); schedule(); return; }
        if (local) { record.bytes.resize(size); record.covered.resize(size); }
        else {
            auto layer = check_and_cast<LowpanLayer *>(owner.getSimulation()->getModule(path.layerId));
            auto tag = layer->selectiveRecovery->allocateTag(owner.getId(), record.token, path.peer);
            if (!tag) { protect(key); queueAck(key, 0); owner.dropLowpanPacket(packet, LOWPAN_TAG_EXHAUSTED); schedule(); return; }
            record.outgoingTag = *tag; record.firstBytes = packet->peekDataAsBytes()->getBytes();
        }
        found = records.emplace(key, std::move(record)).first;
    }
    if (found->second.phase == RECEIVER) acceptReceiver(key, header, *packet);
    else if (found->second.phase == RELAY) acceptRelay(key, header, *packet);
    else queueAck(key, 0);
    delete packet; schedule();
}
} }
