// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_HEHCFTXRXINTERCEPTOR_H
#define __INET_HEHCFTXRXINTERCEPTOR_H

#include "inet/linklayer/ieee80211/mac/contract/IHcfTxRxInterceptor.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211HeOmi.h"

namespace inet {
namespace ieee80211 {

/** Fixed-order HE interceptor; it never invokes the common HCF path itself. */
class INET_API HeHcfTxRxInterceptor final : public IHcfTxRxInterceptor
{
  public:
    enum class LocalRole { STATION, ACCESS_POINT, OTHER };

    class INET_API IActions
    {
      public:
        virtual ~IActions() {}
        virtual bool processHeSoundingFrame(Packet *, const Ptr<const Ieee80211MacHeader>&) = 0;
        virtual void rejectUnexpectedHeTb(Packet *) = 0;
        virtual void processHeTrigger(Packet *, const Ptr<const Ieee80211TriggerFrame>&) = 0;
        virtual void processHeMultiStaBlockAck(Packet *, const Ptr<const Ieee80211MultiStaBlockAck>&) = 0;
        virtual void observeBufferStatus(const Ptr<const Ieee80211DataHeader>&) = 0;
        virtual bool isOperatingModeControlSupported() const = 0;
        virtual void applyOperatingMode(const MacAddress&, const Ieee80211HeOperatingMode&) = 0;
        virtual AccessCategory mapTidToAccessCategory(Tid) const = 0;
        virtual void startMuEdcaTimer(AccessCategory) = 0;
        virtual bool isHeUlMuExchangeActive() const = 0;
        virtual void notifyHeUlMuPacketTransmitted(Packet *) = 0;
        virtual bool isHeDlMuContainer(const Packet *) const = 0;
        virtual void routeHeDlMuContainer(Packet *) = 0;
        virtual LocalRole getLocalRole() const = 0;
        virtual uint16_t getLocalAssociationId() const = 0;
        virtual bool hasActiveHeDlMuMembers() const = 0;
        virtual void processHeDlMuFailedFrame(Packet *) = 0;
        virtual MacAddress getLocalAddress() const = 0;
        virtual MacAddress getBssid() const = 0;
        virtual void transmitHeNdp(Packet *, const Ptr<const Ieee80211MacHeader>&, simtime_t) = 0;
    };

  private:
    IActions *actions;

  public:
    explicit HeHcfTxRxInterceptor(IActions *actions) : actions(actions) {}

    virtual Result processPhyIndication(Packet *packet) override;
    virtual Result processRejectedHeaderlessResponse(Packet *packet,
            IReceiveStep::HeaderlessResponseFamily family) override;
    virtual Result processRecipientFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual Result processTransmittedFrame(Packet *packet) override;
    virtual Result processReceivedResponse(Packet *packet, Packet *lastTransmittedPacket) override;
    virtual Result processFailedFrame(Packet *packet) override;
    virtual Result processTransmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual Result processTransmittedControl(const Ptr<const Ieee80211MacHeader>& header, AccessCategory accessCategory) override;
    virtual Result processTransmitRequest(Packet *packet, simtime_t ifs) override;
};

} // namespace ieee80211
} // namespace inet

#endif
