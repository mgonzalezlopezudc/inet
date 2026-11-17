// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANCONTEXTADVERTISEMENTS_H
#define __INET_LOWPANCONTEXTADVERTISEMENTS_H

#include <array>
#include <map>
#include <tuple>
#include <vector>

#include "inet/linklayer/lowpan/LowpanContextTable.h"
#include "inet/linklayer/ieee802154/Ieee802154Address.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/networklayer/icmpv6/Ipv6NdMessage_m.h"

namespace inet { namespace lowpan {
class LowpanIpv6NeighbourDiscovery;
class LowpanLayer;
class LowpanNativeLinkDomain;

// ND policy owns authority snapshots; adaptation owns effective codec state.
class INET_API LowpanContextAdvertisements
{
    using GroupKey = std::tuple<int, bool, Ipv6Address>; // host interface or -1, ABRO present, authority/source
    struct Context {
        Ipv6Address prefix;
        int length = 0;
        bool compress = false;
        simtime_t compressionUntil;
        simtime_t validUntil;
        simtime_t refreshAt = SIMTIME_MAX;
        bool withdrawal = false;
        double lifetimeSeconds = 0; // Wire duration; independent of clock saturation.
        simtime_t receivedAt;
        bool unlimited = false;
    };
    struct Prefix {
        Ipv6Address address;
        int length = 0;
        bool autonomous = false;
        simtime_t validUntil;
        simtime_t preferredUntil;
        simtime_t refreshAt = SIMTIME_MAX;
        double validSeconds = 0;
        double preferredSeconds = 0;
        simtime_t receivedAt;
        bool validUnlimited = false;
        bool preferredUnlimited = false;
    };
    struct Group {
        bool authorityExtended = false;
        uint32_t version = 0;
        std::array<uint8_t, 22> abro{}; // Forward the received ABRO body unchanged.
        simtime_t validUntil = SIMTIME_MAX;
        simtime_t refreshAt = SIMTIME_MAX;
        std::map<int, Context> contexts;
        std::vector<Prefix> prefixes;
    };
    struct Discovery {
        int attempts = 0;
        bool active = false;
        simtime_t next = SIMTIME_MAX;
    };
    struct Advertising {
        std::vector<GroupKey> remaining;
        int triggeredRounds = 0;
    };
    LowpanIpv6NeighbourDiscovery& owner;
    bool host = false;
    bool origin = false;
    int maxGroups = 0;
    Ipv6Address originAddress;
    uint32_t originVersion = 0;
    uint16_t originLifetime = 0;
    bool originSnapshotKnown = false;
    std::vector<uint8_t> originSnapshot;
    std::map<GroupKey, Group> groups;
    std::map<std::pair<int, int>, GroupKey> effectiveOwners;
    std::map<int, Discovery> discoveries;
    std::map<int, Advertising> advertising;
    cMessage *maintenanceTimer = nullptr;
    double refreshFraction = 0.8;

    std::vector<NetworkInterface *> interfaces() const;
    LowpanLayer *layer(NetworkInterface *interface) const;
    Group parse(const Ipv6RouterAdvertisement& message, bool& authoritative, Ipv6Address& authority) const;
    bool preflight(const GroupKey& key, const Group& group, NetworkInterface *ingress) const;
    void install(const GroupKey& key, const Group& group, NetworkInterface *ingress);
    void removeGroup(const GroupKey& key);
    void expire();
    void scheduleTimer();
    void sendSolicitation(NetworkInterface *interface, Discovery& discovery);
    void updateOrigin();
    void trigger();
    void appendOptions(Ipv6RouterAdvertisement& message, const Group& group) const;
    std::vector<GroupKey> liveGroups() const;

  public:
    explicit LowpanContextAdvertisements(LowpanIpv6NeighbourDiscovery& owner);
    ~LowpanContextAdvertisements();
    bool ownsTimer(const cMessage *message) const { return message == maintenanceTimer; }
    void handleTimer();
    void start();
    void clear();
    void addressReady(NetworkInterface *interface);
    // False means the group was rejected with no accepted-information mutation.
    bool accept(const Ipv6RouterAdvertisement& message, NetworkInterface *interface, const Ipv6Address& source);
    void processAdvertisement(Packet *packet, const Ipv6RouterAdvertisement *message,
            NetworkInterface *interface, LowpanNativeLinkDomain *domain);
    simtime_t nextHostRefresh() const;
    std::vector<int> consumeHostRefresh();
    void sendAdvertisements(NetworkInterface *interface, const Ipv6Address& destination,
            const Ieee802154Address& nativeDestination);
    void sendPeriodic(cMessage *message);
    size_t getNumGroups() const { return groups.size(); }
    bool hasAuthorityCapability(const Ipv6Address& authority) const;
};
} }
#endif
