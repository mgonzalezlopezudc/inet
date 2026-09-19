// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanBoundaryBase.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/queueing/queue/PacketQueue.h"
namespace inet { namespace lowpan {
void LowpanBoundaryBase::initialize(int stage)
{
    PacketPusherBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        mac = getModuleFromPar<ILowpanMac>(par("macModule"), this);
        networkInterface = getContainingNicModule(this);
    }
    else if (stage == INITSTAGE_QUEUEING) {
        auto queue = check_and_cast<queueing::PacketQueue *>(consumer.get());
        auto connectedMac = getConnectedModule<cModule>(queue->gate("out"));
        if (queue->getParentModule() != networkInterface || dynamic_cast<ILowpanMac *>(connectedMac) != mac ||
            connectedMac->getParentModule() != networkInterface || !isMacDomainValid(connectedMac))
            throw cRuntimeError("LoWPAN boundary, queue and MAC must belong to the same interface and domain");
        if (*queue->par("dropperClass").stringValue() || *queue->par("bufferModule").stringValue() ||
            (queue->getMaxNumPackets() < 0 && queue->getMaxTotalLength() < b(0)))
            throw cRuntimeError("LoWPAN compatibility boundary requires a bounded PacketQueue without a dropper or external buffer");
        auto receiveOutput = connectedMac->gate("upperLayerOut");
        if (receiveOutput->getPathEndGate() != gate("lowerLayerIn"))
            throw cRuntimeError("LoWPAN boundary must receive directly from its MAC");
        for (auto gate = receiveOutput; gate != nullptr; gate = gate->getNextGate())
            if (gate->getChannel() != nullptr)
                throw cRuntimeError("LoWPAN deadline profile requires undelayed receive connections");
    }
}

Ptr<const LowpanTransmissionReq> LowpanBoundaryBase::prepareTransmission(const Ieee802154AddressReq& request) const
{
    return mac->prepareTransmission(request);
}

bool LowpanBoundaryBase::supportsPacketPushing(const cGate *gate) const
{
    return gate == inputGate || gate == outputGate;
}

void LowpanBoundaryBase::pushPacket(Packet *packet, const cGate *gate)
{
    Enter_Method("pushPacket");
    take(packet);
    const auto& request = packet->getTag<LowpanTransmissionReq>();
    auto prepared = prepareTransmission(*request);
    if (request->getProfile() != prepared->getProfile() || request->getPayloadLimit() != prepared->getPayloadLimit() ||
        request->getDeliveryDeadline() < SIMTIME_ZERO || request->getDeliveryDeadline() > prepared->getDeliveryDeadline() ||
        packet->getDataLength() > B(prepared->getPayloadLimit()))
        throw cRuntimeError("Invalid LoWPAN prepared frame at compatibility boundary");
    if (!consumer.canPushPacket(packet)) {
        dropPacket(packet, QUEUE_OVERFLOW);
        return;
    }
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    handlePacketProcessed(packet);
    consumer.pushPacket(packet);
}

} }
