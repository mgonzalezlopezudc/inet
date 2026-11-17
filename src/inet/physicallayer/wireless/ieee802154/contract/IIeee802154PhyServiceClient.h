// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE802154PHYSERVICECLIENT_H
#define __INET_IIEEE802154PHYSERVICECLIENT_H

#include "inet/physicallayer/wireless/ieee802154/contract/Ieee802154PhyServicePrimitives.h"

namespace inet {

class Packet;

namespace physicallayer {

/**
 * PHY callbacks carry borrowed, call-scoped records. Clients enter their
 * module context. Ordinary terminal callbacks may submit reentrantly after
 * the provider detaches the operation; reset/stop barriers retain precedence.
 * Transmit-start is nonterminal: the request remains active through PPDU end.
 * Start/terminal callbacks run at their actual recorded event boundary;
 * historical instants remain historical and predictions are named explicitly.
 * Successful confirmations contain the matching value/timing; aborts contain
 * no fabricated SUCCESS metadata. Self-deletion inside a callback is excluded.
 */
class INET_API IIeee802154PhyServiceClient
{
  public:
    virtual ~IIeee802154PhyServiceClient() = default;
    virtual void handleAvailabilityIndication(const Ieee802154PhyAvailabilityIndication& indication) = 0;

    virtual void handleTransmitStarted(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyTransmitStart& start) = 0;
    virtual void handleTransmitConfirm(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyTransmitConfirm& confirm) = 0;
    virtual void handleCcaConfirm(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyCcaConfirm& confirm) = 0;
    virtual void handleEdConfirm(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyEdConfirm& confirm) = 0;
    virtual void handleStateConfirm(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyStateConfirm& confirm) = 0;
    virtual void handleGetConfirm(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyGetConfirm& confirm) = 0;
    virtual void handleSetConfirm(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhySetConfirm& confirm) = 0;
    virtual void handleResetConfirm(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyResetConfirm& confirm) = 0;
    /** Complete PSDU including FCS transfers to the client, which takes it immediately. */
    virtual void handleReceiveIndication(Packet *psdu,
            const Ieee802154PhyReceiveIndication& indication) = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
