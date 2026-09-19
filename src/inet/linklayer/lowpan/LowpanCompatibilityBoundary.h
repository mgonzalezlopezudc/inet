// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANCOMPATIBILITYBOUNDARY_H
#define __INET_LOWPANCOMPATIBILITYBOUNDARY_H

#include "inet/linklayer/lowpan/LowpanLinkDomain.h"
#include "inet/linklayer/lowpan/contract/ILowpanLink.h"
#include "inet/linklayer/lowpan/LowpanBoundaryBase.h"

namespace inet { namespace lowpan {

class INET_API LowpanCompatibilityBoundary : public LowpanBoundaryBase
{
  protected:
    LowpanLinkDomain *domain = nullptr;
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    virtual bool isMacDomainValid(cModule *module) const override;

  public:
    virtual Ieee802154Address getLinkAddress() const override;
    virtual uint16_t getPanId() const override;
    virtual const Ieee802154Address *resolveNeighbor(const Ipv6Address& nextHop) const override;
};

} } // namespace inet::lowpan
#endif
