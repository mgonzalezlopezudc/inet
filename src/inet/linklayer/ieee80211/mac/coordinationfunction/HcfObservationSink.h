//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFOBSERVATIONSINK_H
#define __INET_HCFOBSERVATIONSINK_H

#include "inet/common/INETDefs.h"

namespace inet {

class Packet;
class PacketDropDetails;

namespace ieee80211 {

class FrameSequenceContext;
class HePeerInvalidatedEvent;
class HePeerOperatingModeChangedEvent;
class HeTbResponseEvent;
class OriginatorBlockAckAgreement;
class RecipientBlockAckAgreement;

/**
 * Stateless mapping from typed HCF semantic events to the existing NED signals.
 */
class INET_API HcfObservationSink final
{
  private:
    HcfObservationSink() = delete;

  public:
    static void packetSentToPeer(cComponent *emitter, const Packet *packet);
    static void packetReceivedFromPeer(cComponent *emitter, const Packet *packet);
    static void packetDropped(cComponent *emitter, const Packet *packet,
            PacketDropDetails *details);
    static void linkBroken(cComponent *emitter, const Packet *packet);

    static void frameSequenceStarted(cComponent *emitter,
            const FrameSequenceContext *context);
    static void frameSequenceFinished(cComponent *emitter,
            const FrameSequenceContext *context);
    static void edcaCollisionDetected(cComponent *emitter,
            unsigned long collisionCount);

    static void recipientBlockAckAgreementAdded(cComponent *emitter,
            const RecipientBlockAckAgreement *agreement);
    static void originatorBlockAckAgreementAdded(cComponent *emitter,
            const OriginatorBlockAckAgreement *agreement);
    static void recipientBlockAckAgreementDeleted(cComponent *emitter,
            const RecipientBlockAckAgreement *agreement);
    static void originatorBlockAckAgreementDeleted(cComponent *emitter,
            const OriginatorBlockAckAgreement *agreement);

    static void ampduCreated(cComponent *emitter, const Packet *aggregate,
            unsigned long numMpdus);
    static void datarateSelected(cComponent *emitter, double datarate,
            Packet *packet);

    static void heTbResponseCommitted(cComponent *emitter,
            const HeTbResponseEvent *event);
    static void peerOperatingModeChanged(cComponent *emitter,
            const HePeerOperatingModeChangedEvent *event);
    static void peerInvalidated(cComponent *emitter,
            const HePeerInvalidatedEvent *event);
};

} // namespace ieee80211
} // namespace inet

#endif
