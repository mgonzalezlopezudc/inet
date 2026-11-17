// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanCompatibilityMac.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/ieee802154/Ieee802154MacHeader_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanRadioProfile.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatRadioBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154NarrowbandTransmitter.h"

namespace inet { namespace lowpan {

Define_Module(LowpanCompatibilityMac);

void LowpanCompatibilityMac::handleStartOperation(LifecycleOperation *operation)
{
    Ieee802154Mac::handleStartOperation(operation);
    radio->setRadioMode(physicallayer::IRadio::RADIO_MODE_RECEIVER);
}

void LowpanCompatibilityMac::initialize(int stage)
{
    Ieee802154Mac::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        domain = getModuleFromPar<LowpanLinkDomain>(par("linkDomainModule"), this);
        maxLowerDeliveryLifetime = par("maxLowerDeliveryLifetime");
        if (headerLength != HEADER_BYTES * 8 || upperLayerProtocol != &lowpanProtocol)
            throw cRuntimeError("Invalid static LoWPAN compatibility MAC profile");
    }
    else if (stage == INITSTAGE_LAST) {
        if (maxLowerDeliveryLifetime < getTransmissionTail())
            throw cRuntimeError("LoWPAN lower lifetime is shorter than the configured transmission tail");
        auto radioModule = check_and_cast<cModule *>(radio.get());
        for (auto output : {gate("lowerLayerOut"), radioModule->gate("upperLayerOut")}) {
            auto expected = output == gate("lowerLayerOut") ? radioModule->gate("upperLayerIn") : gate("lowerLayerIn");
            if (output->getPathEndGate() != expected)
                throw cRuntimeError("LoWPAN deadline profile requires direct MAC/radio connections");
            for (auto gate = output; gate != nullptr; gate = gate->getNextGate())
                if (gate->getChannel() != nullptr)
                    throw cRuntimeError("LoWPAN deadline profile does not allow MAC/radio delay channels");
        }
    }
}

simtime_t LowpanCompatibilityMac::getTransmissionTail() const
{
    return LowpanRadioProfile::transmissionTail(radio.get(), aTurnaroundTime, domain->getMaximumPropagationDelay(networkInterface));
}

bool LowpanCompatibilityMac::canMeetDeliveryDeadline(const Packet *packet) const
{
    auto request = packet->findTag<LowpanTransmissionReq>();
    return request != nullptr && simTime() + getTransmissionTail() <= request->getDeliveryDeadline();
}

void LowpanCompatibilityMac::configureNetworkInterface()
{
    Ieee802154Mac::configureNetworkInterface();
    const auto& address = domain->getPeer(networkInterface).address;
    if (address.getMode() != Ieee802154Address::EXTENDED)
        throw cRuntimeError("Static LoWPAN interfaces require an extended native address");
    // RFC 4944 section 6: derive the IPv6 IID from native EUI-64, never its alias.
    uint64_t iid = address.getInt() ^ UINT64_C(0x0200000000000000);
    networkInterface->setInterfaceToken(InterfaceToken(uint32_t(iid), uint32_t(iid >> 32), 64));
    networkInterface->setProtocol(&lowpanProtocol);
}

Ptr<const LowpanTransmissionReq> LowpanCompatibilityMac::prepareTransmission(const Ieee802154AddressReq& request) const
{
    if (!isNativeRequestValid(request))
        throw cRuntimeError("LoWPAN transmission request differs from immutable compatibility domain");
    auto result = makeShared<LowpanTransmissionReq>();
    result->setSrcAddress(request.getSrcAddress());
    result->setDestAddress(request.getDestAddress());
    result->setSrcPanId(request.getSrcPanId());
    result->setDestPanId(request.getDestPanId());
    result->setPayloadLimit(PAYLOAD_BYTES);
    result->setProfile(PROFILE_ID);
    result->setDeliveryDeadline(simTime() + maxLowerDeliveryLifetime);
    return result;
}

void LowpanCompatibilityMac::encapsulate(Packet *packet)
{
    if (!isPreparedFrameValid(packet, false))
        throw cRuntimeError("Invalid or oversized LoWPAN prepared compatibility frame");
    const auto& request = packet->getTag<LowpanTransmissionReq>();
    auto legacyRequest = packet->addTagIfAbsent<MacAddressReq>();
    legacyRequest->setSrcAddress(domain->getPeer(networkInterface).alias);
    legacyRequest->setDestAddress(resolveAlias(request->getDestAddress()));
    Ieee802154Mac::encapsulate(packet);
}

bool LowpanCompatibilityMac::isNativeRequestValid(const Ieee802154AddressReq& request) const
{
    const auto& peer = domain->getPeer(networkInterface);
    return request.getSrcAddress() == peer.address && networkInterface->getMacAddress() == peer.alias &&
        request.getSrcPanId() == domain->getPanId() && request.getDestPanId() == domain->getPanId() &&
        (request.getDestAddress().isBroadcast() || domain->getAddressMap().findAlias(request.getDestAddress()));
}

MacAddress LowpanCompatibilityMac::resolveAlias(const Ieee802154Address& address) const
{
    return address.isBroadcast() ? MacAddress::BROADCAST_ADDRESS : *domain->getAddressMap().findAlias(address);
}

bool LowpanCompatibilityMac::isPreparedFrameValid(const Packet *packet, bool encapsulated) const
{
    auto request = packet->findTag<LowpanTransmissionReq>();
    auto protocol = packet->findTag<PacketProtocolTag>();
    if (request == nullptr || !isNativeRequestValid(*request) || request->getProfile() != PROFILE_ID ||
        request->getPayloadLimit() != PAYLOAD_BYTES || protocol == nullptr ||
        request->getDeliveryDeadline() < SIMTIME_ZERO || packet->findTag<SignalBitrateReq>() != nullptr ||
        protocol->getProtocol() != (encapsulated ? &Protocol::ieee802154 : &lowpanProtocol))
        return false;
    auto payloadLength = packet->getDataLength() - (encapsulated ? B(HEADER_BYTES) : B(0));
    if (payloadLength < b(0) || payloadLength > B(PAYLOAD_BYTES) || payloadLength.get() % 8 != 0)
        return false;
    if (encapsulated) {
        try {
            auto header = packet->peekAtFront<Ieee802154MacHeader>();
            return header->getChunkLength() == B(HEADER_BYTES) && header->getNetworkProtocol() == 0xffff &&
                header->getSrcAddr() == domain->getPeer(networkInterface).alias &&
                header->getDestAddr() == resolveAlias(request->getDestAddress());
        }
        catch (const cRuntimeError&) {
            return false;
        }
    }
    return true;
}

void LowpanCompatibilityMac::handleSelfMessage(cMessage *message)
{
    if (message == ccaTimer && macState == CCA_3) {
        auto candidate = currentTxFrame != nullptr ? currentTxFrame : (txQueue->isEmpty() ? nullptr : txQueue->getPacket(0));
        bool valid = candidate == nullptr || isPreparedFrameValid(candidate, currentTxFrame != nullptr);
        bool timely = candidate == nullptr || (valid && canMeetDeliveryDeadline(candidate));
        if (candidate != nullptr && (!valid || !timely)) {
            if (currentTxFrame == nullptr)
                currentTxFrame = dequeuePacket();
            LowpanPacketDropDetails details;
            details.setReason(OTHER_PACKET_DROP);
            details.setLowpanReason(valid ? LOWPAN_LOWER_LIFETIME_EXPIRED : LOWPAN_ENVELOPE_INVALIDATED);
            dropCurrentTxFrame(details);
            nbDroppedFrames++;
            txAttempts = 0;
            transmissionAttemptInterruptedByRx = false;
            radio->setRadioMode(physicallayer::IRadio::RADIO_MODE_RECEIVER);
            manageQueue();
            return;
        }
    }
    Ieee802154Mac::handleSelfMessage(message);
}

} } // namespace inet::lowpan
