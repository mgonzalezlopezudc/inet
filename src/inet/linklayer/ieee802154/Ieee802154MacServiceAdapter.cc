// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154MacServiceAdapter.h"
#include "inet/common/Protocol.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee802154/Ieee802154MacServiceTag.h"

namespace inet {

Define_Module(Ieee802154MacServiceAdapter);

void Ieee802154MacServiceAdapter::initialize()
{
    service.reference(this, "serviceModule", true);
    const char *name = par("upperProtocol");
    if (*name)
        upperProtocol = Protocol::getProtocol(name);
}

void Ieee802154MacServiceAdapter::handleMessage(cMessage *message)
{
    auto packet = check_and_cast<Packet *>(message);
    if (!packet->arrivedOn("upperLayerIn"))
        throw cRuntimeError("Unexpected MAC service adapter input");
    auto request = packet->getTag<Ieee802154MacDataReq>()->request;
    packet->removeTag<Ieee802154MacDataReq>();
    if (lastRequestId == UINT64_MAX)
        throw cRuntimeError("IEEE 802.15.4 service request IDs exhausted");
    auto id = ++lastRequestId;
    auto admission = service ? service->submitData(id, packet, request) : Ieee802154MacServiceAdmission::UNAVAILABLE;
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        delete packet;
        auto result = new Packet("MCPS-DATA.refused");
        auto tag = result->addTag<Ieee802154MacDataResultInd>();
        tag->requestId = id;
        tag->msduHandle = request.msduHandle;
        tag->admission = admission;
        send(result, "upperLayerOut");
    }
}

void Ieee802154MacServiceAdapter::handleDataConfirm(Ieee802154MacServiceRequestId id, const Ieee802154MacDataConfirm& confirm)
{
    Enter_Method("handleDataConfirm");
    auto packet = new Packet("MCPS-DATA.confirm");
    auto tag = packet->addTag<Ieee802154MacDataResultInd>();
    tag->requestId = id;
    tag->msduHandle = confirm.msduHandle;
    tag->confirm = confirm;
    send(packet, "upperLayerOut");
}

void Ieee802154MacServiceAdapter::prepareIndication(Packet *packet)
{
    take(packet);
    // Wire receptions cannot depend on sender-side local tags.
    packet->clearTags();
}

void Ieee802154MacServiceAdapter::handleDataIndication(Packet *packet, const Ieee802154MacDataIndication& indication)
{
    Enter_Method("handleDataIndication");
    prepareIndication(packet);
    packet->addTag<Ieee802154MacDataInd>()->indication = indication;
    if (upperProtocol)
        packet->addTag<PacketProtocolTag>()->setProtocol(upperProtocol);
    send(packet, "upperLayerOut");
}

void Ieee802154MacServiceAdapter::handlePromiscuousIndication(Packet *packet, const Ieee802154MacPromiscuousIndication& indication)
{
    Enter_Method("handlePromiscuousIndication");
    prepareIndication(packet);
    packet->addTag<Ieee802154MacPromiscuousInd>()->indication = indication;
    send(packet, "upperLayerOut");
}

void Ieee802154MacServiceAdapter::handleCommStatusIndication(Packet *packet, const Ieee802154MacCommStatusIndication& indication)
{
    Enter_Method("handleCommStatusIndication");
    if (!packet)
        packet = new Packet("MLME-COMM-STATUS.indication");
    prepareIndication(packet);
    packet->addTag<Ieee802154MacCommStatusInd>()->indication = indication;
    send(packet, "upperLayerOut");
}

void Ieee802154MacServiceAdapter::handleGetConfirm(Ieee802154MacServiceRequestId, const Ieee802154MacGetConfirm&)
{
    throw cRuntimeError("Native data adapter did not issue GET");
}

void Ieee802154MacServiceAdapter::handleSetConfirm(Ieee802154MacServiceRequestId, const Ieee802154MacSetConfirm&)
{
    throw cRuntimeError("Native data adapter did not issue SET");
}

void Ieee802154MacServiceAdapter::handleResetConfirm(Ieee802154MacServiceRequestId, const Ieee802154MacResetConfirm&)
{
    throw cRuntimeError("Native data adapter did not issue RESET");
}

} // namespace inet
