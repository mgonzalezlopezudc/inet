//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHEDLMUEXECUTIONSERVICES_H
#define __INET_IHEDLMUEXECUTIONSERVICES_H

#include <optional>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/HeDlMuExchangeTypes.h"

namespace inet {
namespace queueing { class IPacketQueue; }
namespace ieee80211 {

class IOriginatorBlockAckAgreementHandler;
class IOriginatorMacDataService;
class IQosRateSelection;
struct Ieee80211NegotiatedHeCapabilities;

class INET_API IHeDlMuExecutionServices
{
  public:
    virtual ~IHeDlMuExecutionServices() = default;
    virtual queueing::IPacketQueue *resolveHeDlMuQueue(HcfQueueToken token) const = 0;
    virtual Packet *getReservedHeDlMuPacket(HeDlMuExchangeId id,
            const MacAddress& peer) const = 0;
    virtual bool isReservedHeDlMuPacket(HeDlMuExchangeId id,
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
};

} // namespace ieee80211
} // namespace inet

#endif
