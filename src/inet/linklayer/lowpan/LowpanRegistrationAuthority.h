// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANREGISTRATIONAUTHORITY_H
#define __INET_LOWPANREGISTRATIONAUTHORITY_H

#include "inet/networklayer/icmpv6/Ipv6NdMessage_m.h"
#include <array>

namespace inet {
class Packet;
namespace lowpan {
class LowpanIpv6NeighbourDiscovery;

// The routed DAD table is independent of native neighbor bindings and data routes.
class INET_API LowpanRegistrationAuthority
{
  protected:
    struct Entry {
        std::array<uint8_t, 8> owner;
        simtime_t validUntil; // absolute deadline; equality is expired
    };
    LowpanIpv6NeighbourDiscovery& owner;
    std::map<Ipv6Address, Entry> entries;
    size_t capacity = 0;
    cMessage *expiryTimer = nullptr;
    bool isLocalAuthorityAddress(const Ipv6Address& address) const;
    bool hasValidOptions(const Packet *packet, b offset) const;
    void expireEntries(bool notify = true);
    void scheduleExpiry();
    void sendConfirmation(const Ipv6DuplicateAddressHeader& request, const Ipv6Address& source,
            const Ipv6Address& destination, int status);
    void drop(Packet *packet, bool unsupported = false);

  public:
    explicit LowpanRegistrationAuthority(LowpanIpv6NeighbourDiscovery& owner);
    ~LowpanRegistrationAuthority();
    bool ownsTimer(const cMessage *message) const { return message == expiryTimer; }
    void handleTimer();
    // True means the request was consumed, including malformed/unsupported requests.
    bool processPacket(Packet *packet, const Icmpv6Header *message);
    // Admission is read-only, including at an expiry boundary. Commit cannot notify
    // until the native caller has established its binding and data route as well.
    int checkRegistration(const Ipv6Address& address, const std::array<uint8_t, 8>& requestedOwner, bool remove) const;
    bool commitRegistration(const Ipv6Address& address, const std::array<uint8_t, 8>& requestedOwner, simtime_t validUntil);
    void publishEntryCount();
    size_t getNumEntries() const { return entries.size(); }
    void clear(bool notify = true);
};
} }
#endif
