// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANIPV6NEIGHBOURDISCOVERY_H
#define __INET_LOWPANIPV6NEIGHBOURDISCOVERY_H

#include "inet/networklayer/icmpv6/Ipv6NeighbourDiscovery.h"
#include "inet/linklayer/lowpan/LowpanNativeLinkDomain.h"
#include <tuple>

namespace inet { namespace lowpan {
class LowpanHostRegistration;

// Native registration ownership is separate from the ordinary 48-bit ND cache.
class INET_API LowpanIpv6NeighbourDiscovery : public Ipv6NeighbourDiscovery
{
    friend class LowpanHostRegistration;
  protected:
    std::unique_ptr<LowpanHostRegistration> hostRegistration;
    bool hostAddressSubscription = false;
    struct Registration {
        Ieee802154Address owner;
        Ieee802154Address address;
        opp_component_ptr<LowpanNativeLinkDomain> domain;
        int interfaceModuleId = -1;
        simtime_t validUntil;
    };
    using RegistrationKey = std::pair<int, Ipv6Address>;
    std::map<RegistrationKey, Registration> registrations;
    bool registrationRouter = false;
    int maxRegistrations = 0;
    cMessage *registrationExpiryTimer = nullptr;
    using RouterAdvertisementKey = std::tuple<int, Ipv6Address, Ieee802154Address>;
    std::map<RouterAdvertisementKey, simtime_t> pendingRouterAdvertisements;
    cMessage *routerAdvertisementTimer = nullptr;
    int maxPendingRouterAdvertisements = 0;
    static simsignal_t registrationCountChangedSignal;
    static simsignal_t nativeRouterDiscoveredSignal;
    static simsignal_t nativeHostRegistrationSucceededSignal;
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *message) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details) override;
    virtual void start() override;
    virtual void stop() override;
    virtual void initiateDad(const Ipv6Address& tentativeAddr, NetworkInterface *ie) override;
    virtual void startRouterDiscovery(NetworkInterface *ie) override;
    virtual void createRaTimer(NetworkInterface *ie) override;
    virtual void processNDMessage(Packet *packet, const Icmpv6Header *message) override;
    virtual void processIpv6Datagram(Packet *packet) override;
    void processRegistration(Packet *packet, const Ipv6NeighbourSolicitation *message, NetworkInterface *interface,
            LowpanNativeLinkDomain *domain);
    void processNativeNeighbourSolicitation(Packet *packet, const Ipv6NeighbourSolicitation *message,
            NetworkInterface *interface, LowpanNativeLinkDomain *domain);
    bool readRegistration(const Packet *packet, const Ipv6NeighbourSolicitation *message, NetworkInterface *interface,
            LowpanNativeLinkDomain *domain, Registration& requested, const Ipv6NdOption *&aro) const;
    int applyRegistration(const RegistrationKey& key, const Registration& requested);
    void sendRegistrationAdvertisement(const RegistrationKey& key, const Registration& requested,
            const Ipv6NdOption& aro, int status, const Ipv6Address& routerAddress);
    bool readNativeSource(const Packet *packet, const Ipv6NdOptions& options, LowpanNativeLinkDomain *domain,
            Ieee802154Address& address) const;
    Ipv6NdOption *createNativeLinkOption(const Ieee802154Address& address) const;
    void processNativeRouterSolicitation(Packet *packet, const Ipv6RouterSolicitation *message,
            NetworkInterface *interface, LowpanNativeLinkDomain *domain);
    void sendNativeRouterAdvertisement(const RouterAdvertisementKey& key);
    void scheduleRouterAdvertisements();
    void clearRouterAdvertisements();
    void updateHostAddressSubscription(bool enabled);
    void removeRegistration(const RegistrationKey& key, bool notify = true);
    void clearRegistrations(bool notify);
    void expireRegistrations();
    void scheduleRegistrationExpiry();
    void removeRegistrationRoute(const RegistrationKey& key);

  public:
    virtual ~LowpanIpv6NeighbourDiscovery();
    bool isNativeSourceUsable(NetworkInterface *interface, const Ipv6Address& address) const;
    bool usesNativeHostRegistration() const { return hostRegistration != nullptr; }
    virtual void sendRedirect(Packet *redirectedPacket, const Ipv6Address& targetAddr,
            const Ipv6Address& destAddr, NetworkInterface *ie) override;
};

} } // namespace inet::lowpan
#endif
