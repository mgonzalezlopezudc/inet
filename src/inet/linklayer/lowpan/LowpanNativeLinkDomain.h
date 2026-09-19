// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANNATIVELINKDOMAIN_H
#define __INET_LOWPANNATIVELINKDOMAIN_H
#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee802154/Ieee802154Address.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/networklayer/contract/ipv6/Ipv6Address.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
namespace inet { namespace lowpan {
class INET_API LowpanNativeLinkDomain : public SimpleModule
{
  public:
    struct Peer {
        NetworkInterface *networkInterface = nullptr;
        physicallayer::IRadio *radio = nullptr;
        Ieee802154Address address;
    };
  protected:
    uint16_t panId = 0xffff;
    const physicallayer::IRadioMedium *medium = nullptr;
    std::map<int, Peer> peers;
    std::map<std::pair<int, Ipv6Address>, Ieee802154Address> neighbors;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    void validateParticipants(cModule *module) const;
  public:
    uint16_t getPanId() const { return panId; }
    const Peer& getPeer(const NetworkInterface *networkInterface) const;
    const Ieee802154Address *resolveNeighbor(int interfaceId, const Ipv6Address& nextHop) const;
    bool contains(const Ieee802154Address& address) const;
    simtime_t getMaximumPropagationDelay(const NetworkInterface *sender) const;
};
} }
#endif
