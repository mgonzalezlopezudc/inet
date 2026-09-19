// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanLayer.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFragmenter.h"
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"
#include "inet/linklayer/lowpan/LowpanUdpNhcCodec.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/NextHopAddressTag_m.h"
#include "inet/networklayer/ipv6/Ipv6Header_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

namespace inet { namespace lowpan {

Define_Module(LowpanLayer);
simsignal_t LowpanLayer::datagramAcceptedSignal = registerSignal("datagramAccepted");
simsignal_t LowpanLayer::datagramCompletedSignal = registerSignal("datagramCompleted");
simsignal_t LowpanLayer::fragmentSentSignal = registerSignal("fragmentSent");
simsignal_t LowpanLayer::fragmentReceivedSignal = registerSignal("fragmentReceived");
simsignal_t LowpanLayer::reassemblyCompletedSignal = registerSignal("reassemblyCompleted");
simsignal_t LowpanLayer::reassemblyExpiredSignal = registerSignal("reassemblyExpired");

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
        reassembly = std::make_unique<LowpanReassemblyTable>(par("maxReassemblyDatagrams"), par("maxReassemblyBytes"), reassemblyTimeout);
        expiryTimer = new cMessage("lowpanReassemblyExpiry");
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        if (networkInterface->getMtu() != 1280 || networkInterface->getProtocol() != &lowpanProtocol)
            throw cRuntimeError("LoWPAN requires the native link contract and IPv6 MTU 1280");
    }
    else if (stage == INITSTAGE_QUEUEING) {
        checkPacketOperationSupport(gate("lowerLayerOut"));
        for (auto gate = this->gate("lowerLayerIn")->getPathStartGate(); gate != nullptr; gate = gate->getNextGate())
            if (gate->getChannel() != nullptr)
                throw cRuntimeError("LoWPAN deadline profile requires an undelayed receive boundary");
    }
}

void LowpanLayer::clearTransientState()
{
    cancelEvent(expiryTimer);
    reassembly->clear();
    // Tag guards deliberately outlive interface teardown.
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

void LowpanLayer::handleMessageWhenDown(cMessage *message)
{
    dropPacket(check_and_cast<Packet *>(message), INTERFACE_DOWN);
}

void LowpanLayer::handleMessageWhenUp(cMessage *message)
{
    if (message == expiryTimer) {
        auto expired = reassembly->expire(simTime());
        if (expired != 0)
            emit(reassemblyExpiredSignal, (long)expired);
        scheduleExpiry();
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
    if (packet->peekAtFront<Ipv6Header>()->getDestAddress().isMulticast())
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
    request.setSrcAddress(link->getLinkAddress());
    request.setDestAddress(destination);
    request.setSrcPanId(link->getPanId());
    request.setDestPanId(link->getPanId());
    auto prepared = link->prepareTransmission(request);
    int size = packet->getDataLength().get<B>();
    Ptr<const Chunk> encodedHeader = makeShared<LowpanHeader>();
    int originalHeaderLength = 0;
    if (useIphc) {
        auto iphc = LowpanIphcCodec::encode(*packet->peekAtFront<Ipv6Header>(), request.getSrcAddress(), request.getDestAddress());
        if (iphc != nullptr) {
            int length = iphc->getChunkLength().get<B>();
            // RFC 6282 section 4: compressed headers must fit wholly in FRAG1.
            // Uncompressed dispatch remains valid when its IPv6 bytes must span frames.
            if (size - 40 + length <= prepared->getPayloadLimit() || length + 4 <= prepared->getPayloadLimit()) {
                encodedHeader = iphc;
                originalHeaderLength = 40;
            }
        }
    }
    if (useIphc && originalHeaderLength == 40 && size >= 48 &&
        packet->peekAtFront<Ipv6Header>()->getProtocolId() == IP_PROT_UDP) {
        try {
            auto nhc = LowpanUdpNhcCodec::encode(packet->peekDataAt<BytesChunk>(B(40), B(8))->getBytes(), size - 40);
            if (nhc != nullptr) {
                auto iphc = LowpanIphcCodec::encode(*packet->peekAtFront<Ipv6Header>(), request.getSrcAddress(), request.getDestAddress(), true);
                int length = (iphc->getChunkLength() + nhc->getChunkLength()).get<B>();
                if (size - 48 + length <= prepared->getPayloadLimit() || length + 4 <= prepared->getPayloadLimit()) {
                    auto chain = makeShared<SequenceChunk>();
                    chain->insertAtBack(iphc);
                    chain->insertAtBack(nhc);
                    encodedHeader = chain;
                    originalHeaderLength = 48;
                }
            }
        }
        catch (const cRuntimeError& error) {
            // E.g. a declared rather than computed transport checksum stays inline.
            EV_DEBUG << "UDP NHC fallback: " << error.what() << endl;
        }
    }
    int encodedLength = encodedHeader->getChunkLength().get<B>();
    auto ranges = LowpanFragmenter::plan(size, prepared->getPayloadLimit(), prepared->getPayloadLimit(), encodedLength, originalHeaderLength);
    if (ranges.empty()) {
        dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT);
        return;
    }
    if (ranges.size() > 1) {
        // RFC 4944 receiver lifetime is at most 60 seconds. Use that maximum
        // even when this interface's own receive timeout is configured shorter.
        auto tag = tagAllocator.allocate(simTime(), prepared->getDeliveryDeadline() + SimTime(60));
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
            *fragment->addTag<LowpanTransmissionReq>() = *prepared;
            fragment->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
            if (!consumer.canPushPacket(fragment)) {
                delete fragment;
                dropPacket(packet, QUEUE_OVERFLOW);
                return;
            }
            if (!accepted) {
                emit(datagramAcceptedSignal, packet);
                accepted = true;
            }
            emit(fragmentSentSignal, fragment);
            consumer.pushPacket(fragment);
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
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    if (!consumer.canPushPacket(packet)) {
        dropPacket(packet, QUEUE_OVERFLOW);
        return;
    }
    emit(datagramAcceptedSignal, acceptedDatagram.get());
    handlePacketProcessed(packet);
    consumer.pushPacket(packet);
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
    auto dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
    if ((dispatch & 0xf8) == 0xc0 || (dispatch & 0xf8) == 0xe0) {
        processFragment(packet, (dispatch & 0xf8) == 0xc0);
        return;
    }
    if ((dispatch & 0xe0) == 0x60) {
        if (!decompressHeader(packet, -1)) {
            dropLowpanPacket(packet, LOWPAN_MALFORMED);
            return;
        }
        deliverIpv6(packet);
        return;
    }
    if (dispatch != 0x41) {
        dropLowpanPacket(packet, LOWPAN_UNSUPPORTED_DISPATCH);
        return;
    }
    packet->popAtFront<LowpanHeader>(B(1));
    deliverIpv6(packet);
}

void LowpanLayer::processFragment(Packet *packet, bool first)
{
    emit(fragmentReceivedSignal, packet);
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
        auto dispatch = packet->peekAtFront<BytesChunk>(B(1))->getBytes()[0];
        if (dispatch == 0x41)
            packet->popAtFront<LowpanHeader>(B(1));
        else if ((dispatch & 0xe0) == 0x60)
            valid = valid && decompressHeader(packet, size);
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
        indication->getSrcAddress(), indication->getDestAddress(), size, tag};
    auto result = reassembly->accept(key, offset, first, *packet, simTime());
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

bool LowpanLayer::decompressHeader(Packet *packet, int originalSize)
{
    try {
        auto header = packet->peekAtFront<LowpanIphcHeader>(b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
        if (header->isIncomplete() || header->isIncorrect() || header->getChunkLength() > packet->getDataLength())
            return false;
        auto encodedLength = header->getChunkLength();
        Ptr<const LowpanUdpNhcHeader> nhc;
        if (header->getNh()) {
            nhc = packet->peekDataAt<LowpanUdpNhcHeader>(encodedLength, b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
            if (nhc->isIncomplete() || nhc->isIncorrect() || nhc->getChecksumElided())
                return false;
            encodedLength += nhc->getChunkLength();
            if (encodedLength > packet->getDataLength())
                return false;
        }
        if (originalSize < 0)
            originalSize = (nhc ? 48 : 40) + (packet->getDataLength() - encodedLength).get<B>();
        auto indication = packet->getTag<Ieee802154AddressInd>();
        auto ipv6 = LowpanIphcCodec::decode(*header, originalSize, indication->getSrcAddress(), indication->getDestAddress(), nhc != nullptr);
        if (ipv6 == nullptr)
            return false;
        auto udp = nhc ? LowpanUdpNhcCodec::decode(*nhc, originalSize - 40) : nullptr;
        if (nhc && !udp)
            return false;
        // Normalize byte-only FRAG1 input before inserting reconstructed headers.
        // The data part may start after an already popped fragmentation header.
        packet->popAtFront(encodedLength);
        packet->trimFront();
        if (udp)
            packet->insertAtFront(udp);
        packet->insertAtFront(ipv6);
        return true;
    }
    catch (const cRuntimeError&) {
        return false;
    }
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
