// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanNativeMac.h"
#include "inet/linklayer/lowpan/LowpanRadioProfile.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameHeader_m.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameFormat.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/checksum/Checksum.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154NarrowbandTransmitter.h"
namespace inet { namespace lowpan {
using physicallayer::IRadio;
Define_Module(LowpanNativeMac);

LowpanNativeMac::~LowpanNativeMac()
{
    cancelAndDelete(accessTimer);
    cancelAndDelete(ackTimer);
    delete pendingAck;
}

void LowpanNativeMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        domain = getModuleFromPar<LowpanNativeLinkDomain>(par("linkDomainModule"), this);
        radio.reference(this, "radioModule", true);
        txQueue = getQueue(gate(upperLayerInGateId));
        nextSequence = intrand(256);
        maxLifetime = par("maxLowerDeliveryLifetime");
        maxBackoffs = par("macMaxCSMABackoffs");
        maxRetries = par("macMaxFrameRetries");
        minExponent = par("macMinBE");
        maxExponent = par("macMaxBE");
        if (maxBackoffs < 0 || maxBackoffs > 5 || maxRetries < 0 || maxRetries > 7 || minExponent < 0 || minExponent > maxExponent || maxExponent > 8)
            throw cRuntimeError("Invalid native IEEE 802.15.4 retry/backoff parameters");
        accessTimer = new cMessage("nativeAccess");
        ackTimer = new cMessage("nativeAckWait");
        WATCH(state);
        WATCH(backoffs);
        WATCH(retries);
    }
    else if (stage == INITSTAGE_LAST) {
        auto tail = LowpanRadioProfile::transmissionTail(radio.get(), SimTime(192, SIMTIME_US), domain->getMaximumPropagationDelay(networkInterface));
        if (maxLifetime < tail)
            throw cRuntimeError("Native LoWPAN lower lifetime cannot fit a maximum frame");
        auto transmitter = check_and_cast<const physicallayer::Ieee802154NarrowbandTransmitter *>(radio->getTransmitter());
        if (transmitter->getBitrate() != bps(250000) || transmitter->getHeaderLength() != B(2) ||
            simtime_t(transmitter->par("preambleDuration")) != SimTime(128, SIMTIME_US))
            throw cRuntimeError("Native LoWPAN profile requires 250kbps, sixteen-bit PHR and128us preamble");
        if (2 * domain->getMaximumPropagationDelay(networkInterface) >= SimTime(16, SIMTIME_US))
            throw cRuntimeError("Native LoWPAN profile requires propagation within the immediate ACK wait bound");
        auto radioModule = check_and_cast<cModule *>(radio.get());
        cStringTokenizer switching(radioModule->par("switchingTimes"));
        switching.nextToken(); // Unit prefix; this profile accounts for turnaround in the MAC.
        while (switching.hasMoreTokens())
            if (std::stod(switching.nextToken()) != 0)
                throw cRuntimeError("Native LoWPAN profile requires zero radio switchingTimes");
        for (auto output : {gate("lowerLayerOut"), radioModule->gate("upperLayerOut")})
            for (auto path = output; path != nullptr; path = path->getNextGate())
                if (path->getChannel() != nullptr)
                    throw cRuntimeError("Native LoWPAN requires undelayed MAC/radio connections");
    }
}

void LowpanNativeMac::configureNetworkInterface()
{
    auto address = domain->getPeer(networkInterface).address;
    uint64_t iid = address.getInt() ^ UINT64_C(0x0200000000000000);
    networkInterface->setInterfaceToken(InterfaceToken(uint32_t(iid), uint32_t(iid >> 32), 64));
    networkInterface->setMtu(1280);
    networkInterface->setDatarate(250000);
    networkInterface->setBroadcast(true);
    networkInterface->setMulticast(true);
    networkInterface->setProtocol(&lowpanProtocol);
}

bool LowpanNativeMac::isRequestValid(const Ieee802154AddressReq& request) const
{
    return request.getSrcAddress() == domain->getPeer(networkInterface).address && request.getSrcPanId() == domain->getPanId() &&
        request.getDestPanId() == domain->getPanId() && (request.getDestAddress().isBroadcast() || domain->contains(request.getDestAddress()));
}

Ptr<const LowpanTransmissionReq> LowpanNativeMac::prepareTransmission(const Ieee802154AddressReq& request) const
{
    if (!isRequestValid(request)) throw cRuntimeError("Invalid native LoWPAN request tuple");
    auto prepared = makeShared<LowpanTransmissionReq>();
    prepared->setSrcAddress(request.getSrcAddress());
    prepared->setDestAddress(request.getDestAddress());
    prepared->setSrcPanId(request.getSrcPanId());
    prepared->setDestPanId(request.getDestPanId());
    prepared->setProfile(2);
    prepared->setPayloadLimit(Ieee802154FrameFormat::getPayloadLimit(request.getDestAddress().isBroadcast()));
    prepared->setDeliveryDeadline(simTime() + maxLifetime);
    return prepared;
}

bool LowpanNativeMac::isPreparedValid(const Packet *packet, bool encapsulated) const
{
    auto request = packet->findTag<LowpanTransmissionReq>();
    auto protocol = packet->findTag<PacketProtocolTag>();
    if (!request || !isRequestValid(*request) || request->getProfile() != 2 ||
        request->getPayloadLimit() != Ieee802154FrameFormat::getPayloadLimit(request->getDestAddress().isBroadcast()) ||
        request->getDeliveryDeadline() < SIMTIME_ZERO || packet->findTag<SignalBitrateReq>() ||
        !protocol || protocol->getProtocol() != &lowpanProtocol || packet->getDataLength().get() % 8 != 0)
        return false;
    if (!encapsulated) return packet->getDataLength() <= B(request->getPayloadLimit());
    auto header = packet->peekAtFront<Ieee802154FrameHeader>();
    return header->getSourceAddress() == request->getSrcAddress() && header->getDestinationAddress() == request->getDestAddress() &&
        header->getPanId() == request->getDestPanId() && packet->getDataLength() <= B(Ieee802154FrameFormat::MAX_FRAME_BYTES);
}

bool LowpanNativeMac::canMeetDeadline() const
{
    return simTime() + LowpanRadioProfile::transmissionTail(radio.get(), SimTime(192, SIMTIME_US), domain->getMaximumPropagationDelay(networkInterface)) <=
        currentTxFrame->getTag<LowpanTransmissionReq>()->getDeliveryDeadline();
}

void LowpanNativeMac::appendFcs(Packet *packet) const
{
    uint16_t checksum = crc16_ccitt(packet->peekDataAsBytes()->getBytes());
    packet->insertAtBack(makeShared<BytesChunk>(std::vector<uint8_t>{uint8_t(checksum), uint8_t(checksum >> 8)}));
}

void LowpanNativeMac::dropEnvelope(bool expired)
{
    LowpanPacketDropDetails details;
    details.setReason(OTHER_PACKET_DROP);
    details.setLowpanReason(expired ? LOWPAN_LOWER_LIFETIME_EXPIRED : LOWPAN_ENVELOPE_INVALIDATED);
    dropCurrentTxFrame(details);
    state = IDLE;
    radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
    startNextFrame();
}

void LowpanNativeMac::startNextFrame()
{
    if (isDown() || state != IDLE) return;
    if (!currentTxFrame && canDequeuePacket()) {
        // Set state before the pull, which may notify the active sink synchronously.
        state = BACKOFF;
        currentTxFrame = dequeuePacket();
        if (!isPreparedValid(currentTxFrame, false)) { dropEnvelope(false); return; }
        auto request = currentTxFrame->getTag<LowpanTransmissionReq>();
        auto header = makeShared<Ieee802154FrameHeader>();
        bool broadcast = request->getDestAddress().isBroadcast();
        header->setFrameControl(broadcast ? 0xd841 : 0xdc61);
        header->setChunkLength(B(Ieee802154FrameFormat::getHeaderLength(header->getFrameControl())));
        header->setSequenceNumber(nextSequence++);
        header->setPanId(request->getDestPanId());
        header->setSourceAddress(request->getSrcAddress());
        header->setDestinationAddress(request->getDestAddress());
        currentTxFrame->insertAtFront(header);
        appendFcs(currentTxFrame);
        retries = backoffs = 0;
    }
    if (currentTxFrame) startBackoff();
}

void LowpanNativeMac::startBackoff()
{
    state = BACKOFF;
    int exponent = std::min(minExponent + backoffs, maxExponent);
    radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
    scheduleAt(std::max(simTime(), nextAccessTime) + SimTime(intrand(1 << exponent) * 320, SIMTIME_US), accessTimer);
}

void LowpanNativeMac::finishData(bool successful, PacketDropReason reason)
{
    if (successful) deleteCurrentTxFrame();
    else {
        PacketDropDetails details;
        details.setReason(reason);
        dropCurrentTxFrame(details);
    }
    state = IDLE;
    nextAccessTime = simTime() + SimTime(640, SIMTIME_US);
    radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
    startNextFrame();
}

void LowpanNativeMac::handleSelfMessage(cMessage *message)
{
    if (message == ackTimer) {
        ASSERT(state == ACK_WAIT);
        if (++retries > maxRetries) finishData(false, RETRY_LIMIT_REACHED);
        else { backoffs = 0; startBackoff(); }
        return;
    }
    ASSERT(message == accessTimer);
    if (state == BACKOFF) {
        state = CCA;
        ccaBusy = radio->getReceptionState() != IRadio::RECEPTION_STATE_IDLE;
        scheduleAfter(SimTime(128, SIMTIME_US), accessTimer);
    }
    else if (state == CCA) {
        if (ccaBusy || radio->getReceptionState() != IRadio::RECEPTION_STATE_IDLE) {
            if (++backoffs > maxBackoffs) finishData(false, CONGESTION);
            else startBackoff();
        }
        else if (!isPreparedValid(currentTxFrame, true)) dropEnvelope(false);
        else if (!canMeetDeadline()) dropEnvelope(true);
        else {
            state = DATA_TURNAROUND;
            radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
            scheduleAfter(SimTime(192, SIMTIME_US), accessTimer);
        }
    }
    else if (state == DATA_TURNAROUND) {
        if (!isPreparedValid(currentTxFrame, true)) dropEnvelope(false);
        else if (!canMeetDeadline()) dropEnvelope(true);
        else {
            state = DATA_TRANSMIT;
            auto wire = currentTxFrame->dup();
            wire->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&lowpanNativeProtocol);
            sendDown(wire);
        }
    }
    else if (state == ACK_TURNAROUND) {
        state = ACK_TRANSMIT;
        auto packet = pendingAck;
        pendingAck = nullptr;
        sendDown(packet);
    }
    else throw cRuntimeError("Unexpected native MAC access timer state");
}

void LowpanNativeMac::dropReceived(Packet *packet, PacketDropReason reason)
{
    PacketDropDetails details;
    details.setReason(reason);
    emit(packetDroppedSignal, packet, &details);
    delete packet;
}

void LowpanNativeMac::handleLowerPacket(Packet *packet)
{
    if (packet->hasBitError() || packet->getDataLength() < B(5) || packet->getDataLength() > B(Ieee802154FrameFormat::MAX_FRAME_BYTES) || packet->getDataLength().get() % 8 != 0) {
        dropReceived(packet, INCORRECTLY_RECEIVED); return;
    }
    auto bytes = packet->peekDataAsBytes()->getBytes();
    if (crc16_ccitt(bytes.data(), bytes.size() - 2) != (unsigned(bytes[bytes.size() - 2]) | unsigned(bytes.back()) << 8)) {
        dropReceived(packet, INCORRECTLY_RECEIVED); return;
    }
    Packet wire("native-wire", makeShared<BytesChunk>(bytes));
    auto header = wire.peekAtFront<Ieee802154FrameHeader>(b(-1), Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT);
    if (header->isIncomplete() || header->isIncorrect() || header->getChunkLength() + B(2) > packet->getDataLength()) {
        dropReceived(packet, INCORRECTLY_RECEIVED); return;
    }
    if (header->getFrameControl() == 2) {
        if (packet->getDataLength() == B(5) && state == ACK_WAIT &&
            currentTxFrame->peekAtFront<Ieee802154FrameHeader>()->getSequenceNumber() == header->getSequenceNumber()) {
            cancelEvent(ackTimer);
            finishData(true);
        }
        delete packet;
        return;
    }
    if (header->getPanId() != domain->getPanId() || !domain->contains(header->getSourceAddress()) ||
        (!header->getDestinationAddress().isBroadcast() && header->getDestinationAddress() != domain->getPeer(networkInterface).address)) {
        dropReceived(packet, NOT_ADDRESSED_TO_US); return;
    }
    if (state != IDLE && state != BACKOFF && state != CCA) {
        dropReceived(packet, OTHER_PACKET_DROP); return;
    }
    if (header->getFrameControl() & 0x20) {
        cancelEvent(accessTimer);
        auto ackHeader = makeShared<Ieee802154FrameHeader>();
        ackHeader->setFrameControl(2);
        ackHeader->setChunkLength(B(Ieee802154FrameFormat::ACK_HEADER_BYTES));
        ackHeader->setSequenceNumber(header->getSequenceNumber());
        pendingAck = new Packet("native-ack", ackHeader);
        pendingAck->addTag<PacketProtocolTag>()->setProtocol(&lowpanNativeProtocol);
        appendFcs(pendingAck);
        state = ACK_TURNAROUND;
        radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
        scheduleAfter(SimTime(192, SIMTIME_US), accessTimer);
    }
    // IEEE 802.15.4-2024 section 6.6.1: the device-wide eight-bit DSN ties an
    // immediate ACK to a transmission; it cannot identify duplicates across gaps.
    if (!(header->getFrameControl() & 0x20)) {
        nextAccessTime = simTime() + SimTime(640, SIMTIME_US);
        if (state == BACKOFF || state == CCA) {
            cancelEvent(accessTimer);
            startBackoff();
        }
    }
    packet->removeAtFront(header->getChunkLength());
    packet->removeAtBack(B(2));
    packet->clearTags();
    auto indication = packet->addTag<Ieee802154AddressInd>();
    indication->setSrcAddress(header->getSourceAddress());
    indication->setDestAddress(header->getDestinationAddress());
    indication->setSrcPanId(header->getPanId());
    indication->setDestPanId(header->getPanId());
    packet->addTag<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
    packet->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    sendUp(packet);
}

void LowpanNativeMac::transmissionFinished()
{
    if (state == DATA_TRANSMIT) {
        nextAccessTime = simTime() + SimTime(640, SIMTIME_US);
        if (currentTxFrame->peekAtFront<Ieee802154FrameHeader>()->getFrameControl() & 0x20) {
            state = ACK_WAIT;
            radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
            scheduleAfter(SimTime(560, SIMTIME_US), ackTimer);
        }
        else finishData(true);
    }
    else if (state == ACK_TRANSMIT) {
        state = IDLE;
        nextAccessTime = simTime() + SimTime(640, SIMTIME_US);
        radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
        startNextFrame();
    }
}

void LowpanNativeMac::receiveSignal(cComponent *, simsignal_t signal, intval_t value, cObject *)
{
    Enter_Method("receiveSignal");
    if (signal == IRadio::receptionStateChangedSignal) {
        if (state == CCA && value != IRadio::RECEPTION_STATE_IDLE) ccaBusy = true;
        return;
    }
    if (signal != IRadio::transmissionStateChangedSignal) return;
    auto previous = previousTransmission;
    previousTransmission = static_cast<IRadio::TransmissionState>(value);
    if (previous == IRadio::TRANSMISSION_STATE_TRANSMITTING && value == IRadio::TRANSMISSION_STATE_IDLE)
        transmissionFinished();
}

void LowpanNativeMac::clearTransientState()
{
    cancelEvent(accessTimer);
    cancelEvent(ackTimer);
    delete pendingAck;
    pendingAck = nullptr;
    state = IDLE;
    previousTransmission = IRadio::TRANSMISSION_STATE_UNDEFINED;
    nextAccessTime = simTime();
}
void LowpanNativeMac::handleStartOperation(LifecycleOperation *operation)
{
    MacProtocolBase::handleStartOperation(operation);
    check_and_cast<cModule *>(radio.get())->subscribe(IRadio::transmissionStateChangedSignal, this);
    check_and_cast<cModule *>(radio.get())->subscribe(IRadio::receptionStateChangedSignal, this);
    radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
}
void LowpanNativeMac::handleStopOperation(LifecycleOperation *operation)
{
    clearTransientState();
    check_and_cast<cModule *>(radio.get())->unsubscribe(IRadio::transmissionStateChangedSignal, this);
    check_and_cast<cModule *>(radio.get())->unsubscribe(IRadio::receptionStateChangedSignal, this);
    MacProtocolBase::handleStopOperation(operation);
    radio->setRadioMode(IRadio::RADIO_MODE_OFF);
}
void LowpanNativeMac::handleCrashOperation(LifecycleOperation *operation)
{
    clearTransientState();
    check_and_cast<cModule *>(radio.get())->unsubscribe(IRadio::transmissionStateChangedSignal, this);
    check_and_cast<cModule *>(radio.get())->unsubscribe(IRadio::receptionStateChangedSignal, this);
    MacProtocolBase::handleCrashOperation(operation);
    radio->setRadioMode(IRadio::RADIO_MODE_OFF);
}
void LowpanNativeMac::handleUpperPacket(Packet *) { throw cRuntimeError("Native LoWPAN MAC requires its bounded pull queue"); }
queueing::IPassivePacketSource *LowpanNativeMac::getProvider(const cGate *gate) { return gate->getId() == upperLayerInGateId ? txQueue.get() : nullptr; }
void LowpanNativeMac::handleCanPullPacketChanged(const cGate *) { Enter_Method("handleCanPullPacketChanged"); startNextFrame(); }
void LowpanNativeMac::handlePullPacketProcessed(Packet *, const cGate *, bool) { throw cRuntimeError("Native MAC does not support asynchronous pulls"); }
} }
