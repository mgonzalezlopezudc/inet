//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtHcfFeature.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HtRateControl.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorQoSAckPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfAggregationService.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HtSoundingFs.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/queue/InProgressFrames.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

template <typename T>
Ptr<const T> findHtActionBody(const Packet *packet)
{
    if (packet == nullptr || packet->getDataLength() == b(0))
        return nullptr;
    auto data = packet->peekData();
    if (auto chunk = dynamicPtrCast<const T>(data))
        return chunk;
    if (auto sequence = dynamicPtrCast<const SequenceChunk>(data))
        for (const auto& chunk : sequence->getChunks())
            if (auto result = dynamicPtrCast<const T>(chunk))
                return result;
    return nullptr;
}

bool isImmediateHtFeedback(Ieee80211HtExplicitFeedback capability)
{
    return capability == Ieee80211HtExplicitFeedback::IMMEDIATE ||
            capability == Ieee80211HtExplicitFeedback::BOTH;
}

class HtHcfFeature::ImplicitSelectionActions final :
        public HcfAggregationService::IHtImplicitSelectionActions
{
  private:
    const HtHcfFeature *feature;
    Edcaf *edcaf;

  public:
    ImplicitSelectionActions(const HtHcfFeature *feature, Edcaf *edcaf) :
        feature(feature), edcaf(edcaf) {}

    virtual std::vector<Packet *> getCandidates(Packet *sourcePacket) const override
    {
        return edcaf->getInProgressFrames()->getEligibleFramesLike(
                sourcePacket, 64, std::numeric_limits<int>::max());
    }

    virtual long long getMaxAggregateLength(
            const Ptr<const Ieee80211DataHeader>& sourceHeader,
            Ieee80211PhyFamily phyFamily) const override
    {
        int exponent = feature->getMaxAmpduLengthExponent(
                sourceHeader->getReceiverAddress(), 3, phyFamily);
        return (1LL << (13 + exponent)) - 1;
    }

    virtual AckPolicy selectAckPolicy(Packet *candidate,
            const Ptr<const Ieee80211DataHeader>& header) const override
    {
        OriginatorBlockAckAgreement *agreement = nullptr;
        if (feature->originatorBlockAckAgreementHandler != nullptr)
            agreement = feature->originatorBlockAckAgreementHandler->getAgreement(
                    header->getReceiverAddress(), header->getTid());
        // IEEE Std 802.11-2024, 10.25.6.5 and Table 9-661: an implicit
        // BlockAck request requires an established immediate Block Ack agreement.
        if (!HtHcfFeature::isImmediateBlockAckAgreement(agreement))
            return NORMAL_ACK;
        return feature->originatorAckPolicy->computeAckPolicy(candidate, header,
                agreement);
    }
};

void HtHcfFeature::configure(Ieee80211Mac *mac,
        std::function<Ieee80211ModeSet *()> modeSetProvider,
        IIeee80211HtRateControl *rateControl, ITx *tx,
        IQosRateSelection *rateSelection,
        std::function<int(const MacAddress&, int, Ieee80211PhyFamily)>
                maxAmpduLengthExponentProvider,
        IOriginatorBlockAckAgreementHandler *originatorBlockAckAgreementHandler,
        IOriginatorQoSAckPolicy *originatorAckPolicy,
        bool soundingEnabled, int soundingNsts,
        Ieee80211HtFeedbackKind soundingFeedbackKind,
        simtime_t soundingRetryInterval)
{
    if (soundingNsts < 2 || soundingNsts > 4 ||
            static_cast<int>(soundingFeedbackKind) < 1 ||
            static_cast<int>(soundingFeedbackKind) > 3)
        throw cRuntimeError("Invalid HT sounding parameters");
    if (soundingRetryInterval < SIMTIME_ZERO)
        throw cRuntimeError("HT sounding retry interval must not be negative");
    this->mac = mac;
    this->modeSetProvider = std::move(modeSetProvider);
    this->rateControl = rateControl;
    this->tx = tx;
    this->rateSelection = rateSelection;
    if (!maxAmpduLengthExponentProvider)
        throw cRuntimeError("HT HCF maximum A-MPDU length provider is not defined");
    this->maxAmpduLengthExponentProvider = std::move(maxAmpduLengthExponentProvider);
    this->originatorBlockAckAgreementHandler = originatorBlockAckAgreementHandler;
    this->originatorAckPolicy = originatorAckPolicy;
    this->soundingEnabled = soundingEnabled;
    this->soundingNsts = soundingNsts;
    this->soundingFeedbackKind = soundingFeedbackKind;
    this->soundingRetryInterval = soundingRetryInterval;
}

bool HtHcfFeature::isImplicitBlockAckEnabled(bool configured) const
{
    auto modeSet = getModeSet();
    if (!configured || modeSet == nullptr)
        return false;
    for (int i = 0; i < modeSet->getNumModes(); i++)
        if (Ieee80211ModeSet::isHighThroughputMode(modeSet->getMode(i)))
            return true;
    return false;
}

std::vector<Packet *> HtHcfFeature::selectImplicitBlockAckFrames(Edcaf *edcaf,
        const HcfAggregationService& aggregationService, bool configured) const
{
    auto modeSet = getModeSet();
    auto sourcePacket = edcaf->getInProgressFrames()->getFrameToTransmit();
    auto sourceHeader = sourcePacket == nullptr ? nullptr :
            dynamicPtrCast<const Ieee80211DataHeader>(
                    sourcePacket->peekAtFront<Ieee80211MacHeader>());
    bool enabled = isImplicitBlockAckEnabled(configured) && sourceHeader != nullptr &&
            sourceHeader->getFragmentNumber() == 0 && !sourceHeader->getMoreFragments();
    if (!enabled)
        return {};
    auto modeReq = sourcePacket->findTag<Ieee80211ModeReq>();
    auto mode = modeReq == nullptr ? rateSelection->computeMode(sourcePacket,
            sourcePacket->peekAtFront<Ieee80211MacHeader>(),
            edcaf->getTxopProcedure()) : modeReq->getMode();
    HcfAggregationService::HtImplicitSelectionRequest request;
    request.sourcePacket = sourcePacket;
    request.mode = mode;
    request.phyFamily = modeSet->getPhyFamily(mode);
    request.enabled = enabled;
    ImplicitSelectionActions actions(this, edcaf);
    return aggregationService.selectHtImplicitBlockAckFrames(request, actions);
}

HtHcfFeature::AmpduAckContext HtHcfFeature::classifyAmpduAckContext(
        unsigned int numAggregateMembers,
        const std::vector<Ptr<const Ieee80211MacHeader>>& headers)
{
    if (numAggregateMembers == 0)
        return AmpduAckContext::ORDINARY;
    for (const auto& header : headers) {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
        if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS &&
                dataHeader->getAckPolicy() == NORMAL_ACK)
            return AmpduAckContext::IMPLICIT_BLOCK_ACK;
    }
    return AmpduAckContext::ORDINARY;
}

bool HtHcfFeature::isImmediateBlockAckAgreement(
        const OriginatorBlockAckAgreement *agreement)
{
    return agreement != nullptr && agreement->getIsAddbaResponseReceived() &&
            !agreement->getIsDelayedBlockAckPolicySupported();
}

bool HtHcfFeature::processNdpAnnouncement(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header)
{
    auto modeSet = getModeSet();
    if (header->getType() != ST_QOS_NULL || !header->getOrder() ||
            !header->getHtMcsControlPresent() || !header->getHtNdpAnnouncement())
        return false;
    pendingSounding.clear();
    auto peer = header->getTransmitterAddress();
    auto mib = mac->getMib();
    auto negotiated = mib->getNegotiatedHtCapabilities(peer);
    auto modeInd = packet->findTag<Ieee80211ModeInd>();
    auto provenance = packet->findTag<Ieee80211PhyProvenanceInd>();
    auto kindValue = header->getHtCsiSteering();
    if (!soundingEnabled || rateControl == nullptr || !negotiated ||
            !negotiated->localRxPeerTx.valid || !negotiated->localRxPeerTx.htcSupported ||
            !negotiated->localRxPeerTx.mcsRequestAllowed ||
            !negotiated->localRxPeerTx.transmitterCanSendNdp ||
            !negotiated->localRxPeerTx.receiverCanReceiveNdp ||
            !header->getHtMcsRequest() ||
            header->getHtMcsRequestSequenceIdentifier() > 6 ||
            kindValue < 1 || kindValue > 3 || modeInd == nullptr ||
            modeSet->getPhyFamily(modeInd->getMode()) != Ieee80211PhyFamily::HT ||
            provenance == nullptr || provenance->getTransmitterRadioId() < 0)
        return true;
    auto kind = static_cast<Ieee80211HtFeedbackKind>(kindValue);
    auto capability = kind == Ieee80211HtFeedbackKind::CSI ?
            negotiated->localRxPeerTx.explicitCsiFeedback :
            kind == Ieee80211HtFeedbackKind::NONCOMPRESSED_BEAMFORMING ?
            negotiated->localRxPeerTx.explicitNoncompressedFeedback :
            negotiated->localRxPeerTx.explicitCompressedFeedback;
    if (!isImmediateHtFeedback(capability))
        return true;
    HtSoundingPendingState::Snapshot pending;
    pending.valid = true;
    pending.peer = peer;
    pending.associationGeneration = mib->getHtAssociationGeneration(peer);
    pending.requestToken = header->getHtMcsRequestSequenceIdentifier();
    pending.soundingNsts = soundingNsts;
    pending.feedbackKind = kind;
    pending.channelWidth = modeInd->getMode()->getDataMode()->getBandwidth();
    pending.transmitterRadioId = provenance->getTransmitterRadioId();
    pending.announcementReceptionEnd = provenance->getEndTime();
    pendingSounding.setSnapshot(pending);
    return true;
}

bool HtHcfFeature::suppressRecipientAck(
        const Ptr<const Ieee80211MacHeader>& header)
{
    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
    return dataHeader != nullptr && dataHeader->getType() == ST_QOS_NULL &&
            dataHeader->getHtNdpAnnouncement();
}

bool HtHcfFeature::transmitNdpIfRequested(Packet *packet, simtime_t ifs,
        ITx::ICallback *callback) const
{
    auto request = packet->findTag<Ieee80211HtTransmissionReq>();
    if (request == nullptr || !request->getNdp())
        return false;
    auto header = makeShared<Ieee80211DataHeader>();
    header->setType(ST_QOS_NULL);
    header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
    header->setTransmitterAddress(mac->getAddress());
    header->setAddress3(mac->getMib()->getBssid());
    tx->transmitFrame(packet, header, ifs, callback);
    return true;
}

bool HtHcfFeature::processHeaderlessNdpIndication(Packet *packet,
        ITx::ICallback *callback)
{
    auto modeSet = getModeSet();
    auto indication = packet->findTag<Ieee80211NdpInd>();
    if (indication == nullptr || indication->getPhyFormat() != IEEE80211_NDP_PHY_HT)
        return false;
    const auto pending = pendingSounding.getSnapshot();
    const auto feedbackKind = pending.feedbackKind;
    auto provenance = packet->findTag<Ieee80211PhyProvenanceInd>();
    auto snir = packet->findTag<SnirInd>();
    auto expectedStart = pending.announcementReceptionEnd + modeSet->getSifsTime();
    bool valid = soundingEnabled && rateControl != nullptr && pending.valid &&
            provenance != nullptr && snir != nullptr && indication->getSounding() &&
            provenance->getTransmitterRadioId() == pending.transmitterRadioId &&
            std::abs((provenance->getStartTime() - expectedStart).raw()) <=
                    modeSet->getSlotTime().raw() &&
            Hz(indication->getChannelWidth()) == pending.channelWidth &&
            indication->getNumberOfSpaceTimeStreams() == pending.soundingNsts &&
            indication->getNumberOfLtfSymbols() ==
                    (pending.soundingNsts == 3 ? 4 : pending.soundingNsts) &&
            std::isfinite(snir->getMinimumSnir()) && snir->getMinimumSnir() > 0 &&
            std::isfinite(snir->getAverageSnir()) && snir->getAverageSnir() > 0;
    auto negotiated = valid ? mac->getMib()->getNegotiatedHtCapabilities(pending.peer) :
            std::optional<Ieee80211NegotiatedHtCapabilities>();
    valid &= negotiated.has_value() &&
            mac->getMib()->getHtAssociationGeneration(pending.peer) ==
                    pending.associationGeneration;
    if (valid) {
        int maxPerStreamMcs = negotiated->localRxPeerTx.mcsNss.maxMcsPerNss[
                pending.soundingNsts - 1];
        valid = maxPerStreamMcs >= 0;
        if (valid) {
            double snirDb = 10 * std::log10(snir->getMinimumSnir());
            int perStreamMcs = std::clamp(static_cast<int>((snirDb - 4) / 3),
                    0, maxPerStreamMcs);
            uint8_t recommendedMcs = 8 * (pending.soundingNsts - 1) + perStreamMcs;
            auto measurement = HtCsiCache::deriveMeasurement(
                    snir->getMinimumSnir(), snir->getAverageSnir(), recommendedMcs,
                    pending.soundingNsts, indication->getNumberOfLtfSymbols(), feedbackKind);
            measurement.reportBytes[0] = std::clamp<int>(
                    std::lround((snirDb + 20) * 4), 0, 255);
            if (measurement.reportBytes.size() > 1)
                measurement.reportBytes[1] = recommendedMcs;
            rateControl->getHtCsiCache().update(pending.peer, pending.channelWidth,
                    pending.associationGeneration, pending.soundingNsts,
                    pending.requestToken, measurement);
            auto requestMode = modeSet->findHtMode(0, pending.soundingNsts,
                    pending.channelWidth, false);
            Ieee80211HtMcsControl mfbControl;
            if (requestMode != nullptr) {
                rateControl->processReceivedHtMcsRequest(pending.peer,
                        pending.requestToken, requestMode);
                if (rateControl->getPendingHtMcsControl(pending.peer, false, true,
                        mfbControl) && mfbControl.mcsFeedbackSequenceIdentifier < 7)
                    mfbTransmissionState.setPending(pending.peer, mfbControl);
            }
            if (mfbTransmissionState.getPending().peer.isUnspecified()) {
                mfbControl.mcsFeedbackSequenceIdentifier = pending.requestToken;
                mfbControl.mcsFeedback = recommendedMcs;
                mfbTransmissionState.setPending(pending.peer, mfbControl);
            }

            Ptr<Ieee80211HtMimoFeedback> feedback;
            if (feedbackKind == Ieee80211HtFeedbackKind::CSI)
                feedback = makeShared<Ieee80211HtCsiFeedback>();
            else if (feedbackKind == Ieee80211HtFeedbackKind::NONCOMPRESSED_BEAMFORMING)
                feedback = makeShared<Ieee80211HtNoncompressedBeamformingFeedback>();
            else
                feedback = makeShared<Ieee80211HtCompressedBeamformingFeedback>();
            feedback->setNc(1);
            feedback->setNr(pending.soundingNsts);
            feedback->setChannelWidth(pending.channelWidth.get());
            feedback->setGrouping(1);
            feedback->setCoefficientSize(
                    feedbackKind == Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING ? 0 : 4);
            feedback->setCodebookInformation(0);
            feedback->setRemainingMatrixSegments(0);
            feedback->setSoundingTimestamp(static_cast<uint32_t>(
                    simTime().inUnit(SIMTIME_US)));
            feedback->setReportArraySize(measurement.reportBytes.size());
            for (size_t i = 0; i < measurement.reportBytes.size(); i++)
                feedback->setReport(i, measurement.reportBytes[i]);
            feedback->setChunkLength(B(8 + measurement.reportBytes.size()));
            auto header = makeShared<Ieee80211ActionFrame>();
            header->setType(ST_NOACKACTION);
            header->setCategory(7);
            header->setReceiverAddress(pending.peer);
            header->setTransmitterAddress(mac->getAddress());
            header->setAddress3(mac->getMib()->getBssid());
            auto response = new Packet("HT-MIMO-Feedback", header);
            response->insertAtBack(feedback);
            response->insertAtBack(makeShared<Ieee80211MacTrailer>());
            response->addTag<Ieee80211ModeReq>()->setMode(
                    modeSet->getSlowestMandatoryMode(MHz(20)));
            // IEEE Std 802.11-2024, 9.6.11 and 10.33.
            tx->transmitFrame(response, header, modeSet->getSifsTime(), callback);
            delete response;
        }
    }
    pendingSounding.clear();
    delete packet;
    return true;
}

void HtHcfFeature::processReceivedMcsControl(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header)
{
    processNdpAnnouncement(packet, header);
    if (rateControl == nullptr || !header->getOrder() ||
            !header->getHtMcsControlPresent())
        return;
    auto peer = header->getTransmitterAddress();
    auto receivedMode = packet->findTag<Ieee80211ModeInd>();
    if (header->getHtMcsRequest())
        rateControl->processReceivedHtMcsRequest(peer,
                header->getHtMcsRequestSequenceIdentifier(),
                receivedMode == nullptr ? nullptr : receivedMode->getMode());
    if (header->getHtMcsFeedbackSequenceIdentifier() < 7 &&
            header->getHtMcsFeedback() <= 127)
        rateControl->processReceivedHtMcsFeedback(peer,
                header->getHtMcsFeedbackSequenceIdentifier(),
                header->getHtMcsFeedback());
}

void HtHcfFeature::attachPendingMcsControl(Packet *packet,
        const IIeee80211Mode *mode)
{
    auto modeSet = getModeSet();
    if (rateControl == nullptr || mode == nullptr ||
            modeSet->getPhyFamily(mode) != Ieee80211PhyFamily::HT)
        return;
    auto header = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront());
    if (header == nullptr || (header->getType() != ST_DATA_WITH_QOS &&
            header->getType() != ST_QOS_NULL) || header->getHtMcsControlPresent() ||
            header->getOperatingModePresent() || header->getBufferStatusPresent())
        return;
    auto negotiated = mac->getMib()->getNegotiatedHtCapabilities(
            header->getReceiverAddress());
    if (!negotiated || !negotiated->localTxPeerRx.valid ||
            !negotiated->localTxPeerRx.htcSupported)
        return;
    Ieee80211HtMcsControl control;
    if (!rateControl->getPendingHtMcsControl(header->getReceiverAddress(),
            negotiated->localTxPeerRx.mcsRequestAllowed,
            negotiated->localTxPeerRx.mcsFeedbackAllowed, control))
        return;
    auto mutableHeader = packet->removeAtFront<Ieee80211DataHeader>();
    mutableHeader->setOrder(true);
    mutableHeader->setHtMcsControlPresent(true);
    mutableHeader->setHtTrainingRequest(control.trainingRequest);
    mutableHeader->setHtMcsRequest(control.mcsRequest);
    mutableHeader->setHtMcsRequestSequenceIdentifier(control.mcsRequestSequenceIdentifier);
    mutableHeader->setHtMcsFeedbackSequenceIdentifier(control.mcsFeedbackSequenceIdentifier);
    mutableHeader->setHtMcsFeedback(control.mcsFeedback);
    mutableHeader->setHtCsiSteering(control.csiSteering);
    mutableHeader->setHtNdpAnnouncement(control.ndpAnnouncement);
    mutableHeader->setChunkLength(mutableHeader->getChunkLength() + B(4));
    packet->insertAtFront(mutableHeader);
}

bool HtHcfFeature::isSoundingEligible(const MacAddress& peer,
        const IIeee80211Mode *mode) const
{
    auto modeSet = getModeSet();
    if (!soundingEnabled || rateControl == nullptr || mode == nullptr ||
            peer.isMulticast() || modeSet->getPhyFamily(mode) != Ieee80211PhyFamily::HT)
        return false;
    if (!soundingRetryState.isAttemptAllowed(peer, simTime()))
        return false;
    auto mib = mac->getMib();
    auto negotiated = mib->getNegotiatedHtCapabilities(peer);
    if (!negotiated || !negotiated->localTxPeerRx.valid ||
            mib->getHtAssociationGeneration(peer) == 0 ||
            !negotiated->localTxPeerRx.htcSupported ||
            !negotiated->localTxPeerRx.mcsRequestAllowed ||
            !negotiated->localTxPeerRx.transmitterCanSendNdp ||
            !negotiated->localTxPeerRx.receiverCanReceiveNdp ||
            negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[soundingNsts - 1] < 0)
        return false;
    auto capability = soundingFeedbackKind == Ieee80211HtFeedbackKind::CSI ?
            negotiated->localTxPeerRx.explicitCsiFeedback :
            soundingFeedbackKind == Ieee80211HtFeedbackKind::NONCOMPRESSED_BEAMFORMING ?
            negotiated->localTxPeerRx.explicitNoncompressedFeedback :
            negotiated->localTxPeerRx.explicitCompressedFeedback;
    if (!isImmediateHtFeedback(capability))
        return false;
    return true;
}

IFrameSequence *HtHcfFeature::createSoundingSequence(const MacAddress& peer,
        const IIeee80211Mode *mode)
{
    if (!isSoundingEligible(peer, mode))
        return nullptr;
    auto modeSet = getModeSet();
    auto mib = mac->getMib();
    auto generation = mib->getHtAssociationGeneration(peer);
    auto token = soundingRetryState.recordAttempt(peer, simTime(), soundingRetryInterval);
    auto ndpMode = modeSet->getHtNdpMode(mode, soundingNsts);
    return new HtSoundingFs(mib, &rateControl->getHtCsiCache(), peer, generation,
            token, soundingNsts, soundingFeedbackKind, modeSet, ndpMode);
}

void HtHcfFeature::sendStandaloneMfb(ITx::ICallback *callback)
{
    auto modeSet = getModeSet();
    const auto pending = mfbTransmissionState.getPending();
    if (pending.peer.isUnspecified() ||
            pending.control.mcsFeedbackSequenceIdentifier >= 7)
        return;
    auto header = makeShared<Ieee80211DataHeader>();
    header->setType(ST_QOS_NULL);
    header->setReceiverAddress(pending.peer);
    header->setTransmitterAddress(mac->getAddress());
    header->setAddress3(mac->getMib()->getBssid());
    header->setAckPolicy(NO_ACK);
    header->setOrder(true);
    header->setHtMcsControlPresent(true);
    header->setHtMcsFeedbackSequenceIdentifier(
            pending.control.mcsFeedbackSequenceIdentifier);
    header->setHtMcsFeedback(pending.control.mcsFeedback);
    header->setChunkLength(B(30));
    auto packet = new Packet("HT-MFB", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    packet->addTag<Ieee80211ModeReq>()->setMode(
            modeSet->getSlowestMandatoryMode(MHz(20)));
    mfbTransmissionState.startStandaloneTransmission();
    tx->transmitFrame(packet, header, modeSet->getSifsTime(), callback);
    delete packet;
    mfbTransmissionState.clearPending();
}

bool HtHcfFeature::processTransmissionComplete(Packet *packet,
        ITx::ICallback *callback)
{
    if (isSoundingFeedback(packet)) {
        sendStandaloneMfb(callback);
        return true;
    }
    return mfbTransmissionState.completeStandaloneTransmission();
}

bool HtHcfFeature::isSoundingTransmission(const Packet *packet)
{
    if (packet == nullptr)
        return false;
    if (packet->findTag<Ieee80211HtTransmissionReq>() != nullptr)
        return true;
    if (packet->getDataLength() == b(0))
        return false;
    auto header = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront());
    return header != nullptr && header->getHtNdpAnnouncement();
}

bool HtHcfFeature::isSoundingFeedback(const Packet *packet)
{
    return findHtActionBody<Ieee80211HtMimoFeedback>(packet) != nullptr;
}

void HtHcfFeature::invalidatePeer(const MacAddress& peer)
{
    soundingRetryState.invalidate(peer);
    pendingSounding.invalidate(peer);
    mfbTransmissionState.invalidate(peer);
    if (rateControl != nullptr)
        rateControl->invalidateHtPeer(peer);
}

} // namespace ieee80211
} // namespace inet
