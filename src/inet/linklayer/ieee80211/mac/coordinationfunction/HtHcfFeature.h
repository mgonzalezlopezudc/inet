//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HTHCFFEATURE_H
#define __INET_HTHCFFEATURE_H

#include <functional>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtMfbTransmissionState.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtSoundingPendingState.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HtSoundingRetryState.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace physicallayer {
class IIeee80211Mode;
class Ieee80211ModeSet;
}
namespace ieee80211 {

class Edcaf;
class HcfAggregationService;
class Ieee80211Mac;
class IIeee80211HtRateControl;
class IOriginatorBlockAckAgreementHandler;
class IOriginatorQoSAckPolicy;
class IQosRateSelection;
class IFrameSequence;
class OriginatorBlockAckAgreement;

/** Owns HT-specific HCF state and protocol decisions. */
class INET_API HtHcfFeature
{
  public:
    enum class AmpduAckContext {
        ORDINARY,
        IMPLICIT_BLOCK_ACK,
    };

  private:
    class ImplicitSelectionActions;

    Ieee80211Mac *mac = nullptr;
    std::function<physicallayer::Ieee80211ModeSet *()> modeSetProvider;
    IIeee80211HtRateControl *rateControl = nullptr;
    ITx *tx = nullptr;
    IQosRateSelection *rateSelection = nullptr;
    std::function<int(const MacAddress&, int,
            physicallayer::Ieee80211PhyFamily)> maxAmpduLengthExponentProvider;
    IOriginatorBlockAckAgreementHandler *originatorBlockAckAgreementHandler = nullptr;
    IOriginatorQoSAckPolicy *originatorAckPolicy = nullptr;

    HtSoundingPendingState pendingSounding;
    HtSoundingRetryState soundingRetryState;
    HtMfbTransmissionState mfbTransmissionState;
    bool soundingEnabled = false;
    int soundingNsts = 2;
    Ieee80211HtFeedbackKind soundingFeedbackKind = Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING;
    simtime_t soundingRetryInterval = SIMTIME_ZERO;

    void sendStandaloneMfb(ITx::ICallback *callback);
    int getMaxAmpduLengthExponent(const MacAddress& peer, int defaultExponent,
            physicallayer::Ieee80211PhyFamily phyFamily) const
        { return maxAmpduLengthExponentProvider(peer, defaultExponent, phyFamily); }
    physicallayer::Ieee80211ModeSet *getModeSet() const
        { return modeSetProvider ? modeSetProvider() : nullptr; }

  public:
    void configure(Ieee80211Mac *mac,
            std::function<physicallayer::Ieee80211ModeSet *()> modeSetProvider,
            IIeee80211HtRateControl *rateControl, ITx *tx,
            IQosRateSelection *rateSelection,
            std::function<int(const MacAddress&, int,
                    physicallayer::Ieee80211PhyFamily)> maxAmpduLengthExponentProvider,
            IOriginatorBlockAckAgreementHandler *originatorBlockAckAgreementHandler,
            IOriginatorQoSAckPolicy *originatorAckPolicy,
            bool soundingEnabled, int soundingNsts,
            Ieee80211HtFeedbackKind soundingFeedbackKind,
            simtime_t soundingRetryInterval);

    bool isImplicitBlockAckEnabled(bool configured) const;
    std::vector<Packet *> selectImplicitBlockAckFrames(Edcaf *edcaf,
            const HcfAggregationService& aggregationService, bool configured) const;
    static AmpduAckContext classifyAmpduAckContext(unsigned int numAggregateMembers,
            const std::vector<Ptr<const Ieee80211MacHeader>>& headers);
    static bool isImmediateBlockAckAgreement(
            const OriginatorBlockAckAgreement *agreement);

    bool processNdpAnnouncement(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header);
    static bool suppressRecipientAck(
            const Ptr<const Ieee80211MacHeader>& header);
    bool transmitNdpIfRequested(Packet *packet, simtime_t ifs,
            ITx::ICallback *callback) const;
    bool processHeaderlessNdpIndication(Packet *packet, ITx::ICallback *callback);
    void processReceivedMcsControl(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header);
    void attachPendingMcsControl(Packet *packet,
            const physicallayer::IIeee80211Mode *mode);
    bool isSoundingEligible(const MacAddress& peer,
            const physicallayer::IIeee80211Mode *mode) const;
    IFrameSequence *createSoundingSequence(const MacAddress& peer,
            const physicallayer::IIeee80211Mode *mode);
    bool processTransmissionComplete(Packet *packet, ITx::ICallback *callback);
    static bool isSoundingTransmission(const Packet *packet);
    static bool isSoundingFeedback(const Packet *packet);
    void invalidatePeer(const MacAddress& peer);

    HtSoundingPendingState::Snapshot getPendingSoundingSnapshot() const
        { return pendingSounding.getSnapshot(); }
    void setPendingMfb(const MacAddress& peer, const Ieee80211HtMcsControl& control)
        { mfbTransmissionState.setPending(peer, control); }
    void setTransmitterForCompatibility(ITx *value) { tx = value; }
    void sendStandaloneMfbForCompatibility(ITx::ICallback *callback)
        { sendStandaloneMfb(callback); }
};

} // namespace ieee80211
} // namespace inet

#endif
