// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANBOUNDARYBASE_H
#define __INET_LOWPANBOUNDARYBASE_H
#include "inet/linklayer/lowpan/contract/ILowpanLink.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/queueing/base/PacketPusherBase.h"
namespace inet { namespace lowpan {
class INET_API LowpanBoundaryBase : public queueing::PacketPusherBase, public ILowpanLink
{
  protected:
    ILowpanMac *mac = nullptr;
    NetworkInterface *networkInterface = nullptr;
    virtual void initialize(int stage) override;
    virtual bool isMacDomainValid(cModule *module) const = 0;
  public:
    virtual Ptr<const LowpanTransmissionReq> prepareTransmission(const Ieee802154AddressReq& request) const override;
    virtual bool supportsPacketPushing(const cGate *gate) const override;
    virtual void pushPacket(Packet *packet, const cGate *gate) override;
};
} }
#endif
