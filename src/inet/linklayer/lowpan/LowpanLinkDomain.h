// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANLINKDOMAIN_H
#define __INET_LOWPANLINKDOMAIN_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/lowpan/LowpanLinkAddressMap.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"

namespace inet { namespace lowpan {

/** Shared immutable configuration for a dedicated compatibility radio medium. */
class INET_API LowpanLinkDomain : public SimpleModule
{
  public:
    struct Peer {
        NetworkInterface *networkInterface = nullptr;
        physicallayer::IRadio *radio = nullptr;
        Ieee802154Address address;
        MacAddress alias;
    };

  protected:
    uint16_t panId = 0xffff;
    const physicallayer::IRadioMedium *medium = nullptr;
    LowpanLinkAddressMap addressMap;
    std::map<int, Peer> peers;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    void validateParticipants(cModule *module) const;

  public:
    uint16_t getPanId() const { return panId; }
    const LowpanLinkAddressMap& getAddressMap() const { return addressMap; }
    const Peer& getPeer(const NetworkInterface *networkInterface) const;
    simtime_t getMaximumPropagationDelay(const NetworkInterface *sender) const;
};

} } // namespace inet::lowpan
#endif
