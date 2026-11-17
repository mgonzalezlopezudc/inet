// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanLayer.h"
#include "inet/linklayer/lowpan/LowpanSourceFragmentation.h"
#include "inet/linklayer/lowpan/LowpanFragmentForwarding.h"
#include "inet/linklayer/lowpan/LowpanSelectiveRecovery.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/linklayer/lowpan/LowpanMeshHeader_m.h"
#include "inet/linklayer/lowpan/LowpanBroadcastHeader_m.h"
#include "inet/linklayer/lowpan/LowpanExtendedDispatchHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFragmenter.h"
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"
#include "inet/linklayer/lowpan/LowpanNhcCodec.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/NextHopAddressTag_m.h"
#include "inet/networklayer/common/Icmpv6Type_m.h"
#include "inet/networklayer/ipv6/Ipv6ExtensionHeaders.h"
#include "inet/networklayer/ipv6/Ipv6Header_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

namespace inet { namespace lowpan {
namespace {
simtime_t meshClockDuration(double seconds)
{
    return seconds >= SIMTIME_MAX.dbl() ? SIMTIME_MAX : SimTime(seconds);
}

struct MeshPayload {
    bool fragmented;
    uint16_t size = 0;
    uint16_t tag = 0;
    uint8_t offset = 0;
};

std::optional<MeshPayload> inspectMeshPayload(const Packet& packet, LowpanDispatchPrefix::State state)
{
    using Prefix = LowpanDispatchPrefix;
    auto prefix = Prefix::parse(packet, 0, state);
    if (prefix.kind == Prefix::MALFORMED || prefix.kind == Prefix::MESH || prefix.kind == Prefix::BROADCAST)
        return std::nullopt;
    auto bytes = packet.peekDataAsBytes()->getBytes();
    int start = prefix.pagingLength;
    bool first = prefix.kind == Prefix::FRAG1;
    bool following = prefix.kind == Prefix::FRAGN;
    MeshPayload result{first || following};
    if (!result.fragmented) return result; // Unknown inner dispatch remains opaque in transit.
    int header = first ? 4 : 5;
    if (int(bytes.size()) <= start + header) return std::nullopt;
    result.size = ((bytes[start] & 7) << 8) | bytes[start + 1];
    result.tag = (bytes[start + 2] << 8) | bytes[start + 3];
    if (following) result.offset = bytes[start + 4];
    auto payload = bytes.size() - start - header;
    auto inner = first ? Prefix::parse(packet, start + header, prefix.state).kind : Prefix::UNKNOWN;
    if (result.size < 40 || (following && (result.offset == 0 || 8 * result.offset >= result.size ||
            8 * result.offset + payload > result.size || (8 * result.offset + payload < result.size && payload % 8 != 0))) ||
            (first && (inner == Prefix::MALFORMED || inner == Prefix::MESH || inner == Prefix::BROADCAST ||
                inner == Prefix::FRAG1 || inner == Prefix::FRAGN)))
        return std::nullopt;
    return result;
}

bool isRouterDiscovery(const Packet *packet, bool includeOtherNdControl = false)
{
    try {
        auto protocol = packet->peekAtFront<Ipv6Header>(B(40))->getProtocolId();
        b offset = B(40);
        while (isIpv6ExtensionHeader(protocol)) {
            auto remaining = packet->getDataLength() - offset;
            if (remaining < B(8))
                return false;
            auto extension = peekIpv6ExtensionHeaderAt(packet, offset, protocol);
            auto length = extension->getChunkLength();
            if (length < B(8) || length > remaining || length.get() % 8 != 0)
                return false;
            if (auto fragment = dynamicPtrCast<const Ipv6FragmentHeader>(extension))
                if (fragment->getFragmentOffset() != 0)
                    return false;
            protocol = extension->getNextHeaderProtocol();
            offset += length;
        }
        if (protocol != IP_PROT_IPv6_ICMP || offset >= packet->getDataLength())
            return false;
        auto type = packet->peekDataAt<BytesChunk>(offset, B(1))->getBytes()[0];
        return type == ICMPv6_ROUTER_SOL || type == ICMPv6_ROUTER_AD ||
            (includeOtherNdControl && ((type >= ICMPv6_NEIGHBOUR_SOL && type <= ICMPv6_REDIRECT) ||
                type == ICMPv6_DUPLICATE_ADDRESS_REQUEST || type == ICMPv6_DUPLICATE_ADDRESS_CONFIRMATION));
    }
    catch (const cRuntimeError&) {
        return false; // Inspection must not replace the existing malformed-packet handling.
    }
}

bool isRecoveryData(const Packet *packet)
{
    // Admission requires positive classification; inability to inspect is not evidence of non-ND data.
    try {
        auto next = packet->peekAtFront<Ipv6Header>(B(40))->getProtocolId();
        b offset = B(40);
        while (isIpv6ExtensionHeader(next)) {
            if (packet->getDataLength() - offset < B(8)) return false;
            auto extension = peekIpv6ExtensionHeaderAt(packet, offset, next);
            if (!extension->isCorrect() || extension->getChunkLength() < B(8) ||
                    extension->getChunkLength() > packet->getDataLength() - offset) return false;
            if (auto fragment = dynamicPtrCast<const Ipv6FragmentHeader>(extension))
                if (fragment->getFragmentOffset() != 0) return false;
            next = extension->getNextHeaderProtocol(); offset += extension->getChunkLength();
        }
        if (next != IP_PROT_IPv6_ICMP) return true;
        if (offset >= packet->getDataLength()) return false;
        auto type = packet->peekDataAt<BytesChunk>(offset, B(1))->getBytes()[0];
        return !((type >= 133 && type <= 137) || type == 157 || type == 158);
    }
    catch (const cRuntimeError&) { return false; }
}
}

Define_Module(LowpanLayer);
simsignal_t LowpanLayer::datagramAcceptedSignal = registerSignal("datagramAccepted");
simsignal_t LowpanLayer::datagramCompletedSignal = registerSignal("datagramCompleted");
simsignal_t LowpanLayer::fragmentSentSignal = registerSignal("fragmentSent");
simsignal_t LowpanLayer::fragmentReceivedSignal = registerSignal("fragmentReceived");
simsignal_t LowpanLayer::reassemblyCompletedSignal = registerSignal("reassemblyCompleted");
simsignal_t LowpanLayer::reassemblyExpiredSignal = registerSignal("reassemblyExpired");
simsignal_t LowpanLayer::reassemblyInvalidatedSignal = registerSignal("reassemblyInvalidated");
simsignal_t LowpanLayer::meshForwardedSignal = registerSignal("meshForwarded");
simsignal_t LowpanLayer::nativeFrameTransmissionSucceededSignal = registerSignal("nativeFrameTransmissionSucceeded");
simsignal_t LowpanLayer::nativeFrameTransmissionFailedSignal = registerSignal("nativeFrameTransmissionFailed");

LowpanLayer::LowpanLayer() = default;

LowpanLayer::~LowpanLayer()
{
    cancelAndDelete(expiryTimer);
}

void LowpanLayer::initialize(int stage)
{
    OperationalMixin<queueing::PacketProcessorBase>::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        consumer.reference(gate("lowerLayerOut"), true);
        link = check_and_cast<ILowpanLink *>(consumer.get());
        networkInterface = getContainingNicModule(this);
        reassemblyTimeout = par("reassemblyTimeout");
        std::string compression = par("headerCompression").stdstringValue();
        if (compression != "none" && compression != "iphc")
            throw cRuntimeError("Unsupported LoWPAN headerCompression profile");
        useIphc = compression == "iphc";
        meshUnder = par("meshUnder");
        if (par("nativeSelectiveRecovery").boolValue()) {
            if (!useIphc || meshUnder || dynamic_cast<LowpanNativeBoundary *>(link) == nullptr)
                throw cRuntimeError("Native selective recovery requires IPHC and a native boundary without Mesh");
            selectiveRecovery = std::make_unique<LowpanSelectiveRecovery>(*this);
            WATCH_EXPR("numRecoveryRecords", selectiveRecovery->getNumRecords());
            WATCH_EXPR("recoveryRetainedBytes", selectiveRecovery->getRetainedBytes());
        }
        if (par("nativeFragmentPacing").boolValue()) {
            if (meshUnder || dynamic_cast<LowpanNativeBoundary *>(link) == nullptr)
                throw cRuntimeError("Paced source fragmentation requires a native boundary without Mesh");
            sourceFragments = std::make_unique<LowpanSourceFragmentation>(*this);
            WATCH_EXPR("numPacedSourceDatagrams", sourceFragments->getNumDatagrams());
            WATCH_EXPR("pacedSourceBytes", sourceFragments->getRetainedBytes());
        }
        if (par("nativeFragmentForwarding").boolValue()) {
            if (!sourceFragments || meshUnder || dynamic_cast<LowpanNativeBoundary *>(link) == nullptr)
                throw cRuntimeError("Native fragment forwarding requires pacing and native mode without Mesh");
            fragmentForwarding = std::make_unique<LowpanFragmentForwarding>(*this);
            WATCH_EXPR("numFragmentForwardingRecords", fragmentForwarding->getNumRecords());
            WATCH_EXPR("fragmentForwardingBytes", fragmentForwarding->getBufferedBytes());
        }
        int hops = par("meshHops");
        double residenceSeconds = par("meshResidence").doubleValue();
        double retentionSeconds = par("broadcastRetention").doubleValue();
        double tickSeconds = SimTime::fromRaw(1).dbl();
        maxBroadcastDatagrams = par("maxBroadcastDatagrams");
        if (hops < 0 || hops > 255 || !std::isfinite(residenceSeconds) || residenceSeconds < tickSeconds ||
                !std::isfinite(retentionSeconds) || retentionSeconds < tickSeconds || maxBroadcastDatagrams <= 0)
            throw cRuntimeError("Invalid LoWPAN mesh limits: durations must be finite and at least one simulation tick");
        meshResidence = meshClockDuration(residenceSeconds);
        broadcastRetention = meshClockDuration(retentionSeconds);
        meshHops = hops;
        for (auto route : par("meshRoutes").xmlValue()->getChildren()) {
            auto final = route->getAttribute("final"), next = route->getAttribute("nextHop");
            if (strcmp(route->getTagName(), "route") || !final || !next ||
                    !meshRoutes.emplace(Ieee802154Address(final), Ieee802154Address(next)).second)
                throw cRuntimeError("Invalid or duplicate LoWPAN mesh route");
        }
        WATCH_EXPR("numBroadcastDatagrams", broadcasts.size());
        WATCH_EXPR("numPendingNativeTransmissions", pendingNativeTransmissions.size());
        reassembly = std::make_unique<LowpanReassemblyTable>(par("maxReassemblyDatagrams"), par("maxReassemblyBytes"), reassemblyTimeout);
        expiryTimer = new cMessage("lowpanReassemblyExpiry");
        std::array<bool, 16> seen{};
        auto integer = [](cXMLElement *element, const char *attribute, int maximum) {
            auto text = element->getAttribute(attribute);
            if (text == nullptr || *text == '\0')
                throw cRuntimeError("LoWPAN context requires attribute '%s'", attribute);
            char *end = nullptr;
            long value = std::strtol(text, &end, 10);
            if (*end != '\0' || value < 0 || value > maximum)
                throw cRuntimeError("Invalid LoWPAN context attribute '%s'", attribute);
            return int(value);
        };
        for (auto element : par("compressionContexts").xmlValue()->getChildren()) {
            if (strcmp(element->getTagName(), "context") != 0)
                throw cRuntimeError("Unknown LoWPAN compression context element");
            int id = integer(element, "id", 15);
            int prefixLength = integer(element, "prefixLength", 128);
            auto prefix = element->getAttribute("prefix");
            auto permission = element->getAttribute("compress");
            auto until = element->getAttribute("validUntil");
            if (seen[id] || prefix == nullptr ||
                (permission != nullptr && strcmp(permission, "true") != 0 && strcmp(permission, "false") != 0))
                throw cRuntimeError("Invalid or duplicate LoWPAN compression context");
            compressionContexts.install(id, Ipv6Address(prefix), prefixLength,
                permission == nullptr || strcmp(permission, "true") == 0, until ? SimTime::parse(until) : SimTime::getMaxTime());
            configuredContexts[id] = true;
            seen[id] = true;
        }
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        if (networkInterface->getMtu() != 1280 || networkInterface->getProtocol() != &lowpanProtocol)
            throw cRuntimeError("LoWPAN requires the native link contract and IPv6 MTU 1280");
        if (meshUnder) {
            auto native = dynamic_cast<LowpanNativeBoundary *>(link);
            if (!native) throw cRuntimeError("LoWPAN mesh-under requires a native interface");
            for (const auto& route : meshRoutes)
                if (route.first.getMode() != Ieee802154Address::EXTENDED || route.first.getInt() == UINT64_MAX ||
                        route.second.getMode() != Ieee802154Address::EXTENDED || route.second == link->getLinkAddress() ||
                        !native->getLinkDomain()->contains(route.second))
                    throw cRuntimeError("LoWPAN mesh route requires a valid extended final and native next hop");
        }
    }
    else if (stage == INITSTAGE_QUEUEING) {
        checkPacketOperationSupport(gate("lowerLayerOut"));
        for (auto gate = this->gate("lowerLayerIn")->getPathStartGate(); gate != nullptr; gate = gate->getNextGate())
            if (gate->getChannel() != nullptr)
                throw cRuntimeError("LoWPAN deadline profile requires an undelayed receive boundary");
    }
    else if (stage == INITSTAGE_LAST && fragmentForwarding) fragmentForwarding->validatePeers();
}

void LowpanLayer::setCompressionContext(int id, const Ipv6Address& prefix, int prefixLength,
        bool allowCompression, simtime_t validUntil)
{
    Enter_Method("setCompressionContext");
    installContext(id, prefix, prefixLength, allowCompression, validUntil, validUntil);
    configuredContexts.at(id) = true;
}

bool LowpanLayer::isConfiguredContext(int id) const
{
    Enter_Method_Silent();
    return id >= 0 && id < 16 && configuredContexts[id];
}

const LowpanContextTable::Context *LowpanLayer::getCompressionContext(int id) const
{
    Enter_Method_Silent();
    return compressionContexts.get(id);
}

void LowpanLayer::setLearnedContext(int id, const Ipv6Address& prefix, int prefixLength,
        bool allowCompression, simtime_t compressionUntil, simtime_t validUntil)
{
    Enter_Method("setLearnedContext");
    if (isConfiguredContext(id))
        throw cRuntimeError("Learned LoWPAN context overlaps configured CID %d", id);
    installContext(id, prefix, prefixLength, allowCompression, compressionUntil, validUntil);
}

void LowpanLayer::installContext(int id, const Ipv6Address& prefix, int prefixLength,
        bool allowCompression, simtime_t compressionUntil, simtime_t validUntil)
{
    if (compressionContexts.install(id, prefix, prefixLength, allowCompression, compressionUntil, validUntil)) {
        long count = reassembly->getNumContexts();
        reassembly->clear();
        cancelEvent(expiryTimer);
        if (count != 0)
            emit(reassemblyInvalidatedSignal, count);
    }
}

void LowpanLayer::removeLearnedContext(int id)
{
    Enter_Method("removeLearnedContext");
    if (isConfiguredContext(id))
        return;
    if (auto context = compressionContexts.get(id)) {
        auto prefix = context->prefix;
        auto length = context->prefixLength;
        compressionContexts.install(id, prefix, length, false, SIMTIME_ZERO, SIMTIME_ZERO);
    }
}

void LowpanLayer::clearTransientState()
{
    notifyForwardingUnavailable();
    forwardedTransmissions.clear();
    if (sourceFragments) sourceFragments->clear();
    pendingNativeTransmissions.clear();
    cancelEvent(expiryTimer);
    reassembly->clear();
    // Tag guards deliberately outlive interface teardown.
}

bool LowpanLayer::isNativeTransmissionOperational() const
{
    auto native = dynamic_cast<const LowpanNativeBoundary *>(link);
    return !native || (operationalState == OPERATING && native->isTransmissionOperational());
}

bool LowpanLayer::canPushPreparedPacket(Packet *packet) const
{
    return isNativeTransmissionOperational() && consumer.canPushPacket(packet);
}

PacketDropReason LowpanLayer::preparedRefusalReason() const
{
    return isNativeTransmissionOperational() ? QUEUE_OVERFLOW : INTERFACE_DOWN;
}

bool LowpanLayer::pushPreparedPacket(Packet *packet)
{
    if (!canPushPreparedPacket(packet)) {
        dropPacket(packet, preparedRefusalReason());
        return false;
    }
    if (dynamic_cast<LowpanNativeBoundary *>(link))
        pendingNativeTransmissions.insert(packet->getId());
    // Queue pull and terminal failure can synchronously delete this original.
    consumer.pushPacket(packet);
    return true;
}

std::optional<uint16_t> LowpanLayer::allocateDatagramTag(simtime_t now, simtime_t reuseAfter)
{
    return fragmentForwarding ? tagAllocator.allocate(now, reuseAfter, uint16_t(intrand(65536))) : tagAllocator.allocate(now, reuseAfter);
}

bool LowpanLayer::pushForwardedPacket(Packet *packet, int ownerModuleId, int64_t token, bool recovery)
{
    Enter_Method("pushForwardedPacket");
    take(packet);
    auto id = packet->getId();
    forwardedTransmissions.emplace(id, ForwardedTransmission{ownerModuleId, token, recovery});
    bool accepted = pushPreparedPacket(packet);
    if (!accepted) forwardedTransmissions.erase(id);
    return accepted; // The original may have been deleted synchronously.
}

void LowpanLayer::notifyForwardingUnavailable()
{
    if (!fragmentForwarding && !selectiveRecovery) return;
    std::vector<int> layers;
    auto table = networkInterface->getInterfaceTable();
    for (int i = 0; i < table->getNumInterfaces(); i++)
        for (cModule::SubmoduleIterator it(table->getInterface(i)); !it.end(); ++it)
            if (auto layer = dynamic_cast<LowpanLayer *>(*it)) layers.push_back(layer->getId());
    for (auto id : layers)
        if (auto layer = dynamic_cast<LowpanLayer *>(getSimulation()->getModule(id))) {
            if (layer->fragmentForwarding) layer->fragmentForwarding->handleInterfaceUnavailable(networkInterface->getId());
            if (layer->selectiveRecovery) layer->selectiveRecovery->handleInterfaceUnavailable(networkInterface->getId());
        }
}

void LowpanLayer::handlePushPacketProcessed(Packet *packet, const cGate *, bool successful)
{
    Enter_Method("handlePushPacketProcessed");
    auto id = packet->getId();
    std::optional<ForwardedTransmission> forwarded;
    auto context = forwardedTransmissions.find(id);
    if (context != forwardedTransmissions.end()) { forwarded = context->second; forwardedTransmissions.erase(context); }
    auto found = pendingNativeTransmissions.find(id);
    if (found == pendingNativeTransmissions.end()) return;
    pendingNativeTransmissions.erase(found);
    // STOPPING/CRASHING still satisfy isUp(); neither side may continue during drain.
    if (!isNativeTransmissionOperational()) {
        if (sourceFragments) sourceFragments->completed(id, false);
        if (forwarded)
            if (auto layer = dynamic_cast<LowpanLayer *>(getSimulation()->getModule(forwarded->ownerModuleId))) {
                if (forwarded->recovery) {
                    if (layer->selectiveRecovery) layer->selectiveRecovery->handleTransmissionCompleted(forwarded->token, id, false);
                }
                else if (layer->fragmentForwarding) layer->fragmentForwarding->handleTransmissionCompleted(forwarded->token, id, false);
            }
        return;
    }
    emit(successful ? nativeFrameTransmissionSucceededSignal : nativeFrameTransmissionFailedSignal, packet);
    if (sourceFragments) sourceFragments->completed(id, successful && isNativeTransmissionOperational());
    if (forwarded)
        if (auto layer = dynamic_cast<LowpanLayer *>(getSimulation()->getModule(forwarded->ownerModuleId))) {
            if (forwarded->recovery) {
                if (layer->selectiveRecovery) layer->selectiveRecovery->handleTransmissionCompleted(forwarded->token, id, successful && isNativeTransmissionOperational());
            }
            else if (layer->fragmentForwarding) layer->fragmentForwarding->handleTransmissionCompleted(forwarded->token, id, successful && isNativeTransmissionOperational());
        }
}

void LowpanLayer::handleStopOperation(LifecycleOperation *operation)
{
    clearTransientState();
}

void LowpanLayer::handleCrashOperation(LifecycleOperation *operation)
{
    clearTransientState();
}

void LowpanLayer::scheduleExpiry()
{
    cancelEvent(expiryTimer);
    if (reassembly->getNumContexts() != 0)
        scheduleAt(reassembly->getNextExpiry(), expiryTimer);
}

void LowpanLayer::expireReassembly()
{
    auto expired = reassembly->expire(simTime());
    if (expired != 0) emit(reassemblyExpiredSignal, (long)expired);
    scheduleExpiry();
}

void LowpanLayer::handleMessageWhenDown(cMessage *message)
{
    if (selectiveRecovery && selectiveRecovery->isTimer(message)) { selectiveRecovery->handleTimer(); return; }
    if (fragmentForwarding && fragmentForwarding->isTimer(message)) { fragmentForwarding->handleTimer(); return; }
    dropPacket(check_and_cast<Packet *>(message), INTERFACE_DOWN);
}

void LowpanLayer::handleMessageWhenUp(cMessage *message)
{
    if (selectiveRecovery && selectiveRecovery->isTimer(message)) { selectiveRecovery->handleTimer(); return; }
    if (fragmentForwarding && fragmentForwarding->isTimer(message)) { fragmentForwarding->handleTimer(); return; }
    if (sourceFragments && sourceFragments->isTimer(message)) { sourceFragments->handleTimer(); return; }
    if (message == expiryTimer) {
        expireReassembly();
        return;
    }
    auto packet = check_and_cast<Packet *>(message);
    if (packet->getArrivalGate() == gate("upperLayerIn"))
        processUpperPacket(packet);
    else if (packet->getArrivalGate() == gate("lowerLayerIn"))
        processLowerPacket(packet);
    else
        throw cRuntimeError("Unexpected LoWPAN arrival gate");
}

bool LowpanLayer::isValidIpv6Packet(const Packet *packet) const
{
    if (packet->getDataLength() < B(40) || packet->getDataLength().get() % 8 != 0)
        return false;
    try {
        const auto& header = packet->peekAtFront<Ipv6Header>(B(40));
        return !header->isIncorrect() && header->getVersion() == 6 &&
               B(40) + header->getPayloadLength() == packet->getDataLength();
    }
    catch (const cRuntimeError&) {
        return false;
    }
}

void LowpanLayer::dropLowpanPacket(Packet *packet, LowpanDropReason reason)
{
    LowpanPacketDropDetails details;
    details.setReason(OTHER_PACKET_DROP);
    details.setLowpanReason(reason);
    emit(packetDroppedSignal, packet, &details);
    delete packet;
}

void LowpanLayer::removeRequestTags(Packet *packet) const
{
    packet->removeTagIfPresent<MacAddressReq>();
    packet->removeTagIfPresent<MacAddressInd>();
    packet->removeTagIfPresent<InterfaceReq>();
    packet->removeTagIfPresent<NextHopAddressReq>();
    packet->removeTagIfPresent<DispatchProtocolReq>();
    packet->removeTagIfPresent<DispatchProtocolInd>();
    packet->removeTagIfPresent<Ieee802154AddressReq>();
    packet->removeTagIfPresent<LowpanTransmissionReq>();
}

void LowpanLayer::processUpperPacket(Packet *packet)
{
    auto protocol = packet->findTag<PacketProtocolTag>();
    if (protocol == nullptr || protocol->getProtocol() != &Protocol::ipv6 || !isValidIpv6Packet(packet)) {
        dropLowpanPacket(packet, LOWPAN_MALFORMED);
        return;
    }
    if (packet->findTag<SignalBitrateReq>() != nullptr) {
        dropLowpanPacket(packet, LOWPAN_ENVELOPE_INVALIDATED);
        return;
    }
    Ieee802154Address destination;
    auto explicitRequest = packet->findTag<Ieee802154AddressReq>();
    auto native = dynamic_cast<LowpanNativeBoundary *>(link);
    bool multicast = packet->peekAtFront<Ipv6Header>()->getDestAddress().isMulticast();
    if (explicitRequest != nullptr && native != nullptr) {
        destination = explicitRequest->getDestAddress();
        if (explicitRequest->getSrcAddress() != link->getLinkAddress() ||
                explicitRequest->getSrcPanId() != link->getPanId() || explicitRequest->getDestPanId() != link->getPanId() ||
                (multicast ? !destination.isBroadcast() : !native->getLinkDomain()->contains(destination))) {
            dropLowpanPacket(packet, LOWPAN_MALFORMED);
            return;
        }
    }
    else if (multicast)
        destination = Ieee802154Address("ffff");
    else {
        auto nextHop = packet->findTag<NextHopAddressReq>();
        auto neighbor = nextHop != nullptr && nextHop->getNextHopAddress().getType() == L3Address::IPv6 ?
            link->resolveNeighbor(nextHop->getNextHopAddress().toIpv6()) : nullptr;
        if (neighbor == nullptr) {
            dropPacket(packet, ADDRESS_RESOLUTION_FAILED);
            return;
        }
        destination = *neighbor;
    }
    Ieee802154AddressReq request;
    auto finalDestination = destination;
    if (meshUnder && !destination.isBroadcast()) {
        auto route = meshRoutes.find(destination);
        if (route == meshRoutes.end()) {
            dropLowpanPacket(packet, LOWPAN_MESH_NO_ROUTE);
            return;
        }
        destination = route->second;
    }
    if (meshUnder && meshHops == 0) {
        dropLowpanPacket(packet, LOWPAN_MESH_HOP_EXHAUSTED);
        return;
    }
    request.setSrcAddress(link->getLinkAddress());
    request.setDestAddress(destination);
    request.setSrcPanId(link->getPanId());
    request.setDestPanId(link->getPanId());
    auto prepared = link->prepareTransmission(request);
    Ptr<const LowpanMeshHeader> mesh;
    Ptr<const LowpanBroadcastHeader> broadcast;
    if (meshUnder) {
        auto envelope = makeShared<LowpanTransmissionReq>(*prepared);
        envelope->setDeliveryDeadline(std::min(prepared->getDeliveryDeadline(), meshDeadline(meshResidence)));
        prepared = envelope;
        auto header = makeShared<LowpanMeshHeader>();
        header->setOriginator(link->getLinkAddress());
        header->setFinalDestination(finalDestination);
        header->setHopsLeft(meshHops);
        header->setDeepHops(meshHops >= 15);
        header->setChunkLength(B(1 + (meshHops >= 15 ? 1 : 0) + 8 + (finalDestination.isBroadcast() ? 2 : 8)));
        mesh = header;
    }
    int outerLength = mesh ? mesh->getChunkLength().get<B>() + (finalDestination.isBroadcast() ? 2 : 0) : 0;
    int frameBudget = prepared->getPayloadLimit() - outerLength;
    int size = packet->getDataLength().get<B>();
    Ptr<const Chunk> encodedHeader = makeShared<LowpanHeader>();
    int originalHeaderLength = 0;
    // Discovery establishes context knowledge; both encoding passes must bootstrap without it.
    const LowpanContextTable *contexts = useIphc && isRouterDiscovery(packet) ? nullptr : &compressionContexts;
    if (useIphc) {
        auto iphc = LowpanIphcCodec::encode(*packet->peekAtFront<Ipv6Header>(), request.getSrcAddress(), finalDestination, false, contexts, simTime());
        if (iphc != nullptr) {
            int length = iphc->getChunkLength().get<B>();
            // RFC 6282 section 4: compressed headers must fit wholly in FRAG1.
            // Uncompressed dispatch remains valid when its IPv6 bytes must span frames.
            if (size - 40 + length <= frameBudget || length + 4 <= frameBudget) {
                encodedHeader = iphc;
                originalHeaderLength = 40;
            }
        }
    }
    if (useIphc && originalHeaderLength == 40) {
        auto chain = LowpanNhcCodec::encode(*packet, request.getSrcAddress(), finalDestination, contexts, simTime());
        if (chain.header && (size - chain.originalLength + chain.encodedLength <= frameBudget ||
                chain.encodedLength + 4 <= frameBudget)) {
            encodedHeader = chain.header;
            originalHeaderLength = chain.originalLength;
        }
    }
    int encodedLength = encodedHeader->getChunkLength().get<B>();
    if (selectiveRecovery && !destination.isBroadcast() && isRecoveryData(packet) &&
            size - originalHeaderLength + encodedLength > frameBudget) {
        selectiveRecovery->admit(packet, request, prepared->getPayloadLimit());
        return;
    }
    auto ranges = LowpanFragmenter::plan(size, frameBudget, frameBudget, encodedLength, originalHeaderLength);
    if (ranges.empty()) {
        dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT);
        return;
    }
    if (mesh && finalDestination.isBroadcast()) {
        if (sequenceReservations[nextBroadcastSequence] > simTime()) {
            dropLowpanPacket(packet, LOWPAN_BROADCAST_SEQUENCE_EXHAUSTED);
            return;
        }
        auto header = makeShared<LowpanBroadcastHeader>();
        header->setSequenceNumber(nextBroadcastSequence);
        sequenceReservations[nextBroadcastSequence++] = meshReservation(meshHops, broadcastRetention);
        broadcast = header;
    }
    auto prependOuter = [&](Packet *frame) {
        if (broadcast) frame->insertAtFront(broadcast);
        if (mesh) frame->insertAtFront(mesh);
    };
    if (ranges.size() > 1) {
        // Bootstrap/control retains immediate timing; ordinary native unicast uses the opt-in scheduler.
        if (sourceFragments && !destination.isBroadcast() && !isRouterDiscovery(packet, true)) {
            sourceFragments->admit(packet, encodedHeader, originalHeaderLength, ranges, request, prepared->getPayloadLimit());
            return;
        }
        // RFC 4944 receiver lifetime is at most 60 seconds. Use that maximum
        // even when this interface's own receive timeout is configured shorter.
        auto reuseAfter = meshUnder ? meshReservation(meshHops, meshClockDuration(60)) :
            fragmentForwarding ? SimTime::fromRaw(prepared->getDeliveryDeadline().raw() > SimTime::getMaxTime().raw() - meshClockDuration(60).raw() ?
                SimTime::getMaxTime().raw() : prepared->getDeliveryDeadline().raw() + meshClockDuration(60).raw()) : prepared->getDeliveryDeadline() + SimTime(60);
        auto tag = allocateDatagramTag(simTime(), reuseAfter);
        if (!tag) {
            dropLowpanPacket(packet, LOWPAN_TAG_EXHAUSTED);
            return;
        }
        bool accepted = false;
        for (const auto& range : ranges) {
            auto fragment = new Packet((std::string(packet->getName()) + "-lowpan-" + std::to_string(range.offset)).c_str());
            int payloadOffset = range.offset == 0 ? originalHeaderLength : range.offset;
            int payloadLength = range.length - (range.offset == 0 ? originalHeaderLength : 0);
            if (payloadLength != 0) {
                fragment->insertAtBack(packet->peekDataAt(B(payloadOffset), B(payloadLength)));
                fragment->copyRegionTags(*packet, packet->getFrontOffset() + B(payloadOffset), b(0), B(payloadLength));
            }
            if (range.offset == 0) {
                fragment->insertAtFront(encodedHeader);
                auto header = makeShared<LowpanFrag1Header>();
                header->setDatagramSize(size);
                header->setDatagramTag(*tag);
                fragment->insertAtFront(header);
            }
            else {
                auto header = makeShared<LowpanFragnHeader>();
                header->setDatagramSize(size);
                header->setDatagramTag(*tag);
                header->setDatagramOffset(range.offset / 8);
                fragment->insertAtFront(header);
            }
            prependOuter(fragment);
            *fragment->addTag<LowpanTransmissionReq>() = *prepared;
            fragment->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
            if (!canPushPreparedPacket(fragment)) {
                delete fragment;
                dropPacket(packet, preparedRefusalReason());
                return;
            }
            if (!accepted) {
                emit(datagramAcceptedSignal, packet);
                accepted = true;
            }
            emit(fragmentSentSignal, fragment);
            if (!pushPreparedPacket(fragment)) { delete packet; return; }
        }
        handlePacketProcessed(packet);
        delete packet;
        return;
    }
    auto acceptedDatagram = std::unique_ptr<Packet>(packet->dup());
    removeRequestTags(packet);
    packet->removeTagIfPresent<Ieee802154AddressInd>();
    packet->removeTagIfPresent<InterfaceInd>();
    packet->addTag<LowpanTransmissionReq>()->operator=(*prepared);
    if (originalHeaderLength != 0)
        packet->removeAtFront(B(originalHeaderLength));
    packet->insertAtFront(encodedHeader);
    prependOuter(packet);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    if (!canPushPreparedPacket(packet)) {
        dropPacket(packet, preparedRefusalReason());
        return;
    }
    emit(datagramAcceptedSignal, acceptedDatagram.get());
    handlePacketProcessed(packet);
    pushPreparedPacket(packet);
}

void LowpanLayer::processLowerPacket(Packet *packet)
{
    auto indication = packet->findTag<Ieee802154AddressInd>();
    if (indication == nullptr || indication->getSrcAddress().isUnspecified() || indication->getSrcAddress().isBroadcast() ||
        indication->getSrcPanId() != link->getPanId() || indication->getDestPanId() != link->getPanId() ||
        (indication->getDestAddress() != link->getLinkAddress() && !indication->getDestAddress().isBroadcast()) ||
        packet->getDataLength() < B(1) || packet->getDataLength().get() % 8 != 0) {
        dropLowpanPacket(packet, LOWPAN_MALFORMED);
        return;
    }
    auto prefix = LowpanDispatchPrefix::parse(*packet);
    if (prefix.kind == LowpanDispatchPrefix::MESH && meshUnder) {
        Ptr<const Chunk> leading;
        if (prefix.pagingLength != 0) leading = packet->popAtFront(B(prefix.pagingLength));
        processMeshPacket(packet, prefix.state, leading);
        return;
    }
    processInnerPacket(packet, indication->getSrcAddress(), indication->getDestAddress());
}

simtime_t LowpanLayer::meshDeadline(simtime_t duration) const
{
    auto room = SIMTIME_MAX.raw() - simTime().raw();
    return duration.raw() >= room ? SIMTIME_MAX : SimTime::fromRaw(simTime().raw() + duration.raw());
}

simtime_t LowpanLayer::meshReservation(uint8_t hops, simtime_t retention) const
{
    // Reserve the sum of the same quantized durations enforced at each hop/cache.
    auto room = SIMTIME_MAX.raw() - simTime().raw();
    if (hops != 0 && meshResidence.raw() > room / hops) return SIMTIME_MAX;
    room -= hops * meshResidence.raw();
    if (retention.raw() >= room) return SIMTIME_MAX;
    return SimTime::fromRaw(SIMTIME_MAX.raw() - (room - retention.raw()));
}

void LowpanLayer::expireBroadcasts()
{
    for (auto it = broadcasts.begin(); it != broadcasts.end();)
        if (it->second.expires <= simTime()) it = broadcasts.erase(it); else ++it;
}

bool LowpanLayer::acceptBroadcast(Packet *packet, const Ieee802154Address& originator, uint8_t sequence,
        LowpanDispatchPrefix::State state)
{
    auto shape = inspectMeshPayload(*packet, state);
    ASSERT(shape);
    expireBroadcasts();
    auto key = std::make_pair(originator, sequence);
    auto found = broadcasts.find(key);
    if (found == broadcasts.end()) {
        if (broadcasts.size() >= size_t(maxBroadcastDatagrams)) {
            dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT); return false;
        }
        found = broadcasts.emplace(key, BroadcastRecord{meshDeadline(broadcastRetention), shape->fragmented, shape->size, shape->tag, {}}).first;
    }
    auto& record = found->second;
    if (record.fragmented != shape->fragmented || record.size != shape->size || record.tag != shape->tag) {
        dropLowpanPacket(packet, LOWPAN_MALFORMED); return false;
    }
    if (record.offsets.test(shape->offset)) {
        dropLowpanPacket(packet, LOWPAN_BROADCAST_DUPLICATE); return false;
    }
    record.offsets.set(shape->offset); // Commit before either local delivery or queue admission.
    return true;
}

void LowpanLayer::forwardMesh(Packet *packet, const Ptr<const Chunk>& header, const Ieee802154Address& destination,
        const Ptr<const Chunk>& leadingPages)
{
    Ieee802154AddressReq request;
    request.setSrcAddress(link->getLinkAddress());
    request.setDestAddress(destination);
    request.setSrcPanId(link->getPanId());
    request.setDestPanId(link->getPanId());
    auto prepared = link->prepareTransmission(request);
    auto envelope = makeShared<LowpanTransmissionReq>(*prepared);
    envelope->setDeliveryDeadline(std::min(prepared->getDeliveryDeadline(), meshDeadline(meshResidence)));
    removeRequestTags(packet);
    packet->removeTagIfPresent<Ieee802154AddressInd>();
    packet->removeTagIfPresent<InterfaceInd>();
    packet->insertAtFront(header);
    if (leadingPages) packet->insertAtFront(leadingPages);
    *packet->addTag<LowpanTransmissionReq>() = *envelope;
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    if (packet->getDataLength() > B(envelope->getPayloadLimit())) {
        dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT); return;
    }
    if (!canPushPreparedPacket(packet)) { dropPacket(packet, preparedRefusalReason()); return; }
    emit(meshForwardedSignal, packet);
    pushPreparedPacket(packet);
}

void LowpanLayer::processMeshPacket(Packet *packet, LowpanDispatchPrefix::State state,
        const Ptr<const Chunk>& leadingPages)
{
    auto first = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
    int length = 1 + ((first & 15) == 15 ? 1 : 0) + (first & 0x20 ? 2 : 8) + (first & 0x10 ? 2 : 8);
    if (packet->getDataLength() <= B(length)) { dropLowpanPacket(packet, LOWPAN_MALFORMED); return; }
    auto mesh = packet->popAtFront<LowpanMeshHeader>(B(length), Chunk::PF_ALLOW_INCORRECT);
    auto originator = mesh->getOriginator(), final = mesh->getFinalDestination();
    if (mesh->isIncorrect() || originator.getMode() != Ieee802154Address::EXTENDED ||
            (final.getMode() != Ieee802154Address::EXTENDED && !final.isBroadcast())) {
        dropLowpanPacket(packet, LOWPAN_MALFORMED); return;
    }
    using Prefix = LowpanDispatchPrefix;
    auto prefix = Prefix::parse(*packet, 0, state);
    bool broadcast = final.isBroadcast();
    if (broadcast != (prefix.kind == Prefix::BROADCAST) || prefix.kind == Prefix::MESH) {
        dropLowpanPacket(packet, LOWPAN_MALFORMED); return;
    }
    Ptr<const LowpanBroadcastHeader> bc0;
    Ptr<const Chunk> beforeBc0;
    if (broadcast) {
        if (originator == link->getLinkAddress()) { dropLowpanPacket(packet, LOWPAN_BROADCAST_DUPLICATE); return; }
        if (packet->getDataLength() <= B(prefix.pagingLength + 2)) { dropLowpanPacket(packet, LOWPAN_MALFORMED); return; }
        if (prefix.pagingLength != 0) beforeBc0 = packet->popAtFront(B(prefix.pagingLength));
        bc0 = packet->popAtFront<LowpanBroadcastHeader>(B(2));
        state = prefix.state;
    }
    if (!inspectMeshPayload(*packet, state)) { dropLowpanPacket(packet, LOWPAN_MALFORMED); return; }
    if (broadcast) {
        if (!acceptBroadcast(packet, originator, bc0->getSequenceNumber(), state)) return;
    }
    if (!broadcast && final == link->getLinkAddress()) {
        processInnerPacket(packet, originator, final, state);
        return;
    }
    if (mesh->getHopsLeft() <= 1) {
        if (broadcast) processInnerPacket(packet, originator, final, state);
        else dropLowpanPacket(packet, LOWPAN_MESH_HOP_EXHAUSTED);
        return;
    }
    Ieee802154Address destination("ffff");
    if (!broadcast) {
        auto route = meshRoutes.find(final);
        if (route == meshRoutes.end()) { dropLowpanPacket(packet, LOWPAN_MESH_NO_ROUTE); return; }
        destination = route->second;
    }
    auto forwarded = makeShared<LowpanMeshHeader>(*mesh);
    forwarded->setHopsLeft(mesh->getHopsLeft() - 1);
    auto transit = broadcast ? packet->dup() : packet;
    if (bc0) transit->insertAtFront(bc0);
    if (beforeBc0) transit->insertAtFront(beforeBc0);
    forwardMesh(transit, forwarded, destination, leadingPages);
    if (broadcast) {
        if (isNativeTransmissionOperational()) processInnerPacket(packet, originator, final, state);
        else dropPacket(packet, INTERFACE_DOWN);
    }
}

void LowpanLayer::processInnerPacket(Packet *packet, const Ieee802154Address& source, const Ieee802154Address& destination,
        LowpanDispatchPrefix::State state)
{
    using Prefix = LowpanDispatchPrefix;
    auto prefix = Prefix::parse(*packet, 0, state);
    if (prefix.pagingLength != 0) packet->popAtFront(B(prefix.pagingLength));
    if (prefix.kind == Prefix::MALFORMED || (meshUnder && prefix.kind == Prefix::BROADCAST)) {
        dropLowpanPacket(packet, LOWPAN_MALFORMED); return;
    }
    if (prefix.kind == Prefix::UNSUPPORTED_PAGE) { dropLowpanPacket(packet, LOWPAN_UNSUPPORTED_PAGE); return; }
    if (prefix.kind == Prefix::ESC) {
        auto type = packet->peekAtFront<LowpanExtendedDispatchHeader>(B(2))->getExtensionType();
        dropLowpanPacket(packet, type == 0 || type == 255 ? LOWPAN_MALFORMED : LOWPAN_UNSUPPORTED_EXTENSION); return;
    }
    if (prefix.kind == Prefix::RFRAG || prefix.kind == Prefix::RFRAG_ACK) {
        if (selectiveRecovery) selectiveRecovery->process(packet, prefix.kind == Prefix::RFRAG_ACK);
        else dropLowpanPacket(packet, LOWPAN_UNSUPPORTED_DISPATCH);
        return;
    }
    if (prefix.kind == Prefix::FRAG1 || prefix.kind == Prefix::FRAGN) {
        processFragment(packet, prefix.kind == Prefix::FRAG1, source, destination, prefix.state);
        return;
    }
    if (prefix.kind == Prefix::IPHC) {
        if (!decompressHeader(packet, -1, source, destination)) {
            dropLowpanPacket(packet, LOWPAN_MALFORMED);
            return;
        }
        deliverIpv6(packet);
        return;
    }
    if (prefix.kind != Prefix::IPV6) {
        dropLowpanPacket(packet, LOWPAN_UNSUPPORTED_DISPATCH);
        return;
    }
    packet->popAtFront<LowpanHeader>(B(1));
    deliverIpv6(packet);
}

void LowpanLayer::processFragment(Packet *packet, bool first, const Ieee802154Address& source, const Ieee802154Address& destination,
        LowpanDispatchPrefix::State state)
{
    emit(fragmentReceivedSignal, packet);
    if (fragmentForwarding && fragmentForwarding->process(packet, first, source, destination, state)) return;
    auto expired = reassembly->expire(simTime());
    if (expired != 0)
        emit(reassemblyExpiredSignal, (long)expired);
    if (packet->getDataLength() <= B(5)) {
        scheduleExpiry();
        dropLowpanPacket(packet, LOWPAN_MALFORMED);
        return;
    }
    uint16_t size, tag;
    int offset = 0;
    bool valid;
    if (first) {
        auto header = packet->popAtFront<LowpanFrag1Header>(B(4), Chunk::PF_ALLOW_INCORRECT);
        size = header->getDatagramSize();
        tag = header->getDatagramTag();
        valid = !header->isIncorrect();
        using Prefix = LowpanDispatchPrefix;
        auto prefix = Prefix::parse(*packet, 0, state);
        if (prefix.pagingLength != 0) packet->popAtFront(B(prefix.pagingLength));
        if (prefix.kind == Prefix::IPV6)
            packet->popAtFront<LowpanHeader>(B(1));
        else if (prefix.kind == Prefix::IPHC)
            valid = valid && decompressHeader(packet, size, source, destination);
        else
            valid = false;
    }
    else {
        auto header = packet->popAtFront<LowpanFragnHeader>(B(5), Chunk::PF_ALLOW_INCORRECT);
        size = header->getDatagramSize();
        tag = header->getDatagramTag();
        offset = 8 * header->getDatagramOffset();
        valid = !header->isIncorrect();
    }
    if (!valid) {
        scheduleExpiry();
        dropLowpanPacket(packet, LOWPAN_MALFORMED);
        return;
    }
    auto indication = packet->getTag<Ieee802154AddressInd>();
    LowpanReassemblyTable::Key key{networkInterface->getId(), indication->getSrcPanId(), indication->getDestPanId(),
        source, destination, size, tag};
    auto result = reassembly->accept(key, offset, first, *packet, simTime());
    if (fragmentForwarding) fragmentForwarding->handleReassemblyChanged(key);
    scheduleExpiry();
    if (result.packet != nullptr) {
        *result.packet->addTag<Ieee802154AddressInd>() = *indication;
        delete packet;
        auto completed = result.packet.release();
        if (!isValidIpv6Packet(completed)) {
            dropLowpanPacket(completed, LOWPAN_MALFORMED);
            return;
        }
        emit(reassemblyCompletedSignal, completed);
        deliverIpv6(completed);
    }
    else if (result.status == LowpanReassemblyTable::RESOURCE_LIMIT)
        dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT);
    else if (result.status == LowpanReassemblyTable::INVALID || result.status == LowpanReassemblyTable::CONFLICT ||
             result.status == LowpanReassemblyTable::QUARANTINED)
        dropLowpanPacket(packet, LOWPAN_REASSEMBLY_REJECTED);
    else
        delete packet;
}

bool LowpanLayer::decompressHeader(Packet *packet, int originalSize, const Ieee802154Address& source, const Ieee802154Address& destination)
{
    auto result = LowpanNhcCodec::decode(*packet, originalSize, source, destination, &compressionContexts, simTime());
    if (!result.header)
        return false;
    // Normalize byte-only FRAG1 input before inserting reconstructed headers.
    packet->popAtFront(B(result.encodedLength));
    packet->trimFront();
    packet->insertAtFront(result.header);
    return true;
}

void LowpanLayer::deliverIpv6(Packet *packet)
{
    if (!isValidIpv6Packet(packet)) {
        dropLowpanPacket(packet, LOWPAN_MALFORMED);
        return;
    }
    removeRequestTags(packet);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv6);
    packet->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv6);
    packet->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
    emit(datagramCompletedSignal, packet);
    send(packet, "upperLayerOut");
}

} } // namespace inet::lowpan
