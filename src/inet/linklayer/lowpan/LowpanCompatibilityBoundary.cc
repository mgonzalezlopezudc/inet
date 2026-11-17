// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanCompatibilityBoundary.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/lowpan/LowpanCompatibilityMac.h"
#include "inet/queueing/queue/PacketQueue.h"

namespace inet { namespace lowpan {

Define_Module(LowpanCompatibilityBoundary);

void LowpanCompatibilityBoundary::initialize(int stage)
{
    LowpanBoundaryBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        domain = getModuleFromPar<LowpanLinkDomain>(par("linkDomainModule"), this);
}

bool LowpanCompatibilityBoundary::isMacDomainValid(cModule *module) const
{
    domain->getPeer(networkInterface);
    auto compatible = dynamic_cast<LowpanCompatibilityMac *>(module);
    return compatible != nullptr && compatible->getLinkDomain() == domain;
}

Ieee802154Address LowpanCompatibilityBoundary::getLinkAddress() const
{
    return domain->getPeer(networkInterface).address;
}

uint16_t LowpanCompatibilityBoundary::getPanId() const
{
    return domain->getPanId();
}

const Ieee802154Address *LowpanCompatibilityBoundary::resolveNeighbor(const Ipv6Address& nextHop) const
{
    return domain->getAddressMap().findNeighbor(networkInterface->getId(), nextHop);
}

void LowpanCompatibilityBoundary::handleMessage(cMessage *message)
{
    if (message->getArrivalGate() != gate("lowerLayerIn")) {
        PacketPusherBase::handleMessage(message);
        return;
    }
    auto packet = check_and_cast<Packet *>(message);
    const auto& legacy = packet->getTag<MacAddressInd>();
    auto source = domain->getAddressMap().findIdentity(legacy->getSrcAddress());
    Ieee802154Address destination;
    if (legacy->getDestAddress().isBroadcast())
        destination = Ieee802154Address("ffff");
    else if (auto identity = domain->getAddressMap().findIdentity(legacy->getDestAddress()))
        destination = *identity;
    if (source == nullptr || destination.isUnspecified()) {
        dropPacket(packet, ADDRESS_RESOLUTION_FAILED);
        return;
    }
    auto indication = packet->addTagIfAbsent<Ieee802154AddressInd>();
    indication->setSrcAddress(*source);
    indication->setDestAddress(destination);
    indication->setSrcPanId(domain->getPanId());
    indication->setDestPanId(domain->getPanId());
    packet->removeTagIfPresent<MacAddressInd>();
    packet->removeTagIfPresent<MacAddressReq>();
    packet->removeTagIfPresent<Ieee802154AddressReq>();
    packet->removeTagIfPresent<LowpanTransmissionReq>();
    send(packet, "upperLayerOut");
}

} } // namespace inet::lowpan
