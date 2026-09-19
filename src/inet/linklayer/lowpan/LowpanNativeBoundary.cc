// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanNativeMac.h"
#include "inet/common/ModuleAccess.h"
namespace inet { namespace lowpan {
Define_Module(LowpanNativeBoundary);
void LowpanNativeBoundary::initialize(int stage)
{
    LowpanBoundaryBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        domain = getModuleFromPar<LowpanNativeLinkDomain>(par("linkDomainModule"), this);
}
bool LowpanNativeBoundary::isMacDomainValid(cModule *module) const
{
    domain->getPeer(networkInterface);
    auto native = dynamic_cast<LowpanNativeMac *>(module);
    return native != nullptr && native->getLinkDomain() == domain;
}
Ieee802154Address LowpanNativeBoundary::getLinkAddress() const { return domain->getPeer(networkInterface).address; }
uint16_t LowpanNativeBoundary::getPanId() const { return domain->getPanId(); }
const Ieee802154Address *LowpanNativeBoundary::resolveNeighbor(const Ipv6Address& nextHop) const
{
    return domain->resolveNeighbor(networkInterface->getId(), nextHop);
}
void LowpanNativeBoundary::handleMessage(cMessage *message)
{
    if (message->getArrivalGate() != gate("lowerLayerIn")) {
        PacketPusherBase::handleMessage(message);
        return;
    }
    auto packet = check_and_cast<Packet *>(message);
    packet->getTag<Ieee802154AddressInd>(); // Native MAC owns decoded receive metadata.
    send(packet, "upperLayerOut");
}
} }
