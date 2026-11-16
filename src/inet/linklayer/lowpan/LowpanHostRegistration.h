// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANHOSTREGISTRATION_H
#define __INET_LOWPANHOSTREGISTRATION_H

#include "inet/linklayer/lowpan/LowpanNativeLinkDomain.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanRegistrationInfo.h"
#include "inet/networklayer/icmpv6/Ipv6NdMessage_m.h"
#include <optional>

namespace inet { namespace lowpan {
class LowpanIpv6NeighbourDiscovery;

// Host policy belongs to its ND owner; this helper introduces no module or event boundary.
class INET_API LowpanHostRegistration
{
  protected:
    using RouterKey = std::pair<int, Ipv6Address>;
    using AddressKey = std::pair<int, Ipv6Address>;
    struct AcceptanceCorrelation {
        LowpanRegistrationInfo full;
        Ipv6Address source;
        Ieee802154Address localNative;
        Ieee802154Address peer;
        bool updated = false;
        bool registrarRecencyKnown = false;
        simtime_t validUntil;
    };
    struct RegistrationCopy {
        AddressKey assignment;
        LowpanRegistrationInfo full;
        LowpanRegistrationInfo wire;
        bool updated = false;
        bool registrarRecencyKnown = false;
        Ipv6Address source;
        Ieee802154Address localNative;
        Ieee802154Address peer;
        opp_component_ptr<LowpanNativeLinkDomain> domain;
        int interfaceModuleId = -1;
        std::optional<Ipv6Address> authority;
        int lifetime = 0;
        std::optional<AcceptanceCorrelation> previous;
    };
    struct Transaction {
        bool removal = false;
        std::map<RouterKey, RegistrationCopy> copies;
    };
    struct History {
        std::vector<uint8_t> rovr;
        bool allocated = false;
        bool rejected = false;
        uint8_t tid = 0;
        std::optional<Transaction> transaction;
        std::vector<std::pair<RouterKey, RegistrationCopy>> removals;
        unsigned failures = 0;
        simtime_t blockedUntil;
        std::map<Ipv6Address, simtime_t> authorityCooldowns;
    };
    struct Address {
        bool autonomous = false;
        bool rejected = false;
        simtime_t validUntil;
    };
    struct Accepted {
        simtime_t validUntil;
        simtime_t refreshAt;
        std::optional<RegistrationCopy> copy;
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
        bool updated = false;
        simtime_t capabilityUntil;
        std::optional<Ipv6Address> authority;
        simtime_t authorityUntil;
    };
    struct Discovery {
        bool active = false;
        int attempts = 0;
        simtime_t next = SIMTIME_MAX;
        simtime_t lastSent = SimTime::fromRaw(-1); // negative means no RS has been sent
        simtime_t blockedUntil;
    };
    LowpanIpv6NeighbourDiscovery& owner;
    cMessage *hostTimer = nullptr;
    std::map<RouterKey, Router> routers;
    std::map<AddressKey, Address> addresses;
    std::map<int, Discovery> discoveries;
    std::map<AddressKey, History> histories;
    std::vector<uint8_t> configuredRovr;
    int maxRouters = 0;
    int maxAddresses = 0;
    int registrationLifetime = 0; // minutes, as carried by ARO
    double retryIntervalSeconds = 0;
    double refreshFraction = 0;
    bool updatingPrefixes = false;
    bool updatingAddresses = false;
    AddressKey historyKey(const AddressKey& key) const;
    History *enrollAddress(NetworkInterface *interface, const Ipv6Address& address);
    bool readAuthority(const Ipv6NdOptions& options, std::optional<Ipv6Address>& authority, simtime_t& until) const;
    bool validCopy(const RouterKey& key, const RegistrationCopy& copy) const;
    bool blocked(History& history, const Router& router);
    void coolDown(History& history, const std::optional<Ipv6Address>& authority, bool group);
    void beginTransaction(const AddressKey& key, bool removal,
            const std::vector<std::pair<RouterKey, RegistrationCopy>>& saved = {},
            std::optional<RouterKey> initiatingRouter = std::nullopt);
    void finishCopy(const RouterKey& key, Ipv6Address address);
    void startExtendedRegistrations();
    void startNextExtendedRegistration(const RouterKey& key);
    void sendExtendedRegistration(const RouterKey& key, const RegistrationCopy& copy);
    void processExtendedReply(Packet *packet, const Ipv6NeighbourAdvertisement *message,
            NetworkInterface *interface, LowpanNativeLinkDomain *domain);
    void removeExtendedAddress(const AddressKey& key, bool reject);
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
    simtime_t registrationRetryDeadline(NetworkInterface *interface, bool finalAttempt) const;
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
    void addressAssigned(NetworkInterface *interface, const Ipv6Address& address);
    void addressRemoved(NetworkInterface *interface, const Ipv6Address& address);
    // True means the helper consumed the packet on every success/drop path.
    bool processPacket(Packet *packet, const Icmpv6Header *message, NetworkInterface *interface,
            LowpanNativeLinkDomain *domain);
    bool isSourceUsable(NetworkInterface *interface, const Ipv6Address& address) const;
    size_t getNumRouters() const { return routers.size(); }
    size_t getNumAddresses() const { return addresses.size(); }
    size_t getNumHistories() const { return histories.size(); }
    void clear();
};
} }
#endif
