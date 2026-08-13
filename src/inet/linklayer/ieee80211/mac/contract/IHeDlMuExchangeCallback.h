//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHEDLMUEXCHANGECALLBACK_H
#define __INET_IHEDLMUEXCHANGECALLBACK_H

#include <optional>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Defs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"

namespace inet {

class Packet;

namespace queueing { class IPacketQueue; }

namespace ieee80211 {

class IOriginatorBlockAckAgreementHandler;
class IOriginatorMacDataService;
class IQosRateSelection;
struct Ieee80211NegotiatedHeCapabilities;

/** Immutable identity and PHY selection for one committed HE DL MU member. */
struct INET_API HeDlMuMember
{
    HcfPacketIdentity packetIdentity;
    Packet *packet = nullptr;
    MacAddress peer;
    Tid tid = 0;
    AccessCategory accessCategory = AC_BE;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    int ruToneSize = 0;
};

enum class HeDlMuUserOutcome {
    BLOCK_ACK_RECEIVED,
    BLOCK_ACK_TIMED_OUT,
};

/**
 * Typed provider boundary used by HeDlMuTxOpFs.
 *
 * The frame sequence reports immutable transaction/member outcomes and asks
 * the provider to resolve only opaque Phase-6 queue tokens. It never inspects
 * HeHcf, its parent module, or the active frame-sequence owner.
 */
class INET_API IHeDlMuExchangeCallback
{
  public:
    virtual ~IHeDlMuExchangeCallback() {}

    virtual queueing::IPacketQueue *resolveHeDlMuQueue(HcfQueueToken token) const = 0;
    virtual Packet *getReservedHeDlMuPacket(uint64_t transactionToken,
            const MacAddress& peer) const = 0;
    virtual bool isReservedHeDlMuPacket(uint64_t transactionToken,
            const MacAddress& peer, const Packet *packet) const = 0;
    virtual IOriginatorBlockAckAgreementHandler *getHeDlMuBlockAckHandler() const = 0;
    virtual IOriginatorMacDataService *getHeDlMuOriginatorDataService() const = 0;
    virtual IQosRateSelection *getHeDlMuRateSelection() const = 0;
    virtual MacAddress getHeDlMuTransmitterAddress() const = 0;
    virtual int getHeDlMuFcsMode() const = 0;
    virtual uint8_t getHeDlMuBssColor() const = 0;
    virtual uint16_t getHeDlMuAssociationId(const MacAddress& peer) const = 0;
    virtual std::optional<Ieee80211NegotiatedHeCapabilities>
            getHeDlMuNegotiatedCapabilities(const MacAddress& peer) const = 0;

    virtual void heDlMuPlanFinalized(uint64_t transactionToken,
            const std::vector<HeDlMuMember>& members) = 0;
    virtual void heDlMuPlanCommitted(uint64_t transactionToken,
            Packet *containerPacket, const std::vector<HeDlMuMember>& members) = 0;
    virtual void heDlMuMemberTransmitted(uint64_t transactionToken,
            const HeDlMuMember& member) = 0;
    virtual void heDlMuUserOutcome(uint64_t transactionToken,
            const MacAddress& peer, HeDlMuUserOutcome outcome) = 0;
    virtual void heDlMuPlanningFailed(uint64_t transactionToken,
            AccessCategory accessCategory) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
