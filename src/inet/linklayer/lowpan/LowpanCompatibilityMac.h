// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANCOMPATIBILITYMAC_H
#define __INET_LOWPANCOMPATIBILITYMAC_H

#include "inet/linklayer/ieee802154/Ieee802154Mac.h"
#include "inet/linklayer/lowpan/LowpanLinkDomain.h"
#include "inet/linklayer/lowpan/contract/ILowpanMac.h"

namespace inet { namespace lowpan {

class INET_API LowpanCompatibilityMac : public Ieee802154Mac, public ILowpanMac
{
  protected:
    static constexpr int HEADER_BYTES = 23;
    static constexpr int FRAME_BYTES = 127;
    static constexpr int PAYLOAD_BYTES = FRAME_BYTES - HEADER_BYTES;
    static constexpr int PROFILE_ID = 1;
    LowpanLinkDomain *domain = nullptr;
    simtime_t maxLowerDeliveryLifetime;
    virtual void initialize(int stage) override;
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void configureNetworkInterface() override;
    virtual void encapsulate(Packet *packet) override;
    virtual void handleSelfMessage(cMessage *message) override;
    bool isPreparedFrameValid(const Packet *packet, bool encapsulated) const;
    bool isNativeRequestValid(const Ieee802154AddressReq& request) const;
    simtime_t getTransmissionTail() const;
    bool canMeetDeliveryDeadline(const Packet *packet) const;
    MacAddress resolveAlias(const Ieee802154Address& address) const;

  public:
    const LowpanLinkDomain *getLinkDomain() const { return domain; }
    virtual Ptr<const LowpanTransmissionReq> prepareTransmission(const Ieee802154AddressReq& request) const override;
};

} } // namespace inet::lowpan
#endif
