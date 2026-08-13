//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfObservationSink.h"

#include "inet/common/Simsignals_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"

namespace inet {
namespace ieee80211 {

void HcfObservationSink::packetSentToPeer(cComponent *emitter, const Packet *packet)
{
    emitter->emit(cComponent::registerSignal("packetSentToPeer"), packet);
}

void HcfObservationSink::packetReceivedFromPeer(cComponent *emitter, const Packet *packet)
{
    emitter->emit(cComponent::registerSignal("packetReceivedFromPeer"), packet);
}

void HcfObservationSink::packetDropped(cComponent *emitter, const Packet *packet,
        PacketDropDetails *details)
{
    emitter->emit(cComponent::registerSignal("packetDropped"), packet, details);
}

void HcfObservationSink::linkBroken(cComponent *emitter, const Packet *packet)
{
    emitter->emit(cComponent::registerSignal("linkBroken"), packet);
}

void HcfObservationSink::frameSequenceStarted(cComponent *emitter,
        const FrameSequenceContext *context)
{
    emitter->emit(cComponent::registerSignal("frameSequenceStarted"), context);
}

void HcfObservationSink::frameSequenceFinished(cComponent *emitter,
        const FrameSequenceContext *context)
{
    emitter->emit(cComponent::registerSignal("frameSequenceFinished"), context);
}

void HcfObservationSink::edcaCollisionDetected(cComponent *emitter,
        unsigned long collisionCount)
{
    emitter->emit(cComponent::registerSignal("edcaCollisionDetected"), collisionCount);
}

void HcfObservationSink::recipientBlockAckAgreementAdded(cComponent *emitter,
        const RecipientBlockAckAgreement *agreement)
{
    emitter->emit(cComponent::registerSignal("blockAckAgreementAdded"), agreement);
}

void HcfObservationSink::originatorBlockAckAgreementAdded(cComponent *emitter,
        const OriginatorBlockAckAgreement *agreement)
{
    emitter->emit(cComponent::registerSignal("blockAckAgreementAdded"), agreement);
}

void HcfObservationSink::recipientBlockAckAgreementDeleted(cComponent *emitter,
        const RecipientBlockAckAgreement *agreement)
{
    emitter->emit(cComponent::registerSignal("blockAckAgreementDeleted"), agreement);
}

void HcfObservationSink::originatorBlockAckAgreementDeleted(cComponent *emitter,
        const OriginatorBlockAckAgreement *agreement)
{
    emitter->emit(cComponent::registerSignal("blockAckAgreementDeleted"), agreement);
}

void HcfObservationSink::ampduCreated(cComponent *emitter, const Packet *aggregate,
        unsigned long numMpdus)
{
    emitter->emit(cComponent::registerSignal("ampduCreated"), aggregate);
    emitter->emit(cComponent::registerSignal("ampduNumMpdus"), numMpdus);
}

void HcfObservationSink::datarateSelected(cComponent *emitter, double datarate,
        Packet *packet)
{
    emitter->emit(cComponent::registerSignal("datarateSelected"), datarate, packet);
}

void HcfObservationSink::heTbResponseCommitted(cComponent *emitter,
        const HeTbResponseEvent *event)
{
    emitter->emit(cComponent::registerSignal("heTbResponseCommitted"), event);
    emitter->emit(cComponent::registerSignal("heTbResponseTriggerId"),
            static_cast<unsigned long>(event->triggerId));
    emitter->emit(cComponent::registerSignal("heTbResponseReason"),
            static_cast<long>(event->reason));
    emitter->emit(cComponent::registerSignal("heTbResponseHadPendingPayload"),
            event->hadPendingPayload ? 1L : 0L);
    emitter->emit(cComponent::registerSignal("heTbResponsePendingBytes"),
            event->pendingBytes);
    emitter->emit(cComponent::registerSignal("heTbResponseSelectedBytes"),
            event->selectedBytes);
    emitter->emit(cComponent::registerSignal("heTbResponseReportedBytes"),
            event->reportedBytes);
}

void HcfObservationSink::peerOperatingModeChanged(cComponent *emitter,
        const HePeerOperatingModeChangedEvent *event)
{
    emitter->emit(cComponent::registerSignal("peerOperatingModeChanged"), event);
    emitter->emit(cComponent::registerSignal("peerOperatingModeAssociationId"),
            static_cast<unsigned long>(event->associationId));
    emitter->emit(cComponent::registerSignal("peerOperatingModeRxNss"),
            static_cast<unsigned long>(event->newMode.rxNss));
    emitter->emit(cComponent::registerSignal("peerOperatingModeChannelWidth"),
            static_cast<unsigned long>(event->newMode.channelWidth));
    emitter->emit(cComponent::registerSignal("peerOperatingModeUlMuDisable"),
            event->newMode.ulMuDisable ? 1L : 0L);
}

void HcfObservationSink::peerInvalidated(cComponent *emitter,
        const HePeerInvalidatedEvent *event)
{
    emitter->emit(cComponent::registerSignal("hePeerInvalidated"), event);
}

} // namespace ieee80211
} // namespace inet
