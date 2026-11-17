// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANHOSTREGISTRATION_H
#define __INET_LOWPANHOSTREGISTRATION_H

#include "inet/linklayer/lowpan/LowpanNativeLinkDomain.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/networklayer/icmpv6/Ipv6NdMessage_m.h"

namespace inet { namespace lowpan {
class LowpanIpv6NeighbourDiscovery;

// Host policy belongs to its ND owner; this helper introduces no module or event boundary.
class INET_API LowpanHostRegistration
{
  protected:
    using RouterKey = std::pair<int, Ipv6Address>;
    using AddressKey = std::pair<int, Ipv6Address>;
    struct Address {
        bool autonomous = false;
        bool rejected = false;
        simtime_t validUntil;
    };
    struct Accepted {
        simtime_t validUntil;
        simtime_t refreshAt;
    };
    struct Router {
        Ieee802154Address address;
        opp_component_ptr<LowpanNativeLinkDomain> domain;
        int interfaceModuleId = -1;
        simtime_t validUntil;
        simtime_t informationRefreshAt;
        std::map<Ipv6Address, Accepted> registrations;
        Ipv6Address pending;
        int attempts = 0;
        simtime_t retryAt = SIMTIME_MAX;
        simtime_t blockedUntil;
    };
    struct Discovery {
        bool active = false;
        int attempts = 0;
        simtime_t next = SIMTIME_MAX;
        simtime_t lastSent = SimTime(-10);
        simtime_t blockedUntil;
    };
    LowpanIpv6NeighbourDiscovery& owner;
    cMessage *hostTimer = nullptr;
    std::map<RouterKey, Router> routers;
    std::map<AddressKey, Address> addresses;
    std::map<int, Discovery> discoveries;
    int maxRouters = 0;
    int maxAddresses = 0;
    int registrationLifetime = 0; // minutes, as carried by ARO
    simtime_t retryInterval;
    double refreshFraction = 0;
    bool updatingPrefixes = false;
    void scheduleTimer();
    void sendRouterSolicitation(NetworkInterface *interface, Discovery& discovery);
    void collectAddresses(NetworkInterface *interface);
    void processPrefixes(NetworkInterface *interface, const Ipv6NdOptions& options);
    void processAdvertisement(Packet *packet, const Ipv6RouterAdvertisement *message,
            NetworkInterface *interface, LowpanNativeLinkDomain *domain);
    void processRegistrationReply(Packet *packet, const Ipv6NeighbourAdvertisement *message,
            NetworkInterface *interface, LowpanNativeLinkDomain *domain);
    void startNextRegistration(const RouterKey& key);
    void sendRegistration(const RouterKey& key, const Ipv6Address& address, int lifetime);
    void removeRouter(const RouterKey& key, bool rediscover);
    void removeRouterRoute(const RouterKey& key);
    void rejectAddress(const AddressKey& key);
    void expireAddresses();
    void drop(Packet *packet, LowpanDropReason reason = LOWPAN_MALFORMED);

  public:
    explicit LowpanHostRegistration(LowpanIpv6NeighbourDiscovery& owner);
    ~LowpanHostRegistration();
    bool ownsTimer(const cMessage *message) const { return message == hostTimer; }
    void handleTimer();
    void startDiscovery(NetworkInterface *interface, bool backoff = false);
    void addressAssigned(NetworkInterface *interface);
    // True means the helper consumed the packet on every success/drop path.
    bool processPacket(Packet *packet, const Icmpv6Header *message, NetworkInterface *interface,
            LowpanNativeLinkDomain *domain);
    bool isSourceUsable(NetworkInterface *interface, const Ipv6Address& address) const;
    size_t getNumRouters() const { return routers.size(); }
    size_t getNumAddresses() const { return addresses.size(); }
    void clear();
};
} }
#endif
