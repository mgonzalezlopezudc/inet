// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANREGISTRATIONRELAY_H
#define __INET_LOWPANREGISTRATIONRELAY_H

#include "inet/linklayer/lowpan/LowpanIpv6NeighbourDiscovery.h"
#include <optional>

namespace inet { namespace lowpan {

// Routed confirmation policy shares its ND owner's confirmed registrations.
class INET_API LowpanRegistrationRelay
{
    friend class LowpanIpv6NeighbourDiscovery;
  protected:
    using Key = LowpanIpv6NeighbourDiscovery::RegistrationKey;
    using Registration = LowpanIpv6NeighbourDiscovery::Registration;
    struct Pending {
        Registration requested;
        Ipv6NdOption aro;
        Ipv6Address replyAddress;
        int lifetime = 0;
        int attempts = 0;
        simtime_t expiresAt;
        simtime_t retryAt;
        std::vector<uint8_t> wireOwner;
        bool authorityUpdated = false;
        std::optional<Registration> superseded;
    };
    LowpanIpv6NeighbourDiscovery& owner;
    Ipv6Address authorityAddress;
    Ipv6Address sourceAddress;
    std::map<Key, Pending> pending;
    cMessage *timer = nullptr;
    bool hasValidSource() const;
    bool hasValidContext(const Key& key, const Pending& entry) const;
    size_t occupiedKeys() const;
    void scheduleTimer();
    void sendRequest(const Key& key);
    void complete(const Key& key, int status);
    void removeMatchingRegistration(const Key& key, const Ieee802154Address& requestedOwner);
    bool hasValidOptions(const Packet *packet, b offset) const;
    void processExtendedRegistration(const Key& key, const Registration& requested, const Ipv6NdOption& aro,
            const Ipv6Address& replyAddress);
    bool processExtendedConfirmation(Packet *packet, const Ipv6DuplicateAddressHeader& reply);

  public:
    explicit LowpanRegistrationRelay(LowpanIpv6NeighbourDiscovery& owner);
    ~LowpanRegistrationRelay();
    void start();
    void clear();
    bool ownsTimer(const cMessage *message) const { return message == timer; }
    void handleTimer();
    void processRegistration(const Key& key, const Registration& requested, const Ipv6NdOption& aro,
            const Ipv6Address& replyAddress);
    // True means the DAC packet was consumed on every validation/matching path.
    bool processPacket(Packet *packet, const Icmpv6Header *message);
    size_t getNumPending() const { return pending.size(); }
};

} }
#endif
