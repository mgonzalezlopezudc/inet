//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Addressing.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/contract/DurationFinalizedReq.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HtRateControl.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Hcca.h"

#include <algorithm>
#include <cstring>

#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211MgmtExchangeTag_m.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211FcsChecker.h"
#include "inet/linklayer/ieee80211/twt/ITwtManager.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/Ieee80211HeMuContainerTag_m.h"
#include "inet/linklayer/ieee80211/mac/framesequence/TxOpFs.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/QosRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/protectionmechanism/SingleProtectionMechanism.h"
#include "inet/linklayer/ieee80211/mac/protectionmechanism/HtProtectionPolicy.h"
#include "inet/linklayer/ieee80211/mac/recipient/CtsProcedure.h"
#include "inet/linklayer/ieee80211/mac/queue/InProgressFrames.h"
#include "inet/linklayer/ieee80211/mac/queue/OrigEnqueueTimeTag_m.h"
#include "inet/linklayer/ieee80211/mac/recipient/RecipientAckProcedure.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211FecCodingReq.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

class HcfVhtRuntime final : public VhtHcfFeature::IActions
{
  private:
    Hcf *hcf;
    VhtHcfFeature feature;
    mutable bool continuingFrameSequence = false;
    mutable bool continuingRecipientFrame = false;
    mutable bool continuingSetFrameMode = false;
    mutable bool continuingTransmitFrame = false;
    mutable bool continuingTransmittedFrame = false;
    mutable bool continuingReceivedFrame = false;
    mutable bool continuingTransmissionComplete = false;

    class ContinuationGuard
    {
      private:
        bool& flag;
      public:
        explicit ContinuationGuard(bool& flag) : flag(flag) { flag = true; }
        ~ContinuationGuard() { flag = false; }
    };

  public:
    explicit HcfVhtRuntime(Hcf *hcf) : hcf(hcf) {}

    void initialize()
    {
        bool enableSu = hcf->par("enableVhtSuBeamforming");
        bool enableMu = hcf->par("enableVhtDlMuMimo");
        auto groupId = hcf->par("vhtDlMuGroupId").intValue();
        if (groupId < 1 || groupId > 62)
            throw cRuntimeError("vhtDlMuGroupId must be in the range 1..62");
        IIeee80211VhtPacketRadio *radio = nullptr;
        if (enableSu || enableMu) {
            auto radioModule = getContainingNicModule(hcf)->getSubmodule("radio");
            VhtHcfFeature::validatePacketLevelRadio(radioModule);
            radio = check_and_cast<IIeee80211VhtPacketRadio *>(radioModule);
        }
        feature.configure(this, enableSu, enableMu, groupId,
                hcf->par("beamformingGain"), hcf->par("vhtCsiValidityDuration"),
                check_and_cast<IVhtSoundingCoordinator *>(hcf->getSubmodule("soundingCoordinator")),
                check_and_cast<IVhtGroupIdManager *>(hcf->getSubmodule("groupIdManager")),
                check_and_cast<IIeee80211VhtDlMuScheduler *>(hcf->getSubmodule("dlMuScheduler")),
                radio);
    }

    bool isContinuingFrameSequence() const { return continuingFrameSequence; }
    bool isContinuingRecipientFrame() const { return continuingRecipientFrame; }
    bool isContinuingSetFrameMode() const { return continuingSetFrameMode; }
    bool isContinuingTransmitFrame() const { return continuingTransmitFrame; }
    bool isContinuingTransmittedFrame() const { return continuingTransmittedFrame; }
    bool isContinuingReceivedFrame() const { return continuingReceivedFrame; }
    bool isContinuingTransmissionComplete() const { return continuingTransmissionComplete; }
    void modeSetChanged() { feature.modeSetChanged(); }
    void setTxOpFactoryForTesting(VhtHcfFeature::ITxOpFactory *factory)
        { feature.setTxOpFactoryForTesting(factory); }
    void invalidatePeer(const MacAddress& peer) { feature.invalidatePeer(peer); }
    void startFrameSequence(AccessCategory ac)
    {
        if (!hcf->hasFrameToTransmit(ac)) {
            hcf->releaseChannel(ac);
            return;
        }
        auto snapshot = feature.prepareGrantSnapshot(ac);
        switch (snapshot.startKind) {
            case VhtGrantSnapshot::StartKind::COMMON_SINGLE_USER:
                hcf->startSingleUserExchange(ac);
                return;
            case VhtGrantSnapshot::StartKind::SU_SOUNDING:
            case VhtGrantSnapshot::StartKind::MU_SOUNDING:
                feature.startSounding(snapshot);
                return;
            case VhtGrantSnapshot::StartKind::GROUP_MANAGEMENT:
            case VhtGrantSnapshot::StartKind::BLOCK_ACK_PREREQUISITE:
            case VhtGrantSnapshot::StartKind::DL_MULTIUSER:
                feature.commitPreparedGrant(snapshot);
                return;
        }
        throw cRuntimeError("Unknown VHT grant start kind");
    }

    virtual Ieee80211Mac *getMac() const override { return hcf->mac; }
    virtual Ieee80211ModeSet *getModeSet() const override { return hcf->modeSet; }
    virtual Edca *getEdca() const override { return hcf->edca; }
    virtual IQosRateSelection *getRateSelection() const override { return hcf->rateSelection; }
    virtual IOriginatorBlockAckAgreementHandler *getBlockAckHandler() const override
        { return hcf->originatorBlockAckAgreementHandler.get(); }
    virtual IOriginatorBlockAckAgreementPolicy *getBlockAckPolicy() const override
        { return hcf->originatorBlockAckAgreementPolicy; }
    virtual ITx *getTx() const override { return hcf->tx; }
    virtual ITx::ICallback *getTxCallback() override { return hcf; }
    virtual IOriginatorMacDataService *getOriginatorDataService() const override
        { return hcf->originatorDataService; }
    virtual const IFrameSequence *getActiveFrameSequence() const override
        { return hcf->getFrameSequenceForLegacyAdapter(); }
    virtual IFrameSequenceHandler::ICallback *getFrameSequenceCallback() const override
        { return hcf->getFrameSequenceCallbackForLegacyAdapter(); }
    virtual FrameSequenceContext *buildFrameSequenceContext(AccessCategory ac) override
        { return hcf->buildContext(ac); }
    virtual void startFeatureFrameSequence(IFrameSequence *sequence,
            AccessCategory ac) override
        { hcf->startExchangeFrameSequence(sequence, hcf->buildContext(ac)); }
    virtual void continueBaseFrameSequence(AccessCategory ac) override
    {
        ContinuationGuard guard(continuingFrameSequence);
        hcf->startFrameSequence(ac);
    }
    virtual void continueBaseRecipientFrame(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header) override
    {
        ContinuationGuard guard(continuingRecipientFrame);
        hcf->recipientProcessReceivedFrame(packet, header);
    }
    virtual void continueBaseSetFrameMode(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header,
            const IIeee80211Mode *mode) const override
    {
        ContinuationGuard guard(continuingSetFrameMode);
        hcf->setFrameMode(packet, header, mode);
    }
    virtual void continueBaseTransmitFrame(Packet *packet, simtime_t ifs) override
    {
        ContinuationGuard guard(continuingTransmitFrame);
        hcf->transmitFrame(packet, ifs);
    }
    virtual void continueBaseTransmittedFrame(Packet *packet) override
    {
        ContinuationGuard guard(continuingTransmittedFrame);
        hcf->originatorProcessTransmittedFrame(packet);
    }
    virtual void continueBaseReceivedFrame(Packet *packet,
            Packet *lastTransmittedPacket) override
    {
        ContinuationGuard guard(continuingReceivedFrame);
        hcf->originatorProcessReceivedFrame(packet, lastTransmittedPacket);
    }
    virtual void continueBaseTransmissionComplete(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header) override
    {
        ContinuationGuard guard(continuingTransmissionComplete);
        hcf->transmissionComplete(packet, header);
    }
    virtual void processTransmittedDataFrame(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header, AccessCategory ac) override
        { hcf->originatorProcessTransmittedDataFrame(packet, header, ac); }
    virtual void processFailedFrame(Packet *packet) override
        { hcf->originatorProcessFailedFrame(packet); }
    virtual void reclaimPacketOwnership(Packet *packet) override { hcf->take(packet); }

    bool processHeaderlessNdpIndication(Packet *packet)
        { return feature.processHeaderlessNdpIndication(packet); }
    void recipientProcessReceivedFrame(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header)
        { feature.recipientProcessReceivedFrame(packet, header); }
    void setFrameMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header,
            const IIeee80211Mode *mode) const
        { feature.setFrameMode(packet, header, mode); }
    void transmitFrame(Packet *packet, simtime_t ifs) { feature.transmitFrame(packet, ifs); }
    void originatorProcessTransmittedFrame(Packet *packet)
        { feature.originatorProcessTransmittedFrame(packet); }
    void originatorProcessReceivedFrame(Packet *packet, Packet *lastTransmittedPacket)
        { feature.originatorProcessReceivedFrame(packet, lastTransmittedPacket); }
    void transmissionComplete(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header)
        { feature.transmissionComplete(packet, header); }
};

class HcfRecipientActions final : public HcfRecipientService::IActions
{
  private:
    Hcf *hcf;
    bool wasHeMu = false;

  public:
    explicit HcfRecipientActions(Hcf *hcf) : hcf(hcf) {}

    virtual HcfRecipientService::AddressDisposition classifyAddress(const Packet *,
            const Ptr<const Ieee80211MacHeader>& header) override
    {
        if (!hcf->isForUs(header))
            return HcfRecipientService::AddressDisposition::FOREIGN;
        auto roles = interpretIeee80211AddressRoles(header);
        return roles.receiverAddress.isMulticast() ?
                HcfRecipientService::AddressDisposition::LOCAL_GROUP :
                HcfRecipientService::AddressDisposition::LOCAL_UNICAST;
    }

    virtual void packetReceived(const Packet *packet,
            const Ptr<const Ieee80211MacHeader>&) noexcept override
    {
        EV_INFO << "Processing received frame " << packet->getName()
                << " as recipient.\n";
        hcf->emit(cComponent::registerSignal("packetReceivedFromPeer"), packet);
    }

    virtual void packetDropped(const Packet *packet,
            HcfRecipientService::DropReason reason) noexcept override
    {
        PacketDropDetails details;
        details.setReason(reason == HcfRecipientService::DropReason::NOT_ADDRESSED_TO_US ?
                NOT_ADDRESSED_TO_US :
                reason == HcfRecipientService::DropReason::INCORRECTLY_RECEIVED ?
                INCORRECTLY_RECEIVED : OTHER_PACKET_DROP);
        hcf->emit(cComponent::registerSignal("packetDropped"), packet, &details);
    }

    virtual void processImmediateResponse(Packet *packet,
            const Ptr<const Ieee80211DataOrMgmtHeader>& header) override
    {
        wasHeMu = false;
        int allocationIndex = -1;
        if (auto indication = packet->findTag<Ieee80211HeRxVectorInd>();
                indication != nullptr && indication->getRxVector() != nullptr &&
                indication->getRxVector()->getCommon().getPpduFormat() == HE_MU_DOWNLINK) {
            wasHeMu = true;
            allocationIndex = indication->getRxVector()->getUser().getStaId().has_value() ? 0 : -1;
        }

        if (wasHeMu && allocationIndex != -1) {
            bool isBlockAckPolicyData = false;
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
                isBlockAckPolicyData = dataHeader->getType() == ST_DATA_WITH_QOS &&
                        dataHeader->getAckPolicy() == BLOCK_ACK;
                if (isBlockAckPolicyData && hcf->recipientBlockAckAgreementHandler) {
                    auto agreement = hcf->recipientBlockAckAgreementHandler->getAgreement(
                            dataHeader->getTid(), dataHeader->getTransmitterAddress());
                    if (agreement)
                        hcf->recipientBlockAckAgreementHandler->qosFrameReceived(dataHeader, hcf);
                    if (agreement)
                        EV_INFO << "HeHcf: STA waits for an explicit BAR or MU-BAR Trigger before BlockAck: index = "
                                << allocationIndex << endl;
                }
            }

            if (!isBlockAckPolicyData && hcf->recipientAckPolicy->isAckNeeded(header)) {
                auto ack = makeShared<Ieee80211AckFrame>();
                ack->setReceiverAddress(header->getTransmitterAddress());
                auto dummyRequest = makeShared<Ieee80211BasicBlockAckReq>();
                auto responseMode = hcf->rateSelection->computeResponseBlockAckFrameMode(
                        packet, dummyRequest);
                simtime_t blockAckDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
                simtime_t ifs = (allocationIndex + 1) * hcf->modeSet->getSifsTime() +
                        allocationIndex * blockAckDuration;
                auto ackMode = hcf->rateSelection->computeResponseAckFrameMode(packet, header);
                simtime_t ackDuration = ackMode->getDuration(LENGTH_ACK);
                simtime_t duration = header->getDurationField() - ifs - ackDuration;
                if (duration < 0)
                    duration = 0;
                ack->setDurationField(duration);
                auto ackPacket = new Packet("WlanAck", ack);
                ackPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
                hcf->setFrameMode(ackPacket, ack, ackMode);
                EV_INFO << "HeHcf: STA sequential Ack scheduled: index = "
                        << allocationIndex << ", delay = " << ifs
                        << ", duration = " << ackDuration << endl;
                hcf->tx->transmitFrame(ackPacket, ack, ifs, hcf);
                delete ackPacket;
            }
        }
        else {
            if (!HtHcfFeature::suppressRecipientAck(header))
                hcf->recipientAckProcedure->processReceivedFrame(
                        packet, header,
                        check_and_cast<IRecipientAckPolicy *>(hcf->recipientAckPolicy), hcf);
        }
    }

    virtual bool processHtImplicitBlockAckResponse(Packet *aggregate,
            const std::vector<Ptr<const Ieee80211DataHeader>>& headers) override
    {
        bool sent = hcf->recipientBlockAckProcedure != nullptr &&
                hcf->recipientBlockAckAgreementHandler != nullptr &&
                hcf->recipientBlockAckProcedure->processReceivedHtImplicitBlockAckRequest(
                        aggregate, headers, hcf->recipientBlockAckAgreementHandler.get(),
                        hcf, hcf);
        if (!sent)
            EV_WARN << "Not sending a response for an invalid HT implicit-BlockAck A-MPDU.\n";
        return sent;
    }

    virtual void processOrdinaryAggregateMember(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header) override
    {
        hcf->recipientProcessReceivedFrame(packet, header);
    }

    virtual void deliverData(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header,
            bool implicitBlockAckMember) noexcept override
    {
        if (!implicitBlockAckMember && header->getType() == ST_DATA_WITH_QOS &&
                hcf->recipientBlockAckAgreementHandler && !wasHeMu)
            hcf->recipientBlockAckAgreementHandler->qosFrameReceived(header, hcf);
        hcf->sendUp(hcf->recipientDataService->dataFrameReceived(packet, header,
                hcf->recipientBlockAckAgreementHandler.get()));
    }

    virtual void deliverManagement(Packet *packet,
            const Ptr<const Ieee80211MgmtHeader>& header) noexcept override
    {
        hcf->sendUp(hcf->recipientDataService->managementFrameReceived(packet, header));
        hcf->recipientProcessReceivedManagementFrame(header);
    }

    virtual void processControl(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header) override
    {
        hcf->sendUp(hcf->recipientDataService->controlFrameReceived(packet, header,
                hcf->recipientBlockAckAgreementHandler.get()));
        hcf->recipientProcessReceivedControlFrame(packet, header);
    }

    virtual void deletePacket(Packet *packet) noexcept override
    {
        delete packet;
    }
};

class HcfRecipientFrameDispatchActions final :
        public HcfFrameDispatchService::IRecipientActions,
        public HcfFrameDispatchService::IResponseActions
{
  private:
    Hcf *hcf;

  public:
    explicit HcfRecipientFrameDispatchActions(Hcf *hcf) : hcf(hcf) {}

    virtual void recipientPsPoll(const Ptr<const Ieee80211PsPollFrame>& frame) override
    {
        if (auto twtManager = hcf->mac->getTwtManager())
            twtManager->notifyPeerAwake(frame->getTransmitterAddress());
    }

    virtual void recipientRts(Packet *packet,
            const Ptr<const Ieee80211RtsFrame>& frame) override
    {
        hcf->ctsProcedure->processReceivedRts(packet, frame, hcf->ctsPolicy, hcf);
    }

    virtual HcfFrameDispatchService::MultiTidResponseContext getMultiTidResponseContext(
            const Ptr<const Ieee80211MultiTidBlockAckReq>& frame) override
    {
        HcfFrameDispatchService::MultiTidResponseContext context;
        auto mib = hcf->mac->getMib();
        auto negotiated = mib->getNegotiatedHeCapabilities(frame->getTransmitterAddress());
        context.validResponseAid = mib->getStationType() == Ieee80211Mib::STATION;
        if (mib->getStationType() == Ieee80211Mib::ACCESS_POINT) {
            auto associationId = mib->getAssociationId(frame->getTransmitterAddress());
            context.validResponseAid = associationId > 0 && associationId <= 2007;
            if (context.validResponseAid)
                context.responseAid = associationId;
        }
        context.heMultiTidAggregation = negotiated &&
                negotiated->localRxPeerTx.valid &&
                negotiated->localRxPeerTx.multiTidAggregation;
        context.legacyMultiTidBlockAck = hcf->isLegacyHtMultiTidBlockAckEnabled();
        return context;
    }

    virtual void recipientBlockAckRequest(Packet *packet,
            const Ptr<const Ieee80211BlockAckReq>& frame,
            MultiTidBlockAckResponseFormat responseFormat,
            uint16_t responseAid) override
    {
        if (hcf->recipientBlockAckProcedure)
            hcf->recipientBlockAckProcedure->processReceivedBlockAckReq(packet,
                    frame, hcf->recipientAckPolicy,
                    hcf->recipientBlockAckAgreementHandler.get(), hcf,
                    responseFormat, responseAid);
    }

    virtual void recipientStaleAck(const Ptr<const Ieee80211AckFrame>&) override
    {
        EV_WARN << "ACK frame received after timeout, ignoring it.\n";
    }

    virtual const IIeee80211Mode *selectCtsResponseMode(Packet *packet,
            const Ptr<const Ieee80211RtsFrame>& frame) override
    {
        return hcf->rateSelection->computeResponseCtsFrameMode(packet, frame);
    }

    virtual const IIeee80211Mode *selectBlockAckResponseMode(Packet *packet,
            const Ptr<const Ieee80211BlockAckReq>& frame) override
    {
        return hcf->rateSelection->computeResponseBlockAckFrameMode(packet, frame);
    }

    virtual const IIeee80211Mode *selectImplicitBlockAckResponseMode(
            Packet *responsePacket, Packet *receivedPacket,
            const Ptr<const Ieee80211BlockAck>&,
            const Ptr<const Ieee80211DataHeader>& receivedFrame) override
    {
        auto request = makeShared<Ieee80211CompressedBlockAckReq>();
        request->setReceiverAddress(receivedFrame->getTransmitterAddress());
        request->setTidInfo(receivedFrame->getTid());
        request->setStartingSequenceNumber(receivedFrame->getSequenceNumber());
        auto mode = hcf->rateSelection->computeResponseBlockAckFrameMode(
                receivedPacket, request);
        auto mutableBlockAck = responsePacket->removeAtFront<Ieee80211BlockAck>();
        auto duration = receivedFrame->getDurationField() -
                hcf->modeSet->getSifsTime() -
                mode->getDuration(mutableBlockAck->getChunkLength());
        mutableBlockAck->setDurationField(
                duration < SIMTIME_ZERO ? SIMTIME_ZERO : duration);
        responsePacket->insertAtFront(mutableBlockAck);
        return mode;
    }

    virtual const IIeee80211Mode *selectAckResponseMode(Packet *packet,
            const Ptr<const Ieee80211DataOrMgmtHeader>& frame) override
    {
        return hcf->rateSelection->computeResponseAckFrameMode(packet, frame);
    }

    virtual void transmittedCtsResponse(
            const Ptr<const Ieee80211CtsFrame>& frame) override
    {
        hcf->ctsProcedure->processTransmittedCts(frame);
    }

    virtual void transmittedBlockAckResponse(
            const Ptr<const Ieee80211BlockAck>& frame) override
    {
        if (hcf->recipientBlockAckProcedure)
            hcf->recipientBlockAckProcedure->processTransmittedBlockAck(frame);
    }

    virtual void transmittedAckResponse(
            const Ptr<const Ieee80211AckFrame>& frame) override
    {
        hcf->recipientAckProcedure->processTransmittedAck(frame);
    }

    virtual void recipientAddbaRequest(
            const Ptr<const Ieee80211AddbaRequest>& frame) override
    {
        if (!hcf->recipientBlockAckAgreementHandler ||
                !hcf->originatorBlockAckAgreementHandler)
            return;
        hcf->recipientBlockAckAgreementHandler->processReceivedAddbaRequest(frame,
                hcf->recipientBlockAckAgreementPolicy, hcf);
        auto agreement = hcf->recipientBlockAckAgreementHandler->getAgreement(
                frame->getTid(), frame->getTransmitterAddress());
        hcf->emit(cComponent::registerSignal("blockAckAgreementAdded"), agreement);
    }

    virtual void recipientAddbaResponse(
            const Ptr<const Ieee80211AddbaResponse>& frame) override
    {
        if (!hcf->recipientBlockAckAgreementHandler ||
                !hcf->originatorBlockAckAgreementHandler)
            return;
        hcf->originatorBlockAckAgreementHandler->processReceivedAddbaResp(frame,
                hcf->originatorBlockAckAgreementPolicy, hcf);
        auto agreement = hcf->originatorBlockAckAgreementHandler->getAgreement(
                frame->getTransmitterAddress(), frame->getTid());
        hcf->emit(cComponent::registerSignal("blockAckAgreementAdded"), agreement);
        hcf->resumeContention();
    }

    virtual void recipientDelba(const Ptr<const Ieee80211Delba>& frame) override
    {
        if (!hcf->recipientBlockAckAgreementHandler ||
                !hcf->originatorBlockAckAgreementHandler)
            return;
        if (frame->getInitiator()) {
            auto agreement = hcf->recipientBlockAckAgreementHandler->getAgreement(
                    frame->getTid(), frame->getReceiverAddress());
            hcf->emit(cComponent::registerSignal("blockAckAgreementDeleted"), agreement);
            hcf->recipientBlockAckAgreementHandler->processReceivedDelba(frame,
                    hcf->recipientBlockAckAgreementPolicy);
        }
        else {
            auto agreement = hcf->originatorBlockAckAgreementHandler->getAgreement(
                    frame->getReceiverAddress(), frame->getTid());
            hcf->emit(cComponent::registerSignal("blockAckAgreementDeleted"), agreement);
            hcf->originatorBlockAckAgreementHandler->processReceivedDelba(frame,
                    hcf->originatorBlockAckAgreementPolicy);
        }
    }
};

class HcfOriginatorFrameDispatchActions final :
        public HcfFrameDispatchService::IOriginatorTransmitActions,
        public HcfFrameDispatchService::IOriginatorReceiveActions
{
  private:
    Hcf *hcf;
    Edcaf *edcaf;
    AccessCategory accessCategory;

  public:
    HcfOriginatorFrameDispatchActions(Hcf *hcf, Edcaf *edcaf,
            AccessCategory accessCategory) :
        hcf(hcf), edcaf(edcaf), accessCategory(accessCategory)
    {
        if (edcaf == nullptr)
            throw cRuntimeError("HCF originator frame dispatch requires an EDCAF");
    }

    virtual void transmittedGroup(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header) override
    {
        edcaf->getRecoveryProcedure()->multicastFrameTransmitted();
        if (dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header))
            edcaf->getInProgressFrames()->dropFrame(packet);
    }

    virtual void transmittedData(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header,
            HcfFrameDispatchService::ExpectedResponse expectedResponse) override
    {
        auto response = expectedResponse == HcfFrameDispatchService::ExpectedResponse::ACK ?
                HcfOriginatorService::ExpectedResponse::ACK :
                expectedResponse == HcfFrameDispatchService::ExpectedResponse::BLOCK_ACK ?
                HcfOriginatorService::ExpectedResponse::BLOCK_ACK :
                HcfOriginatorService::ExpectedResponse::NONE;
        hcf->processDispatchedTransmittedData(packet, header, response, edcaf,
                accessCategory);
    }

    virtual bool isManagementAckNeeded(
            const Ptr<const Ieee80211MgmtHeader>& frame) override
    {
        return hcf->originatorAckPolicy->isAckNeeded(frame);
    }

    virtual void transmittedManagementAck(
            const Ptr<const Ieee80211MgmtHeader>& frame) override
    {
        edcaf->getAckHandler()->processTransmittedDataOrMgmtFrame(frame);
    }

    virtual void transmittedAddbaRequest(
            const Ptr<const Ieee80211AddbaRequest>& frame) override
    {
        if (hcf->originatorBlockAckAgreementHandler)
            hcf->originatorBlockAckAgreementHandler->processTransmittedAddbaReq(frame, hcf);
    }

    virtual void transmittedAddbaResponse(
            const Ptr<const Ieee80211AddbaResponse>& frame) override
    {
        hcf->recipientBlockAckAgreementHandler->processTransmittedAddbaResp(frame, hcf);
    }

    virtual void transmittedDelba(const Ptr<const Ieee80211Delba>& frame) override
    {
        if (frame->getInitiator())
            hcf->originatorBlockAckAgreementHandler->processTransmittedDelba(frame);
        else
            hcf->recipientBlockAckAgreementHandler->processTransmittedDelba(frame);
    }

    virtual void transmittedBlockAckRequest(
            const Ptr<const Ieee80211BlockAckReq>& frame) override
    {
        edcaf->getAckHandler()->processTransmittedBlockAckReq(frame);
    }

    virtual void transmittedRts(const Ptr<const Ieee80211RtsFrame>& frame) override
    {
        hcf->rtsProcedure->processTransmittedRts(frame);
    }

    virtual void failedDataOrManagement(Packet *packet,
            const Ptr<const Ieee80211DataOrMgmtHeader>& frame,
            HcfFrameDispatchService::FailureKind failureKind) override
    {
        EV_INFO << (dynamicPtrCast<const Ieee80211DataHeader>(frame) ?
                "Data frame transmission failed\n" :
                "Management frame transmission failed\n");
        auto kind = failureKind == HcfFrameDispatchService::FailureKind::BLOCK_ACK_MISSING ?
                HcfOriginatorService::FailureKind::BLOCK_ACK_MISSING :
                HcfOriginatorService::FailureKind::ACK_TIMEOUT;
        hcf->processDispatchedFailure(packet, frame, kind, edcaf, accessCategory);
    }

    virtual void failedBlockAckRequest(Packet *,
            const Ptr<const Ieee80211BlockAckReq>& frame) override
    {
        hcf->processFailedBlockAckReq(edcaf, frame, true);
    }

    virtual void originatorAddbaResponse(
            const Ptr<const Ieee80211AddbaResponse>& frame) override
    {
        if (!hcf->originatorBlockAckAgreementHandler)
            return;
        hcf->originatorBlockAckAgreementHandler->processReceivedAddbaResp(frame,
                hcf->originatorBlockAckAgreementPolicy, hcf);
        auto agreement = hcf->originatorBlockAckAgreementHandler->getAgreement(
                frame->getTransmitterAddress(), frame->getTid());
        hcf->emit(cComponent::registerSignal("blockAckAgreementAdded"), agreement);
        hcf->resumeContention();
    }

    virtual void originatorAck(Packet *, const Ptr<const Ieee80211AckFrame>& frame,
            Packet *lastTransmittedPacket,
            const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader) override
    {
        hcf->processReceivedAck(edcaf, frame, lastTransmittedPacket,
                lastTransmittedHeader);
    }

    virtual void originatorBlockAck(
            const Ptr<const Ieee80211BlockAck>& frame) override
    {
        hcf->processReceivedBlockAck(edcaf, frame, accessCategory);
    }

    virtual void originatorCts(const Ptr<const Ieee80211CtsFrame>&) override
    {
        edcaf->getRecoveryProcedure()->ctsFrameReceived();
    }

    virtual void originatorIgnoredControl(
            const Ptr<const Ieee80211MacHeader>&) override {}

    virtual void originatorData(const Ptr<const Ieee80211DataHeader>& frame,
            const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader) override
    {
        hcf->originatorProcessReceivedDataFrame(frame, lastTransmittedHeader,
                accessCategory);
    }
};

class Hcf::TransmissionPreparationActions final :
        public HcfTransmissionPreparationService::IActions,
        public HcfAggregationService::ITransmissionPlanningActions
{
  private:
    Hcf *hcf;
    Edcaf *channelOwner;
    TxopProcedure *txop;
    const FrameSequenceContext *frameSequenceContext;
    HcfTransmissionPreparationService::ProtectionMechanism protectionMechanism;

  public:
    TransmissionPreparationActions(Hcf *hcf, Edcaf *channelOwner,
            TxopProcedure *txop, const FrameSequenceContext *frameSequenceContext,
            HcfTransmissionPreparationService::ProtectionMechanism protectionMechanism) :
        hcf(hcf), channelOwner(channelOwner), txop(txop),
        frameSequenceContext(frameSequenceContext),
        protectionMechanism(protectionMechanism)
    {
    }

    virtual bool isHtImplicitBlockAckEligible(
            const HcfTransmissionPreparationService::Request& request) const override
    {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(request.header);
        return frameSequenceContext != nullptr &&
                ieee80211::isHtImplicitBlockAckEligible(frameSequenceContext,
                        request.packet, dataHeader);
    }

    virtual AckPolicy selectAckPolicy(
            const HcfTransmissionPreparationService::Request& request,
            bool) const override
    {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(request.header);
        if (dataHeader == nullptr)
            return NORMAL_ACK;
        OriginatorBlockAckAgreement *agreement = nullptr;
        if (hcf->originatorBlockAckAgreementHandler)
            agreement = hcf->originatorBlockAckAgreementHandler->getAgreement(
                    dataHeader->getReceiverAddress(), dataHeader->getTid());
        return hcf->originatorAckPolicy->computeAckPolicy(request.packet,
                dataHeader, agreement);
    }

    virtual const IIeee80211Mode *selectMode(
            const HcfTransmissionPreparationService::Request& request) const override
    {
        auto modeReq = request.packet->findTag<Ieee80211ModeReq>();
        return modeReq == nullptr ? hcf->rateSelection->computeMode(
                request.packet, request.header, txop) : modeReq->getMode();
    }

    virtual void validateMode(
            const HcfTransmissionPreparationService::Request&,
            const IIeee80211Mode *mode) const override
    {
        hcf->modeSet->getPhyFamily(mode);
    }

    virtual HcfTransmissionPreparationService::AggregatePlan planAggregation(
            const HcfTransmissionPreparationService::Request& request,
            const IIeee80211Mode *mode, AckPolicy ackPolicy,
            bool implicitBlockAck) const override
    {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(request.header);
        auto phyFamily = hcf->modeSet->getPhyFamily(mode);
        bool aggregationAllowed = dataHeader != nullptr &&
                (dataHeader->getAckPolicy() == BLOCK_ACK || ackPolicy == BLOCK_ACK) &&
                txop->allowsMpduAggregation() &&
                !hcf->rtsPolicy->isRtsNeeded(request.packet, request.header) &&
                phyFamily != Ieee80211PhyFamily::DSSS &&
                phyFamily != Ieee80211PhyFamily::ERP_OFDM &&
                phyFamily != Ieee80211PhyFamily::OFDM;
        HcfAggregationService::TransmissionPlanningRequest planningRequest;
        planningRequest.sourcePacket = request.packet;
        planningRequest.mode = mode;
        planningRequest.phyFamily = phyFamily;
        planningRequest.aggregationAllowed = aggregationAllowed;
        planningRequest.implicitBlockAck = implicitBlockAck;
        auto plan = hcf->aggregationService.planTransmission(planningRequest, *this);
        HcfTransmissionPreparationService::AggregatePlan result;
        result.members = std::move(plan.members);
        result.materialize = plan.materialize;
        result.implicitBlockAck = plan.implicitBlockAck;
        return result;
    }

    virtual std::vector<Packet *> getCandidates(Packet *sourcePacket,
            bool implicitBlockAck, long long maxAggregateLength) const override
    {
        return implicitBlockAck ? frameSequenceContext->getHtImplicitBlockAckFrames() :
                channelOwner->getInProgressFrames()->getEligibleFramesLike(
                        sourcePacket, 64, maxAggregateLength);
    }

    virtual long long getMaxAggregateLength(
            const Ptr<const Ieee80211DataHeader>& sourceHeader,
            Ieee80211PhyFamily phyFamily) const override
    {
        int exponent = hcf->getMaxAmpduLengthExponent(
                sourceHeader->getReceiverAddress(), 7, phyFamily);
        return (1LL << (13 + exponent)) - 1;
    }

    virtual void applyRetryState(Packet *candidate) const override
    {
        channelOwner->getAckHandler()->setRetryBitIfNeeded(candidate);
    }

    virtual AckPolicy selectAckPolicy(Packet *candidate,
            const Ptr<const Ieee80211DataHeader>& header) const override
    {
        OriginatorBlockAckAgreement *agreement = nullptr;
        if (hcf->originatorBlockAckAgreementHandler)
            agreement = hcf->originatorBlockAckAgreementHandler->getAgreement(
                    header->getReceiverAddress(), header->getTid());
        return hcf->originatorAckPolicy->computeAckPolicy(candidate, header,
                agreement);
    }

    virtual void applySelectedPolicy(Packet *candidate,
            AckPolicy ackPolicy) const override
    {
        auto header = candidate->removeAtFront<Ieee80211DataHeader>();
        header->setAckPolicy(ackPolicy);
        header->setDurationField(SIMTIME_ZERO);
        candidate->insertAtFront(header);
    }

    virtual void aggregationTrimmed(size_t originalCount, size_t retainedCount,
            simtime_t durationLimit) const override
    {
        OPP_LOGPROXY(hcf, omnetpp::LOGLEVEL_DEBUG, nullptr).getStream()
                << "Trimmed A-MPDU from " << originalCount << " to "
                << retainedCount << " MPDUs for PPDU/TXOP duration "
                << durationLimit << ".\n";
    }

    virtual void validateAggregation(
            const HcfTransmissionPreparationService::Request&,
            const IIeee80211Mode *,
            const HcfTransmissionPreparationService::AggregatePlan&) const override
    {
    }

    virtual void validateProtection(
            const HcfTransmissionPreparationService::Request&,
            const IIeee80211Mode *,
            const HcfTransmissionPreparationService::AggregatePlan&) const override
    {
    }

    virtual HcfTransmissionPreparationService::ProtectionPlan computeProtection(
            const HcfTransmissionPreparationService::Request& request,
            const IIeee80211Mode *,
            const HcfTransmissionPreparationService::AggregatePlan& aggregatePlan) const override
    {
        HcfTransmissionPreparationService::ProtectionPlan result;
        if (protectionMechanism ==
                HcfTransmissionPreparationService::ProtectionMechanism::SINGLE) {
            if (aggregatePlan.materialize || request.durationFinalized ||
                    request.durationExemptForSingleProtection)
                return result;
            auto pendingPacket = channelOwner->getInProgressFrames()->
                    getPendingFrameFor(request.packet);
            const auto& pendingHeader = pendingPacket == nullptr ? nullptr :
                    pendingPacket->peekAtFront<Ieee80211DataOrMgmtHeader>();
            result.updateDuration = true;
            result.duration = hcf->singleProtectionMechanism->computeDurationField(
                    request.packet, request.header, pendingPacket, pendingHeader,
                    txop, hcf->recipientAckPolicy, request.ifs);
        }
        else if (!request.durationFinalized) {
            result.updateDuration = true;
            result.duration = hcf->singleProtectionMechanism->computeDurationField(
                    request.packet, request.header, nullptr, nullptr, txop,
                    hcf->recipientAckPolicy, request.ifs);
        }
        return result;
    }

    virtual void applySourceRetryState(Packet *sourcePacket) override
    {
        if (dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                sourcePacket->peekAtFront<Ieee80211MacHeader>()))
            channelOwner->getAckHandler()->setRetryBitIfNeeded(sourcePacket);
    }

    virtual Ptr<const Ieee80211MacHeader> applyAckPolicy(Packet *sourcePacket,
            const Ptr<const Ieee80211MacHeader>& header, AckPolicy ackPolicy,
            const HcfTransmissionPreparationService::AggregatePlan&) override
    {
        if (dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            auto mutableHeader = sourcePacket->removeAtFront<Ieee80211DataHeader>();
            mutableHeader->setAckPolicy(ackPolicy);
            sourcePacket->insertAtFront(mutableHeader);
            return sourcePacket->peekAtFront<Ieee80211MacHeader>();
        }
        return header;
    }

    virtual Ptr<const Ieee80211MacHeader> applyModePreparation(Packet *sourcePacket,
            const Ptr<const Ieee80211MacHeader>&,
            const IIeee80211Mode *mode) override
    {
        hcf->attachPendingHtMcsControl(sourcePacket, mode);
        return sourcePacket->peekAtFront<Ieee80211MacHeader>();
    }

    virtual void applyAggregateMemberState(Packet *,
            const HcfTransmissionPreparationService::AggregatePlan& aggregatePlan) override
    {
        for (auto frame : aggregatePlan.members) {
            if (!aggregatePlan.implicitBlockAck)
                continue;
            auto mutableHeader = frame->removeAtFront<Ieee80211DataHeader>();
            mutableHeader->setAckPolicy(NORMAL_ACK);
            mutableHeader->setTransmitterAddress(hcf->mac->getAddress());
            frame->insertAtFront(mutableHeader);
        }
    }

    virtual void applyDuration(Packet *sourcePacket,
            const Ptr<const Ieee80211MacHeader>&, simtime_t duration) override
    {
        auto mutableHeader = sourcePacket->removeAtFront<Ieee80211MacHeader>();
        mutableHeader->setDurationField(duration);
        sourcePacket->insertAtFront(mutableHeader);
        if (protectionMechanism ==
                HcfTransmissionPreparationService::ProtectionMechanism::SINGLE)
            OPP_LOGPROXY(hcf, omnetpp::LOGLEVEL_DEBUG, nullptr).getStream()
                     << "Duration for " << sourcePacket->getName()
                     << " is set to " << duration << " s.\n";
    }

    virtual Packet *materializeAggregate(Packet *sourcePacket,
            const HcfTransmissionPreparationService::AggregatePlan& aggregatePlan) override
    {
        auto aggregate = hcf->aggregationService.materializeTransmission(
                sourcePacket, aggregatePlan.members, hcf->mac->getFcsMode(),
                aggregatePlan.implicitBlockAck);
        hcf->emit(cComponent::registerSignal("ampduCreated"), aggregate);
        hcf->emit(cComponent::registerSignal("ampduNumMpdus"),
                (unsigned long)aggregatePlan.members.size());
        return aggregate;
    }

    virtual void setMode(Packet *transmittedPacket,
            const Ptr<const Ieee80211MacHeader>& header,
            const IIeee80211Mode *mode) override
    {
        hcf->setFrameMode(transmittedPacket, header, mode);
    }

    virtual void setSourceMode(Packet *sourcePacket,
            const Ptr<const Ieee80211MacHeader>& header,
            const IIeee80211Mode *mode) override
    {
        hcf->setFrameMode(sourcePacket, header, mode);
    }

    virtual void recordSelectedMode(Packet *transmittedPacket,
            const IIeee80211Mode *mode) override
    {
        hcf->recordSelectedMode(transmittedPacket, mode);
    }

    virtual void observeSelectedRate(Packet *transmittedPacket,
            const IIeee80211Mode *mode) override
    {
        hcf->emit(cComponent::registerSignal("datarateSelected"),
                mode->getDataMode()->getNetBitrate().get<bps>(), transmittedPacket);
        OPP_LOGPROXY(hcf, omnetpp::LOGLEVEL_DEBUG, nullptr).getStream()
                 << "Datarate for " << transmittedPacket->getName()
                 << " is set to " << mode->getDataMode()->getNetBitrate() << ".\n";
    }

    virtual void transmitBorrowed(Packet *transmittedPacket,
            const Ptr<const Ieee80211MacHeader>& header, simtime_t ifs) override
    {
        hcf->tx->transmitFrame(transmittedPacket, header, ifs, hcf);
    }

    virtual void deleteTemporaryPacket(Packet *packet) noexcept override
    {
        delete packet;
    }
};

enum class HcfFailurePath {
    ORIGINATOR,
    INTERNAL_COLLISION,
    RTS_PROTECTION,
};

struct HcfFailureResult {
    HcfOriginatorService::Disposition disposition = HcfOriginatorService::Disposition::PROCESSED;
    HcfOriginatorService::TerminalAction terminalAction = HcfOriginatorService::TerminalAction::NONE;
};

class HcfOriginatorActions final : public HcfOriginatorService::IActions
{
  private:
    using Frame = HcfOriginatorService::Frame;
    using FrameIdentity = HcfOriginatorService::FrameIdentity;
    using FailureKind = HcfOriginatorService::FailureKind;
    using BlockAckMemberStatus = HcfOriginatorService::BlockAckMemberStatus;

    Hcf *hcf;
    Edcaf *edcaf;
    AccessCategory accessCategory;
    bool processingBlockAck = false;
    unsigned int retiredBlockAckStateCount = 0;
    std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>> acknowledgedFrames;

    Ptr<const Ieee80211DataHeader> getDataHeader(const Frame& frame) const
    {
        return dynamicPtrCast<const Ieee80211DataHeader>(frame.header);
    }

    Ptr<const Ieee80211MgmtHeader> getManagementHeader(const Frame& frame) const
    {
        return dynamicPtrCast<const Ieee80211MgmtHeader>(frame.header);
    }

  public:
    HcfOriginatorActions(Hcf *hcf, Edcaf *edcaf, AccessCategory accessCategory) :
        hcf(hcf), edcaf(edcaf), accessCategory(accessCategory)
    {
    }

    virtual bool isCurrent(const FrameIdentity& identity) const noexcept override
    {
        auto inProgressFrames = edcaf->getInProgressFrames();
        for (int i = 0; i < inProgressFrames->getLength(); i++)
            if (inProgressFrames->getFrames(i) == identity.packet)
                return true;
        return false;
    }

    virtual void processTransmitted(const Frame& frame,
            HcfOriginatorService::ExpectedResponse) override
    {
        edcaf->getAckHandler()->processTransmittedDataOrMgmtFrame(frame.header);
        if (auto dataHeader = getDataHeader(frame);
                dataHeader != nullptr && hcf->originatorBlockAckAgreementHandler)
            hcf->originatorBlockAckAgreementHandler->processTransmittedDataFrame(
                    frame.packet, dataHeader,
                    hcf->originatorBlockAckAgreementPolicy, hcf);
    }

    virtual void processNoResponseSuccess(const Frame&) override
    {
    }

    virtual void processTransmissionFailed(const Frame& frame,
            FailureKind) override
    {
        if (auto dataHeader = getDataHeader(frame))
            edcaf->getRecoveryProcedure()->dataFrameTransmissionFailed(
                    frame.packet, dataHeader);
        else if (auto managementHeader = getManagementHeader(frame))
            hcf->edca->getMgmtAndNonQoSRecoveryProcedure()->
                    dataOrMgmtFrameTransmissionFailed(frame.packet,
                            managementHeader, edcaf->getStationRetryCounters());
        else
            throw cRuntimeError("Unknown HCF originator frame kind");
    }

    void processRtsFailure(const Frame& frame)
    {
        if (auto dataHeader = getDataHeader(frame))
            edcaf->getRecoveryProcedure()->rtsFrameTransmissionFailed(dataHeader);
        else if (auto managementHeader = getManagementHeader(frame))
            hcf->edca->getMgmtAndNonQoSRecoveryProcedure()->
                    rtsFrameTransmissionFailed(managementHeader,
                            edcaf->getStationRetryCounters());
        else
            throw cRuntimeError("Unknown HCF RTS-protected frame kind");
    }

    bool isRtsRetryLimitReached(const Frame& frame)
    {
        if (auto dataHeader = getDataHeader(frame))
            return edcaf->getRecoveryProcedure()->isRtsFrameRetryLimitReached(
                    frame.packet, dataHeader);
        if (auto managementHeader = getManagementHeader(frame))
            return hcf->edca->getMgmtAndNonQoSRecoveryProcedure()->
                    isRtsFrameRetryLimitReached(frame.packet, managementHeader);
        throw cRuntimeError("Unknown HCF RTS-protected frame kind");
    }

    virtual bool isRetryLimitReached(const Frame& frame) override
    {
        if (auto dataHeader = getDataHeader(frame))
            return edcaf->getRecoveryProcedure()->isRetryLimitReached(
                    frame.packet, dataHeader);
        if (auto managementHeader = getManagementHeader(frame))
            return hcf->edca->getMgmtAndNonQoSRecoveryProcedure()->
                    isRetryLimitReached(frame.packet, managementHeader);
        throw cRuntimeError("Unknown HCF originator frame kind");
    }

    virtual int getRetryCount(const Frame& frame) override
    {
        if (auto dataHeader = getDataHeader(frame))
            return edcaf->getRecoveryProcedure()->getRetryCount(
                    frame.packet, dataHeader);
        if (auto managementHeader = getManagementHeader(frame))
            return hcf->edca->getMgmtAndNonQoSRecoveryProcedure()->getRetryCount(
                    frame.packet, managementHeader);
        throw cRuntimeError("Unknown HCF originator frame kind");
    }

    virtual void reportRateResult(const Frame& frame, int retryCount,
            bool successful, bool retryLimitReached) override
    {
        if (hcf->dataAndMgmtRateControl)
            hcf->dataAndMgmtRateControl->frameTransmitted(frame.packet,
                    retryCount, successful, retryLimitReached);
    }

    virtual void processAckStateFailed(const Frame& frame) override
    {
        edcaf->getAckHandler()->processFailedFrame(frame.header);
    }

    virtual void processRetryLimitReached(const Frame& frame) override
    {
        if (auto dataHeader = getDataHeader(frame))
            edcaf->getRecoveryProcedure()->retryLimitReached(frame.packet,
                    dataHeader);
        else if (auto managementHeader = getManagementHeader(frame))
            hcf->edca->getMgmtAndNonQoSRecoveryProcedure()->retryLimitReached(
                    frame.packet, managementHeader);
        else
            throw cRuntimeError("Unknown HCF originator frame kind");
    }

    virtual void markRetry(const Frame& frame) override
    {
        auto header = frame.packet->removeAtFront<Ieee80211DataOrMgmtHeader>();
        header->setRetry(true);
        frame.packet->insertAtFront(header);
    }

    virtual void processAckRecoverySuccess(const Frame& frame) override
    {
        if (auto dataHeader = getDataHeader(frame))
            edcaf->getRecoveryProcedure()->ackFrameReceived(frame.packet,
                    dataHeader);
        else if (auto managementHeader = getManagementHeader(frame))
            hcf->edca->getMgmtAndNonQoSRecoveryProcedure()->ackFrameReceived(
                    frame.packet, managementHeader,
                    edcaf->getStationRetryCounters());
        else
            throw cRuntimeError("Unknown HCF originator frame kind");
    }

    virtual void processAckStateReceived(const Frame& frame,
            const Ptr<const Ieee80211AckFrame>& ackFrame) override
    {
        edcaf->getAckHandler()->processReceivedAck(ackFrame, frame.header);
    }

    virtual void processBlockAckReceived(
            const Ptr<const Ieee80211BlockAck>& blockAck) override
    {
        processingBlockAck = true;
        edcaf->getRecoveryProcedure()->blockAckFrameReceived();
        acknowledgedFrames = edcaf->getAckHandler()->processReceivedBlockAck(
                blockAck);
    }

    virtual BlockAckMemberStatus getBlockAckMemberStatus(
            const Frame& frame) override
    {
        auto dataHeader = getDataHeader(frame);
        if (dataHeader == nullptr)
            return BlockAckMemberStatus::NOT_COVERED;
        auto status = edcaf->getAckHandler()->getQoSDataAckStatus(dataHeader);
        if (status == QosAckHandler::Status::BLOCK_ACK_ARRIVED_ACKED)
            return BlockAckMemberStatus::ACKNOWLEDGED;
        if (status == QosAckHandler::Status::BLOCK_ACK_ARRIVED_UNACKED)
            return BlockAckMemberStatus::UNACKNOWLEDGED;
        return BlockAckMemberStatus::NOT_COVERED;
    }

    virtual void processBlockAckMemberResult(const Frame&,
            BlockAckMemberStatus) override
    {
    }

    virtual void processBlockAckAgreement(
            const Ptr<const Ieee80211BlockAck>& blockAck) override
    {
        hcf->originatorProcessBlockAckResult(blockAck, acknowledgedFrames,
                accessCategory);
        if (hcf->originatorBlockAckAgreementHandler)
            hcf->originatorBlockAckAgreementHandler->processReceivedBlockAck(
                    blockAck, hcf);
        EV_TRACE << "It has acknowledged the following frames:" << std::endl;
        for (auto it : acknowledgedFrames)
            EV_TRACE << "   sequenceNumber = "
                    << it.second.second.getSequenceNumber()
                    << ", fragmentNumber = "
                    << (int)it.second.second.getFragmentNumber() << std::endl;
    }

    virtual void retireInProgress(const Frame& frame) override
    {
        edcaf->getInProgressFrames()->dropFrame(frame.packet);
    }

    virtual void retireAckState(const Frame& frame) override
    {
        if (!processingBlockAck)
            edcaf->getAckHandler()->dropFrame(frame.header);
        else if (++retiredBlockAckStateCount == acknowledgedFrames.size())
            edcaf->getAckHandler()->dropFrames(acknowledgedFrames);
    }

    virtual void reportManagementResult(const Frame& frame,
            HcfOriginatorService::ManagementResultKind resultKind) override
    {
        if (resultKind == HcfOriginatorService::ManagementResultKind::ACKNOWLEDGED)
            hcf->notifyMgmtExchangeResult(frame.packet,
                    Ieee80211MgmtExchangeResultKind::ACKNOWLEDGED);
        else
            hcf->notifyMgmtExchangeResult(frame.packet,
                    Ieee80211MgmtExchangeResultKind::RETRY_LIMIT_REACHED);
    }

    void reportRetainingRetry(const Frame& frame, HcfFailurePath path)
    {
        if (path == HcfFailurePath::ORIGINATOR)
            EV_INFO << "Retrying frame " << frame.packet->getName() << ".\n";
    }

    void reportDropping(const Frame& frame, HcfFailurePath path)
    {
        if (path == HcfFailurePath::INTERNAL_COLLISION)
            EV_DETAIL << "The frame has reached its retry limit. Dropping it" << std::endl;
        else if (path == HcfFailurePath::RTS_PROTECTION)
            EV_INFO << "Dropping RTS/CTS protected frame " << frame.packet->getName()
                    << ", because retry limit is reached.\n";
        else
            EV_INFO << "Dropping frame " << frame.packet->getName()
                    << ", because retry limit is reached.\n";
    }

    void observePacketDropped(const Frame& frame)
    {
        PacketDropDetails details;
        details.setReason(RETRY_LIMIT_REACHED);
        details.setLimit(-1);
        hcf->emit(cComponent::registerSignal("packetDropped"), frame.packet, &details);
    }

    void observeLinkBroken(const Frame& frame)
    {
        hcf->emit(cComponent::registerSignal("linkBroken"), frame.packet);
    }
};

static HcfFailureResult processHcfFailure(
        const HcfOriginatorService::Frame& frame,
        HcfOriginatorService::FailureKind failureKind, HcfFailurePath path,
        HcfOriginatorActions& actions)
{
    HcfFailureResult result;
    if (!actions.isCurrent(frame.identity)) {
        result.disposition = HcfOriginatorService::Disposition::STALE_OR_DUPLICATE;
        return result;
    }

    // IEEE Std 802.11-2024, 10.23.2.2, 10.23.2.4 and 10.3.2.9:
    // the authoritative recovery owners update counters before evaluation.
    bool retryLimitReached;
    if (path == HcfFailurePath::RTS_PROTECTION) {
        actions.processRtsFailure(frame);
        retryLimitReached = actions.isRtsRetryLimitReached(frame);
    }
    else {
        actions.processTransmissionFailed(frame, failureKind);
        retryLimitReached = actions.isRetryLimitReached(frame);
    }
    if (path == HcfFailurePath::ORIGINATOR) {
        auto retryCount = actions.getRetryCount(frame);
        actions.reportRateResult(frame, retryCount, false, retryLimitReached);
        actions.processAckStateFailed(frame);
    }
    if (retryLimitReached) {
        if (path == HcfFailurePath::INTERNAL_COLLISION)
            actions.reportDropping(frame, path);
        actions.processRetryLimitReached(frame);
        actions.retireInProgress(frame);
        actions.retireAckState(frame);
        if (path != HcfFailurePath::INTERNAL_COLLISION)
            actions.reportDropping(frame, path);
        actions.observePacketDropped(frame);
        if (frame.kind == HcfOriginatorService::FrameKind::MANAGEMENT)
            actions.reportManagementResult(frame,
                    HcfOriginatorService::ManagementResultKind::RETRY_LIMIT_REACHED);
        actions.observeLinkBroken(frame);
        result.terminalAction = HcfOriginatorService::TerminalAction::DROP_RETIRE;
    }
    else {
        actions.reportRetainingRetry(frame, path);
        if (path == HcfFailurePath::ORIGINATOR)
            actions.markRetry(frame);
        result.terminalAction = HcfOriginatorService::TerminalAction::RETAIN_RETRY;
    }
    return result;
}

void Hcf::claimIngressPacket(Packet *packet)
{
    take(packet);
}

void Hcf::returnIngressPacketToCaller(Packet *packet, cComponent *caller) noexcept
{
    cContextSwitcher contextSwitcher(caller);
    drop(packet);
}

simsignal_t Hcf::edcaCollisionDetectedSignal = cComponent::registerSignal("edcaCollisionDetected");
simsignal_t Hcf::blockAckAgreementAddedSignal = cComponent::registerSignal("blockAckAgreementAdded");
simsignal_t Hcf::blockAckAgreementDeletedSignal = cComponent::registerSignal("blockAckAgreementDeleted");
simsignal_t Hcf::ampduCreatedSignal = cComponent::registerSignal("ampduCreated");
simsignal_t Hcf::ampduNumMpdusSignal = cComponent::registerSignal("ampduNumMpdus");

Define_Module(Hcf);

void Hcf::notifyMgmtExchangeResult(Packet *packet,
        Ieee80211MgmtExchangeResultKind kind)
{
    if (mgmtExchangeResultHandler == nullptr || packet == nullptr)
        return;
    auto header = dynamicPtrCast<const Ieee80211MgmtHeader>(packet->peekAtFront<Ieee80211MacHeader>());
    auto tag = packet->findTag<Ieee80211MgmtExchangeTag>();
    if (header != nullptr && tag != nullptr)
        mgmtExchangeResultHandler->handleIeee80211MgmtExchangeResult(
                Ieee80211MgmtExchangeResult(tag->getTransactionId(), kind));
}

static bool isHeMuContainerPacket(Packet *packet)
{
    return packet != nullptr && packet->findTag<Ieee80211HeMuContainerReq>() != nullptr;
}

void Hcf::addBufferedTrafficServiceBytes(uint32_t& total, uint64_t amount)
{
    HcfMacSapTracker::addBufferedTrafficServiceBytes(total, amount);
}

uint32_t Hcf::calculateBufferedTrafficServiceBytes(Edcaf *edcaf, const MacAddress& peer,
        int tid, const std::vector<Packet *>& additionalPackets) const
{
    HcfMacSapTracker::PacketVector pendingPackets;
    auto pendingQueue = edcaf->getPendingQueue();
    for (int i = 0; i < pendingQueue->getNumPackets(); i++)
        pendingPackets.push_back(pendingQueue->getPacket(i));
    HcfMacSapTracker::PacketVector inProgressPackets;
    auto inProgressFrames = edcaf->getInProgressFrames();
    for (int i = 0; i < inProgressFrames->getLength(); i++)
        inProgressPackets.push_back(inProgressFrames->getFrames(i));
    HcfMacSapTracker::PacketVector additionalPacketView;
    for (auto packet : additionalPackets)
        additionalPacketView.push_back(packet);
    return macSapTracker.calculateBufferedTrafficServiceBytes(peer, tid,
            pendingPackets, inProgressPackets, additionalPacketView);
}

uint64_t Hcf::allocateServiceDataUnitId()
{
    return macSapTracker.allocateServiceDataUnitId();
}

void Hcf::tagMacSapServiceDataUnit(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header)
{
    macSapTracker.tagMacSapServiceDataUnit(packet, header);
}

uint32_t Hcf::getBufferedTrafficServiceBytes(
        Edcaf *edcaf, const MacAddress& peer, int tid) const
{
    if (heRuntime != nullptr)
        return heRuntime->getBufferedTrafficServiceBytes(edcaf, peer, tid);
    return calculateBufferedTrafficServiceBytes(edcaf, peer, tid, {});
}

void Hcf::initialize(int stage)
{
    ModeSetListener::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        auto featureSetModule = getSubmodule("featureSet");
        auto featureSet = dynamic_cast<IHcfFeatureSet *>(featureSetModule);
        if (featureSet == nullptr)
            throw cRuntimeError("HCF featureSet submodule '%s' does not implement IHcfFeatureSet",
                    featureSetModule == nullptr ? "<missing>" : featureSetModule->getFullPath().c_str());
        HcfFeatureConfiguration featureConfiguration;
        featureConfiguration.enableHtSounding = hasPar("enableHtSounding") &&
                par("enableHtSounding").boolValue();
        featureConfiguration.enableVhtSuBeamforming = hasPar("enableVhtSuBeamforming") &&
                par("enableVhtSuBeamforming").boolValue();
        featureConfiguration.enableVhtDlMuMimo = hasPar("enableVhtDlMuMimo") &&
                par("enableVhtDlMuMimo").boolValue();
        featureConfiguration.enableHeUlMuOfdma = hasPar("enableUlMuOfdma") &&
                par("enableUlMuOfdma").boolValue();
        featureConfiguration.enableHeDlMuMimo = hasPar("enableDlMuMimo") &&
                par("enableDlMuMimo").boolValue();
        featureSet->configureFeatures(featureConfiguration);
        mac = check_and_cast<Ieee80211Mac *>(getContainingNicModule(this)->getSubmodule("mac"));
        exchangeEngine = std::make_unique<HcfExchangeEngine>(
                std::make_unique<FrameSequenceHandler>());
        exchangeEngine->initializeTimers();
        edca = check_and_cast<Edca *>(getSubmodule("edca"));
        hcca = check_and_cast<Hcca *>(getSubmodule("hcca"));
        tx = check_and_cast<ITx *>(getModuleByPath(par("txModule")));
        rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
        dataAndMgmtRateControl = dynamic_cast<IRateControl *>(getSubmodule("rateControl"));
        originatorBlockAckAgreementPolicy = dynamic_cast<IOriginatorBlockAckAgreementPolicy *>(getSubmodule("originatorBlockAckAgreementPolicy"));
        recipientBlockAckAgreementPolicy = dynamic_cast<IRecipientBlockAckAgreementPolicy *>(getSubmodule("recipientBlockAckAgreementPolicy"));
        rateSelection = check_and_cast<IQosRateSelection *>(getSubmodule("rateSelection"));
        WATCH_EXPR("frameSequenceInfo", getFrameSequenceInfo());
        WATCH(lastSelectedModePacketName);
        WATCH(lastSelectedModeName);
        WATCH(lastSelectedModeNetBitrate);
        WATCH(lastSelectedModeBandwidth);
        WATCH(lastSelectedModeNumSpatialStreams);
        originatorDataService = check_and_cast<IOriginatorMacDataService *>(getSubmodule(("originatorMacDataService")));
        recipientDataService = check_and_cast<IRecipientQosMacDataService *>(getSubmodule("recipientMacDataService"));
        originatorAckPolicy = check_and_cast<IOriginatorQoSAckPolicy *>(getSubmodule("originatorAckPolicy"));
        recipientAckPolicy = check_and_cast<IRecipientQosAckPolicy *>(getSubmodule("recipientAckPolicy"));
        singleProtectionMechanism = check_and_cast<SingleProtectionMechanism *>(getSubmodule("singleProtectionMechanism"));
        rtsProcedure = std::make_unique<RtsProcedure>();
        rtsPolicy = check_and_cast<IRtsPolicy *>(getSubmodule("rtsPolicy"));
        recipientAckProcedure = std::make_unique<RecipientAckProcedure>();
        ctsProcedure = std::make_unique<CtsProcedure>();
        ctsPolicy = check_and_cast<ICtsPolicy *>(getSubmodule("ctsPolicy"));
        if (originatorBlockAckAgreementPolicy && recipientBlockAckAgreementPolicy) {
            recipientBlockAckAgreementHandler = std::make_unique<RecipientBlockAckAgreementHandler>();
            originatorBlockAckAgreementHandler = std::make_unique<OriginatorBlockAckAgreementHandler>();
            originatorBlockAckProcedure = std::make_unique<OriginatorBlockAckProcedure>();
            recipientBlockAckProcedure = std::make_unique<RecipientBlockAckProcedure>();
            originatorDataService->setBlockAckAgreementHandler(
                    originatorBlockAckAgreementHandler.get());
        }
        if (featureSet->getAmendmentRuntimeKind() == HcfAmendmentRuntimeKind::VHT) {
            vhtRuntime = std::make_unique<HcfVhtRuntime>(this);
            vhtRuntime->initialize();
        }
        if (featureSet->getAmendmentRuntimeKind() == HcfAmendmentRuntimeKind::HE) {
            auto heServices = featureSet->getHeRuntimeServices();
            if (!heServices.isComplete())
                throw cRuntimeError("HE HCF feature set returned incomplete runtime services");
            HeHcfRuntime::Bindings bindings;
            bindings.owner = this;
            bindings.mac = mac;
            bindings.edca = edca;
            bindings.dlScheduler = check_and_cast<IIeee80211HeDlScheduler *>(
                    getSubmodule("dlScheduler"));
            bindings.ulCoordinator = check_and_cast<HeUlCoordinator *>(
                    getSubmodule("ulCoordinator"));
            bindings.blockAckHandler = originatorBlockAckAgreementHandler.get();
            bindings.blockAckPolicy = originatorBlockAckAgreementPolicy;
            bindings.isFrameSequenceRunning = [this] { return isFrameSequenceRunning(); };
            bindings.invalidateBasePeer = [this] (const MacAddress& peer) {
                htFeature->invalidatePeer(peer);
                if (vhtRuntime != nullptr)
                    vhtRuntime->invalidatePeer(peer);
            };
            heRuntime = std::make_unique<HeHcfRuntime>(heServices, bindings,
                    par("csiValidityDuration"), par("defaultCsiLeakage"),
                    par("csiLeakageOverrides").stdstringValue());
            heRuntime->initialize();
        }
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        if (modeSet == nullptr)
            throw cRuntimeError("HCF HT feature cannot initialize without an IEEE 802.11 mode set");
        htFeature->configure(mac, [this] { return modeSet; },
                dynamic_cast<IIeee80211HtRateControl *>(getSubmodule("rateControl")), tx,
                rateSelection,
                [this] (const MacAddress& peer, int defaultExponent,
                        Ieee80211PhyFamily phyFamily) {
                    return getMaxAmpduLengthExponent(peer, defaultExponent, phyFamily);
                },
                originatorBlockAckAgreementHandler.get(), originatorAckPolicy,
                par("enableHtSounding"), par("htSoundingNsts"),
                static_cast<Ieee80211HtFeedbackKind>(
                        par("htSoundingFeedbackKind").intValue()),
                par("htSoundingRetryInterval"));
        if (heRuntime != nullptr)
            heRuntime->initializeLinkLayer();
    }
}

void Hcf::receiveSignal(cComponent *source, simsignal_t signalID,
        cObject *obj, cObject *details)
{
    ModeSetListener::receiveSignal(source, signalID, obj, details);
    if (signalID == modesetChangedSignal && vhtRuntime != nullptr)
        vhtRuntime->modeSetChanged();
}

std::string Hcf::getFrameSequenceInfo() const
{
    if (!isFrameSequenceRunning())
        return "";
    auto history = getFrameSequenceForLegacyAdapter()->getHistory();
    if (history.length() > 32) {
        history.erase(history.begin(), history.end() - 32);
        history = "..." + history;
    }
    return "Fs: " + history;
}

void Hcf::recordSelectedMode(Packet *packet, const IIeee80211Mode *mode)
{
    if (testActionPort.recordSelectedMode) {
        testActionPort.recordSelectedMode(packet, mode);
    }
    lastSelectedModePacketName = packet ? packet->getName() : "";
    lastSelectedModeName = mode ? mode->getName() : "";
    lastSelectedModeNetBitrate = mode ? mode->getDataMode()->getNetBitrate().get() : -1;
    lastSelectedModeBandwidth = mode ? mode->getDataMode()->getBandwidth().get() : -1;
    lastSelectedModeNumSpatialStreams = mode ? mode->getDataMode()->getNumberOfSpatialStreams() : -1;
}

void Hcf::forEachChild(cVisitor *v)
{
    SimpleModule::forEachChild(v);
    if (getFrameSequenceContext() != nullptr)
        v->visit(const_cast<FrameSequenceContext *>(getFrameSequenceContext()));
}

void Hcf::handleMessage(cMessage *msg)
{
    if (heRuntime != nullptr && heRuntime->handleMessage(msg))
        return;
    if (!exchangeEngine->handleMessage(msg, makeExchangeActions()))
        throw cRuntimeError("Unknown msg type");
}

void Hcf::finish()
{
    if (heRuntime != nullptr)
        heRuntime->finish();
    cSimpleModule::finish();
}

void Hcf::handleBlockAckInactivityTimeout()
{
    if (originatorBlockAckAgreementHandler && recipientBlockAckAgreementHandler) {
        originatorBlockAckAgreementHandler->blockAckAgreementExpired(this, this);
        recipientBlockAckAgreementHandler->blockAckAgreementExpired(this, this);
        resumeContention();
    }
    else
        throw cRuntimeError("Unknown event");
}

void Hcf::handleDeferredStartRxTimeout()
{
    exchangeEngine->handleDeferredStartRxTimeout(makeExchangeActions());
}

void Hcf::processResponseAndCancelStartRxTimerIfCompleted(Packet *packet, IReceiveStep *receiveStep)
{
    exchangeEngine->processResponseAndCancelStartRxTimerIfCompleted(
            packet, receiveStep, makeExchangeActions());
}

HcfExchangeEngine::Actions Hcf::makeExchangeActions()
{
    HcfExchangeEngine::Actions actions;
    actions.isReceptionInProgress = [this] () { return isReceptionInProgress(); };
    actions.transmitFrame = [this] (Packet *packet, simtime_t ifs) { transmitFrame(packet, ifs); };
    actions.originatorProcessRtsProtectionFailed = [this] (Packet *packet) { originatorProcessRtsProtectionFailed(packet); };
    actions.originatorProcessTransmittedFrame = [this] (Packet *packet) { originatorProcessTransmittedFrame(packet); };
    actions.originatorProcessReceivedFrame = [this] (Packet *packet, Packet *lastTransmittedPacket) {
        originatorProcessReceivedFrame(packet, lastTransmittedPacket);
    };
    actions.originatorProcessFailedFrame = [this] (Packet *packet) { originatorProcessFailedFrame(packet); };
    actions.frameSequenceStarted = [this] (const FrameSequenceContext *context) {
        emit(cComponent::registerSignal("frameSequenceStarted"), context);
    };
    actions.frameSequenceFinished = [this] (const FrameSequenceContext *) { frameSequenceFinished(); };
    actions.resumeContention = [this] { resumeContention(); };
    actions.discardResponse = [this] (Packet *packet) {
        EV_INFO << "This frame is not for us" << std::endl;
        PacketDropDetails details;
        details.setReason(NOT_ADDRESSED_TO_US);
        emit(cComponent::registerSignal("packetDropped"), packet, &details);
        delete packet;
    };
    actions.inactivityTimeout = [this] { handleBlockAckInactivityTimeout(); };
    actions.cancelTimer = [this] (cMessage *timer) { cancelEvent(timer); };
    actions.scheduleTimer = [this] (simtime_t timeout, cMessage *timer) { scheduleAfter(timeout, timer); };
    actions.rescheduleTimer = [this] (simtime_t timeout, cMessage *timer) { rescheduleAfter(timeout, timer); };
    return actions;
}

void Hcf::replaceFrameSequenceHandler(
        std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler)
{
    exchangeEngine->replaceFrameSequenceHandler(std::move(frameSequenceHandler));
}

void Hcf::installRecipientBlockAckHandlerForTesting(
        std::unique_ptr<IRecipientBlockAckAgreementHandler> handler)
{
    recipientBlockAckAgreementHandler = std::move(handler);
    recipientAckProcedure = std::make_unique<RecipientAckProcedure>();
    recipientBlockAckProcedure = std::make_unique<RecipientBlockAckProcedure>();
}

void Hcf::installOriginatorBlockAckHandlerForTesting(
        std::unique_ptr<IOriginatorBlockAckAgreementHandler> handler)
{
    originatorBlockAckAgreementHandler = std::move(handler);
    if (originatorBlockAckProcedure == nullptr)
        originatorBlockAckProcedure = std::make_unique<OriginatorBlockAckProcedure>();
}

void Hcf::processRecipientBlockAckRequestForTesting(Packet *packet,
        const Ptr<const Ieee80211MultiTidBlockAckReq>& header,
        MultiTidBlockAckResponseFormat responseFormat, uint16_t responseAid)
{
    recipientBlockAckProcedure->processReceivedBlockAckReq(packet, header,
            recipientAckPolicy, recipientBlockAckAgreementHandler.get(), this,
            responseFormat, responseAid);
}

void Hcf::startExchangeFrameSequence(IFrameSequence *frameSequence,
        FrameSequenceContext *context)
{
    exchangeEngine->startFrameSequence(frameSequence, context, makeExchangeActions());
}

bool Hcf::isFrameSequenceRunning() const
{
    return exchangeEngine != nullptr && exchangeEngine->isSequenceRunning();
}

const FrameSequenceContext *Hcf::getFrameSequenceContext() const
{
    if (testActionPort.getFrameSequenceContext)
        return testActionPort.getFrameSequenceContext();
    return exchangeEngine == nullptr ? nullptr : exchangeEngine->getContext();
}

const IFrameSequence *Hcf::getFrameSequenceForLegacyAdapter() const
{
    return exchangeEngine == nullptr ? nullptr :
            exchangeEngine->getFrameSequenceForLegacyAdapter();
}

IFrameSequenceHandler::ICallback *Hcf::getFrameSequenceCallbackForLegacyAdapter() const
{
    return exchangeEngine->getFrameSequenceCallbackForLegacyAdapter();
}

cMessage *Hcf::getStartRxTimerForTest() const
{
    return exchangeEngine->getStartRxTimerForTest();
}

bool Hcf::hasDeferredStartRxTimeoutForTest() const
{
    return exchangeEngine->hasDeferredStartRxTimeout();
}

void Hcf::clearExchangeTimerStateForTest()
{
    exchangeEngine->clearTimerStateForTest();
}

void Hcf::refreshDisplay() const
{
    ModeSetListener::refreshDisplay();
    if (isFrameSequenceRunning()) {
        auto history = getFrameSequenceForLegacyAdapter()->getHistory();
        getDisplayString().setTagArg("tt", 0, ("Fs: " + history).c_str());
    }
    else {
        getDisplayString().removeTag("tt");
    }
}

void Hcf::processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    if (packet == nullptr || header == nullptr)
        throw cRuntimeError("HCF ingress requires a packet and an IEEE 802.11 data or management header");
    Enter_Method("processUpperFrame(%s)", packet->getName());
    if (activeIngressPacket != nullptr)
        throw cRuntimeError(activeIngressPacket == packet ?
                "HCF ingress is already processing this packet" :
                "HCF ingress cannot process a foreign packet while another packet is active");
    if (packet->peekAtFront<Ieee80211DataOrMgmtHeader>().get() != header.get())
        throw cRuntimeError("HCF ingress header does not belong to the submitted packet");

    auto caller = dynamic_cast<cComponent *>(packet->getOwner());
    if (caller == nullptr)
        throw cRuntimeError("HCF ingress caller does not support packet ownership transfer");
    activeIngressPacket = packet;
    bool claimed = false;
    bool enqueueCommitted = false;
    try {
        if (caller != this)
            claimIngressPacket(packet);
        claimed = true;
        EV_INFO << "Processing upper frame: " << packet->getName() << endl;

        AccessCategory accessCategory;
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
        if (dynamicPtrCast<const Ieee80211MgmtHeader>(header))
            accessCategory = AC_VO;
        else if (dataHeader != nullptr) {
            // IEEE Std 802.11-2024, 10.23.2.1: EDCA maps UP/TID to one of the
            // four AC transmit queues and runs one EDCAF per AC.
            accessCategory = edca->classifyFrame(dataHeader);
        }
        else
            throw cRuntimeError("Unsupported HCF upper frame type");
        if (accessCategory < AC_BK || accessCategory >= AC_NUMCATEGORIES)
            throw cRuntimeError("Invalid HCF ingress access category %d", accessCategory);
        EV_INFO << "The upper frame has been classified as a "
                << printAccessCategory(accessCategory) << " frame." << endl;

        if (dataHeader != nullptr)
            tagMacSapServiceDataUnit(packet, dataHeader);
        queueing::IPacketQueue *queue = nullptr;
        if (dataHeader != nullptr && !header->getReceiverAddress().isMulticast() &&
                !header->getReceiverAddress().isBroadcast()) {
            queue = resolvePerStaQueue(header->getReceiverAddress(), accessCategory);
            if (!packet->findTag<OrigEnqueueTimeTag>())
                packet->addTagIfAbsent<OrigEnqueueTimeTag>()->setEnqueueTime(simTime());
        }
        if (queue == nullptr)
            queue = edca->getEdcaf(accessCategory)->getPendingQueue();
        if (queue == nullptr)
            throw cRuntimeError("HCF ingress queue resolution returned no queue");
        try {
            queue->enqueuePacket(packet);
        }
        catch (...) {
            for (int i = 0; i < queue->getNumPackets(); i++) {
                if (queue->getPacket(i) == packet) {
                    queue->removePacket(packet);
                    take(packet);
                    break;
                }
            }
            throw;
        }
        enqueueCommitted = true;
        if (hasFrameToTransmit(accessCategory) && edca->getChannelOwner() == nullptr &&
                !isFrameSequenceRunning()) {
            EV_DETAIL << "Requesting channel for access category "
                      << printAccessCategory(accessCategory) << endl;
            exchangeEngine->channelAccessRequested();
            edca->requestChannelAccess(accessCategory, this);
        }
        activeIngressPacket = nullptr;
    }
    catch (...) {
        if (claimed && !enqueueCommitted && caller != this)
            returnIngressPacketToCaller(packet, caller);
        activeIngressPacket = nullptr;
        throw;
    }
}

void Hcf::scheduleInactivityTimer(simtime_t timeout)
{
    Enter_Method("scheduleInactivityTimer");
    exchangeEngine->scheduleInactivityTimer(timeout, makeExchangeActions());
}

bool Hcf::processFeaturePhyIndication(Packet *packet)
{
    if (txRxInterceptor == nullptr)
        return false;
    auto result = txRxInterceptor->processPhyIndication(packet);
    IHcfTxRxInterceptor::validateResult(result);
    return result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON;
}

bool Hcf::isExpectingIntactAmpduResponse() const
{
    return isFrameSequenceRunning() &&
            dynamic_cast<const ReceiveCollectionStep *>(
                    exchangeEngine->getCurrentStep()) != nullptr;
}

void Hcf::processLowerFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    Enter_Method("processLowerFrame(%s)", packet->getName());
    take(packet);
    EV_INFO << "Processing lower frame: " << packet->getName() << endl;
    auto edcaf = edca->getChannelOwner();
    if (header == nullptr) {
        auto ndpIndication = packet->findTag<physicallayer::Ieee80211NdpInd>();
        if (ndpIndication != nullptr && packet->getDataLength() == b(0) &&
                (processHtHeaderlessNdpIndication(packet) ||
                 processFeaturePhyIndication(packet) ||
                 processHeaderlessNdpIndication(packet))) {
            handleDeferredStartRxTimeout();
            return;
        }
        auto rxVectorInd = packet->findTag<physicallayer::Ieee80211HeRxVectorInd>();
        auto recipientContext =
                packet->findTag<physicallayer::Ieee80211HeTbRecipientContextInd>();
        const bool intactAmpdu = packet->getDataLength() > b(0) &&
                dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                        packet->peekAtFront()) != nullptr &&
                (packet->findTag<physicallayer::Ieee80211MpduReceiveInd>() != nullptr ||
                        isHtImplicitBlockAckEnabled());
        const bool nfrpFeedbackNdp = rxVectorInd != nullptr &&
                rxVectorInd->getRxVector() != nullptr &&
                rxVectorInd->getRxVector()->getCommon().getPpduFormat() ==
                        physicallayer::HE_TRIGGER_BASED_UPLINK &&
                recipientContext != nullptr &&
                recipientContext->getRecipientParameters() != nullptr &&
                recipientContext->getRecipientParameters()->ndpFeedbackReport;
        auto receiveStep = isFrameSequenceRunning() ?
                dynamic_cast<IReceiveStep *>(const_cast<IFrameSequenceStep *>(
                        exchangeEngine->getCurrentStep())) : nullptr;
        if (intactAmpdu && receiveStep != nullptr && receiveStep->acceptsHeaderlessFrame(packet)) {
            processResponseAndCancelStartRxTimerIfCompleted(packet, receiveStep);
            handleDeferredStartRxTimeout();
            return;
        }
        if (intactAmpdu && receiveStep != nullptr && txRxInterceptor != nullptr) {
            auto result = txRxInterceptor->processRejectedHeaderlessResponse(packet,
                    receiveStep->getHeaderlessResponseFamily());
            IHcfTxRxInterceptor::validateResult(result);
            if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON) {
                handleDeferredStartRxTimeout();
                return;
            }
        }
        if (intactAmpdu) {
            recipientProcessReceivedFrame(packet, header);
            handleDeferredStartRxTimeout();
            return;
        }
        if (nfrpFeedbackNdp && receiveStep != nullptr && receiveStep->acceptsHeaderlessFrame(packet)) {
            // A feedback NDP has no MAC header and therefore cannot pass the
            // ordinary receiver-address test. Only the active NFRP collection
            // may opt into this path; it validates Trigger ID, timing,
            // tone-set, STS, and duplicate AID.
            exchangeEngine->processResponse(packet, makeExchangeActions());
            handleDeferredStartRxTimeout();
            return;
        }
        EV_INFO << "Discarding headerless PHY indication outside an active NFRP collection" << endl;
        delete packet;
        handleDeferredStartRxTimeout();
        return;
    }
    if (edcaf && isFrameSequenceRunning()) {
        // IEEE Std 802.11-2024, 10.23.2.2 plus 10.3.2.9/10.3.2.11:
        // EDCA treats the MPDU exchange as failed unless the timeout sees a
        // response from the expected recipient (or a control response without TA).
        // TODO always call processResponse?
        auto receiveStep = dynamic_cast<IReceiveStep *>(const_cast<IFrameSequenceStep *>(
                exchangeEngine->getCurrentStep()));
        exchangeEngine->processResponseAccordingToPolicy(packet, isForUs(header),
                receiveStep, makeExchangeActions());
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented!");
    else if (isForUs(header))
        recipientProcessReceivedFrame(packet, header);
    else {
        EV_INFO << "This frame is not for us" << std::endl;
        PacketDropDetails details;
        details.setReason(NOT_ADDRESSED_TO_US);
        emit(cComponent::registerSignal("packetDropped"), packet, &details);
        delete packet;
    }
    handleDeferredStartRxTimeout();
}

void Hcf::channelGranted(IChannelAccess *channelAccess)
{
    Enter_Method("channelGranted");
    auto edcaf = check_and_cast<Edcaf *>(channelAccess);
    if (edcaf) {
        if (isFrameSequenceRunning()) {
            EV_WARN << "Channel access granted while another EDCAF frame sequence is running. "
                    << "Releasing channel; queued traffic will resume after the current TXOP.\n";
            edcaf->releaseChannel(this);
            return;
        }
        if (tx->isBusy()) {
            EV_WARN << "Channel access granted to the " << printAccessCategory(edcaf->getAccessCategory())
                    << " queue while tx is busy (e.g. pending sequential Ack). Releasing channel.\n";
            edcaf->releaseChannel(this);
            return;
        }
        AccessCategory ac = edcaf->getAccessCategory();
        EV_DETAIL << "Channel access granted to the " << printAccessCategory(ac) << " queue" << std::endl;
        const bool hasEligibleFrame = hasFrameToTransmit(ac);
        auto internallyCollidedEdcafs = hasEligibleFrame ?
                edca->getInternallyCollidedEdcafs() : std::vector<Edcaf *>();
        if (internallyCollidedEdcafs.size() > 0) {
            EV_INFO << "Internal collision happened with the following queues:" << std::endl;
            handleInternalCollision(internallyCollidedEdcafs);
            emit(cComponent::registerSignal("edcaCollisionDetected"),
                    (unsigned long)internallyCollidedEdcafs.size());
        }
        if (hasEligibleFrame && shouldRestartWideChannelAccess(edcaf)) {
            edcaf->restartChannelAccess(this);
            return;
        }
        exchangeEngine->channelGranted();
        // IEEE Std 802.11-2024, 10.23.2.3 and 10.23.2.4: an EDCAF whose
        // backoff reaches zero obtains an EDCA TXOP for its primary AC.
        edcaf->getTxopProcedure()->startTxop(ac);
        if (heRuntime != nullptr) {
            heRuntime->startFrameSequence(ac);
            return;
        }
        if (vhtRuntime != nullptr) {
            vhtRuntime->startFrameSequence(ac);
            return;
        }
        if (!hasEligibleFrame) {
            releaseChannel(ac);
            return;
        }
        startFrameSequence(ac);
    }
    else
        throw cRuntimeError("Channel access granted but channel owner not found!");
}

void Hcf::releaseChannel(AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    exchangeEngine->preparationCompletedWithoutSequence(makeExchangeActions());
    edcaf->releaseChannel(this);
    edcaf->getTxopProcedure()->endTxop();
}


bool Hcf::shouldRestartWideChannelAccess(Edcaf *edcaf)
{
    auto packet = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (packet == nullptr)
        return false;
    auto header = packet->peekAtFront<Ieee80211MacHeader>();
    auto modeReq = packet->findTag<Ieee80211ModeReq>();
    auto mode = modeReq == nullptr ? rateSelection->computeMode(packet, header, edcaf->getTxopProcedure()) : modeReq->getMode();
    setFrameMode(packet, header, mode);
    Hz bandwidth = mode->getDataMode()->getBandwidth();
    if (modeSet == nullptr || bandwidth < MHz(40))
        return false;
    // IEEE Std 802.11-2024, 10.23.2.5 and 11.15.9: a PPDU of 40 MHz or wider
    // may start only if every secondary subchannel it occupies has been idle
    // throughout a PIFS (5/6 GHz) or DIFS (2.4 GHz) interval immediately
    // before backoff expiration; otherwise the EDCAF invokes backoff.
    simtime_t sifs = modeSet->getSifsTime();
    simtime_t slotTime = modeSet->getSlotTime();
    simtime_t interval = modeSet->getOperatingBand() == Ieee80211OperatingBand::BAND_2_4_GHZ ?
            sifs + 2 * slotTime : sifs + slotTime;
    return !rx->isWideChannelIdleFor(bandwidth, interval);
}

FrameSequenceContext *Hcf::buildContext(AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    auto qosContext = new QoSContext(originatorAckPolicy, originatorBlockAckProcedure.get(),
            originatorBlockAckAgreementHandler.get(), edcaf->getTxopProcedure(),
            rateSelection);
    auto htImplicitBlockAckFrames = getHtImplicitBlockAckFrames(edcaf);
    auto context = new FrameSequenceContext(mac->getAddress(), modeSet,
            edcaf->getInProgressFrames(), rtsProcedure.get(), rtsPolicy, nullptr,
            qosContext, isLegacyHtMultiTidBlockAckEnabled());
    context->setHtImplicitBlockAckFrames(htImplicitBlockAckFrames);
    return context;
}

std::vector<Packet *> Hcf::getHtImplicitBlockAckFrames(Edcaf *edcaf) const
{
    return htFeature->selectImplicitBlockAckFrames(edcaf, aggregationService,
            par("useImplicitBlockAck").boolValue());
}

int Hcf::getMaxAmpduLengthExponent(const MacAddress& peer,
        int defaultExponent, Ieee80211PhyFamily phyFamily) const
{
    int exponent = defaultExponent;
    auto mib = mac->getMib();
    if (mib != nullptr) {
        if (phyFamily == Ieee80211PhyFamily::HT) {
            auto negotiated = mib->getNegotiatedHtCapabilities(peer);
            if (negotiated && negotiated->localTxPeerRx.valid)
                exponent = negotiated->localTxPeerRx.receiverMaxAmpduLengthExponent;
        }
        else if (phyFamily == Ieee80211PhyFamily::VHT) {
            auto negotiated = mib->getNegotiatedVhtCapabilities(peer);
            if (negotiated && negotiated->localTxPeerRx.valid)
                exponent = negotiated->localTxPeerRx.receiverMaxAmpduLengthExponent;
        }
        else if (phyFamily == Ieee80211PhyFamily::HE ||
                phyFamily == Ieee80211PhyFamily::EHT) {
            auto negotiated = mib->getNegotiatedHeCapabilities(peer);
            if (negotiated && negotiated->localTxPeerRx.valid)
                exponent = negotiated->localTxPeerRx.receiverMaxAmpduLengthExponent;
        }
    }
    auto configuredExponent = originatorDataService->getMaxAmpduLengthExponent();
    return configuredExponent.has_value() ?
            std::min(exponent, *configuredExponent) : exponent;
}

bool Hcf::isLegacyHtMultiTidBlockAckEnabled() const
{
    // This INET extension models historical IEEE 802.11n Multi-TID BlockAck.
    // It is local, non-negotiated, and not a current 802.11 BA format, so it
    // must not affect VHT/HE/EHT operation.
    return par("useLegacyHtMultiTidBlockAck").boolValue() &&
            modeSet != nullptr &&
            Ieee80211ModeSet::isHtProfileName(modeSet->getName());
}

bool Hcf::isHtImplicitBlockAckEnabled() const
{
    return htFeature->isImplicitBlockAckEnabled(
            par("useImplicitBlockAck").boolValue());
}

TxopProcedure::InitialProtection Hcf::selectInitialProtection(Packet *frame,
        const physicallayer::IIeee80211Mode *firstMode) const
{
    if (frame == nullptr)
        return TxopProcedure::InitialProtection::NONE;
    auto header = frame->peekAtFront<Ieee80211MacHeader>();
    auto negotiatedHt = mac->getMib()->getNegotiatedHtCapabilities(
            header->getReceiverAddress());
    bool isHtMode = modeSet->getPhyFamily(firstMode) == Ieee80211PhyFamily::HT;
    auto protection = HtProtectionPolicy::select(isHtMode,
            header->getReceiverAddress(), negotiatedHt ? &*negotiatedHt : nullptr);
    return protection == HtProtectionPolicy::Protection::LEGACY_RTS_CTS ?
            TxopProcedure::InitialProtection::LEGACY_RTS_CTS :
            TxopProcedure::InitialProtection::NONE;
}

const physicallayer::IIeee80211Mode *Hcf::selectHtSoundingMode(
        AccessCategory ac) const
{
    auto edcaf = edca->getEdcaf(ac);
    auto txop = edcaf->getTxopProcedure();
    auto frameToTransmit = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (frameToTransmit == nullptr)
        return nullptr;
    auto frameHeader = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
    auto existingMode = frameToTransmit->findTag<physicallayer::Ieee80211ModeReq>();
    auto firstMode = existingMode == nullptr ?
            rateSelection->computeMode(frameToTransmit, frameHeader, txop) :
            existingMode->getMode();
    return htFeature->isSoundingEligible(frameHeader->getReceiverAddress(), firstMode) ?
            firstMode : nullptr;
}

bool Hcf::tryStartHtSounding(AccessCategory ac)
{
    auto firstMode = selectHtSoundingMode(ac);
    if (firstMode == nullptr)
        return false;
    auto edcaf = edca->getEdcaf(ac);
    auto txop = edcaf->getTxopProcedure();
    auto frameToTransmit = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (frameToTransmit == nullptr)
        return false;
    auto frameHeader = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
    auto sequence = htFeature->createSoundingSequence(
            frameHeader->getReceiverAddress(), firstMode);
    if (sequence == nullptr)
        return false;
    setFrameMode(frameToTransmit, frameHeader, firstMode);
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
    exchangeEngine->beginPreparation();
    startExchangeFrameSequence(sequence, buildContext(ac));
    return true;
}

void Hcf::startSingleUserExchange(AccessCategory ac)
{
    exchangeEngine->beginPreparation();
    auto edcaf = edca->getEdcaf(ac);
    auto txop = edcaf->getTxopProcedure();
    auto frameToTransmit = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (!txop->isProtectionConfigured()) {
        auto initialProtection = TxopProcedure::InitialProtection::NONE;
        if (frameToTransmit != nullptr) {
            auto frameHeader = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
            // Select the first actual PHY mode once. The request tag is reused by
            // transmitFrame, so protection does not cause a second rate decision.
            auto existingMode = frameToTransmit->findTag<physicallayer::Ieee80211ModeReq>();
            auto firstMode = existingMode == nullptr ?
                    rateSelection->computeMode(frameToTransmit, frameHeader, txop) :
                    existingMode->getMode();
            setFrameMode(frameToTransmit, frameHeader, firstMode);
            initialProtection = selectInitialProtection(frameToTransmit, firstMode);
        }
        // IEEE Std 802.11-2024, 10.23.2.4, 10.23.2.9, 10.23.2.11 and
        // 10.27.3: protection is immutable for this TXOP; the supported HT
        // subset performs one initial legacy RTS/CTS exchange.
        txop->configureProtection(initialProtection);
    }
    startExchangeFrameSequence(new HcfFs(), buildContext(ac));
}

void Hcf::startFrameSequence(AccessCategory ac)
{
    if (heRuntime != nullptr) {
        heRuntime->startFrameSequence(ac);
        return;
    }
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingFrameSequence()) {
        vhtRuntime->startFrameSequence(ac);
        return;
    }
    if (!hasFrameToTransmit(ac)) {
        releaseChannel(ac);
        return;
    }
    if (!tryStartHtSounding(ac))
        startSingleUserExchange(ac);
}

bool Hcf::processHeaderlessNdpIndication(Packet *packet)
{
    return vhtRuntime != nullptr &&
            vhtRuntime->processHeaderlessNdpIndication(packet);
}

void Hcf::resumeContention()
{
    if (testActionPort.resumeContention) {
        testActionPort.resumeContention();
        return;
    }
    for (int i = 0; i < 4; ++i) {
        AccessCategory ac = (AccessCategory)i;
        if (hasFrameToTransmit(ac)) {
            auto edcaf = edca->getEdcaf(ac);
            if (edcaf && !edcaf->isOwning()) {
                EV_DETAIL << "Resuming contention for access category " << printAccessCategory(ac) << std::endl;
                edca->requestChannelAccess(ac, this);
            }
        }
    }
}

void Hcf::handleEdcafInternalCollision(Edcaf *edcaf)
{
    AccessCategory ac = edcaf->getAccessCategory();
    Packet *internallyCollidedFrame = edcaf->getInProgressFrames()->getFrameToTransmit();
    auto internallyCollidedHeader = internallyCollidedFrame->peekAtFront<Ieee80211DataOrMgmtHeader>();
    EV_INFO << printAccessCategory(ac) << " (" << internallyCollidedFrame->getName() << ")" << endl;
    // IEEE Std 802.11-2024, 10.23.2.4: if two EDCAFs can initiate at the
    // same slot boundary, lower-priority ACs report internal collision and
    // invoke the backoff/retry update path from 10.23.2.2 item d).
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(internallyCollidedHeader))
        ASSERT(ac == AccessCategory::AC_BE);
    else if (!dynamicPtrCast<const Ieee80211DataHeader>(internallyCollidedHeader))
        throw cRuntimeError("Unknown frame");
    auto frame = HcfOriginatorService::makeFrame(internallyCollidedFrame,
            internallyCollidedHeader);
    HcfOriginatorActions actions(this, edcaf, ac);
    auto result = processHcfFailure(frame,
            HcfOriginatorService::FailureKind::ACK_TIMEOUT,
            HcfFailurePath::INTERNAL_COLLISION, actions);
    if (result.disposition != HcfOriginatorService::Disposition::STALE_OR_DUPLICATE &&
            (result.terminalAction != HcfOriginatorService::TerminalAction::DROP_RETIRE ||
                    hasFrameToTransmit(ac)))
        edcaf->requestChannel(this);
}

void Hcf::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
{
    if (heRuntime != nullptr) {
        heRuntime->handleInternalCollision(std::move(internallyCollidedEdcafs));
        return;
    }
    for (auto edcaf : internallyCollidedEdcafs)
        handleEdcafInternalCollision(edcaf);
}

/*
 * TODO  If a PHY-RXSTART.indication primitive does not occur during the ACKTimeout interval,
 * the STA concludes that the transmission of the MPDU has failed, and this STA shall invoke its
 * backoff procedure **upon expiration of the ACKTimeout interval**.
 */

void Hcf::frameSequenceFinished()
{
    if (testActionPort.frameSequenceFinished) {
        testActionPort.frameSequenceFinished();
        return;
    }
    Enter_Method("frameSequenceFinished");
    emit(cComponent::registerSignal("frameSequenceFinished"), getFrameSequenceContext());
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        // IEEE Std 802.11-2024, 10.23.2.8 and 10.23.2.9: when the TXOP ends,
        // the holder releases medium control; further traffic must contend
        // unless it is sent as another permitted sequence inside the same TXOP.
        edcaf->releaseChannel(this);
        mac->sendDownPendingRadioConfigMsg(); // TODO review
        edcaf->getTxopProcedure()->endTxop();
    }
    else if (hcca->isOwning()) {
        hcca->releaseChannel(this);
        mac->sendDownPendingRadioConfigMsg(); // TODO review
        throw cRuntimeError("Hcca is unimplemented!");
    }
    else
        throw cRuntimeError("Frame sequence finished but channel owner not found!");
    if (heRuntime != nullptr)
        heRuntime->frameSequenceCompleted();
}

void Hcf::recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (testActionPort.observeRecipientFrame)
        testActionPort.observeRecipientFrame(packet, header);
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingRecipientFrame()) {
        vhtRuntime->recipientProcessReceivedFrame(packet, header);
        return;
    }
    if (txRxInterceptor != nullptr) {
        auto result = txRxInterceptor->processRecipientFrame(packet, header);
        IHcfTxRxInterceptor::validateResult(result);
        if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON)
            return;
    }
    // Amendment-specific VHT/HE preprocessing remains in the virtual overrides.
    // Common HT preprocessing precedes recipient routing as it did before extraction.
    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
        htFeature->processReceivedMcsControl(packet, dataHeader);
    }

    HcfRecipientActions actions(this);
    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
            Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    bool isAmpdu = packet->getDataLength() > b(0) &&
            dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                    packet->peekAtFront(b(-1), parsingFlags)) != nullptr;
    if (isAmpdu)
        recipientService.processAmpdu(packet, isHtImplicitBlockAckEnabled() ?
                HcfRecipientService::AggregateResponsePolicy::HT_IMPLICIT_BLOCK_ACK :
                HcfRecipientService::AggregateResponsePolicy::ORDINARY, actions);
    else
        recipientService.processFrame(packet, header, actions);
}
void Hcf::recipientProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    // IEEE Std 802.11-2024, 10.3.2.9, 10.25.3, 10.25.5 and 10.25.6.
    HcfRecipientFrameDispatchActions actions(this);
    frameDispatchService.dispatchRecipientControl(packet, header, actions);
}

void Hcf::recipientProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header)
{
    // IEEE Std 802.11-2024, 10.25.2.
    HcfRecipientFrameDispatchActions actions(this);
    frameDispatchService.dispatchRecipientManagement(header, actions);
}

void Hcf::transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingTransmissionComplete()) {
        vhtRuntime->transmissionComplete(packet, header);
        return;
    }
    Enter_Method("transmissionComplete");
    if (txRxInterceptor != nullptr) {
        auto result = txRxInterceptor->processTransmissionComplete(packet, header);
        IHcfTxRxInterceptor::validateResult(result);
        if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON)
            return;
    }
    if (htFeature->processTransmissionComplete(packet, this)) {
        return;
    }
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        exchangeEngine->transmissionComplete(makeExchangeActions());
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented!");
    else
        recipientProcessTransmittedControlResponseFrame(packet, header);
}

void Hcf::originatorProcessRtsProtectionFailed(Packet *packet)
{
    if (testActionPort.originatorProcessRtsProtectionFailed) {
        testActionPort.originatorProcessRtsProtectionFailed(packet);
        return;
    }
    Enter_Method("originatorProcessRtsProtectionFailed");
    auto protectedHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        EV_INFO << "RTS frame transmission failed\n";
        // IEEE Std 802.11-2024, 10.3.2.9 and 10.23.2.2: a failed RTS/CTS
        // exchange invokes the EDCAF retry/backoff update for the protected
        // frame exchange.
        if (!dynamicPtrCast<const Ieee80211DataHeader>(protectedHeader) &&
                !dynamicPtrCast<const Ieee80211MgmtHeader>(protectedHeader))
            throw cRuntimeError("Unknown frame"); // TODO QoSDataFrame, NonQoSDataFrame
        auto frame = HcfOriginatorService::makeFrame(packet, protectedHeader);
        HcfOriginatorActions actions(this, edcaf, edcaf->getAccessCategory());
        processHcfFailure(frame,
                HcfOriginatorService::FailureKind::ACK_TIMEOUT,
                HcfFailurePath::RTS_PROTECTION, actions);
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

bool Hcf::processTransmittedAmpdu(Packet *packet, Edcaf *edcaf, AccessCategory ac)
{
    auto ampdu = aggregationService.takeTransmission(packet);
    if (!ampdu)
        return false;
    for (auto subframe : ampdu->subframes) {
        auto dataHeader = subframe->peekAtFront<Ieee80211DataHeader>();
        if (ampdu->implicitBlockAck) {
            edcaf->getAckHandler()->processTransmittedHtImplicitBlockAckFrame(
                    dataHeader);
            if (originatorBlockAckAgreementHandler)
                originatorBlockAckAgreementHandler->processTransmittedDataFrame(
                        subframe, dataHeader,
                        originatorBlockAckAgreementPolicy, this);
        }
        else
            originatorProcessTransmittedDataFrame(subframe, dataHeader, ac);
    }
    return true;
}

void Hcf::processDispatchedTransmittedData(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header,
        HcfOriginatorService::ExpectedResponse expectedResponse,
        Edcaf *edcaf, AccessCategory accessCategory)
{
    auto frame = HcfOriginatorService::makeFrame(packet, header);
    HcfOriginatorActions actions(this, edcaf, accessCategory);
    originatorService.processTransmitted(frame, expectedResponse, actions);
}

void Hcf::processDispatchedFailure(Packet *packet,
        const Ptr<const Ieee80211DataOrMgmtHeader>& header,
        HcfOriginatorService::FailureKind failureKind,
        Edcaf *edcaf, AccessCategory accessCategory)
{
    auto frame = HcfOriginatorService::makeFrame(packet, header);
    HcfOriginatorActions actions(this, edcaf, accessCategory);
    processHcfFailure(frame, failureKind, HcfFailurePath::ORIGINATOR, actions);
}

void Hcf::originatorProcessTransmittedFrame(Packet *packet)
{
    if (testActionPort.originatorProcessTransmittedFrame) {
        testActionPort.originatorProcessTransmittedFrame(packet);
        return;
    }
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingTransmittedFrame()) {
        vhtRuntime->originatorProcessTransmittedFrame(packet);
        return;
    }
    Enter_Method("originatorProcessTransmittedFrame");
    if (txRxInterceptor != nullptr) {
        auto result = txRxInterceptor->processTransmittedFrame(packet);
        IHcfTxRxInterceptor::validateResult(result);
        if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON)
            return;
    }
    EV_INFO << "Processing transmitted frame " << packet->getName() << " as originator in frame sequence.\n";
    if (HtHcfFeature::isSoundingTransmission(packet))
        return;
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        edcaf->emit(cComponent::registerSignal("packetSentToPeer"), packet);
        AccessCategory ac = edcaf->getAccessCategory();
        if (isHeMuContainerPacket(packet))
            return;
        if (processTransmittedAmpdu(packet, edcaf, ac))
            return;
        auto transmittedHeader = packet->peekAtFront<Ieee80211MacHeader>();
        // IEEE Std 802.11-2024, 10.3.2.11 and 10.25.3.
        HcfOriginatorFrameDispatchActions actions(this, edcaf, ac);
        frameDispatchService.dispatchTransmitted(packet, transmittedHeader, actions);
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented");
    else
        throw cRuntimeError("Frame transmitted but channel owner not found");
}

void Hcf::originatorProcessTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    HcfOriginatorFrameDispatchActions actions(this, edcaf, ac);
    frameDispatchService.dispatchTransmittedData(packet, dataHeader, actions);
}

void Hcf::originatorProcessTransmittedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& mgmtHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    HcfOriginatorFrameDispatchActions actions(this, edcaf, ac);
    frameDispatchService.dispatchTransmittedManagement(mgmtHeader, actions);
}

void Hcf::originatorProcessTransmittedControlFrame(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac)
{
    if (txRxInterceptor != nullptr) {
        auto result = txRxInterceptor->processTransmittedControl(controlHeader, ac);
        IHcfTxRxInterceptor::validateResult(result);
        if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON)
            return;
    }
    auto edcaf = edca->getEdcaf(ac);
    HcfOriginatorFrameDispatchActions actions(this, edcaf, ac);
    frameDispatchService.dispatchTransmittedControl(controlHeader, actions);
}

void Hcf::processFailedBlockAckReq(Edcaf *edcaf,
        const Ptr<const Ieee80211BlockAckReq>& blockAckReq,
        bool requireValidSequenceNumber)
{
    auto failedFrameIds = edcaf->getAckHandler()->processFailedBlockAckReq(blockAckReq);
    // IEEE Std 802.11-2024, 10.23.2.2 and 10.23.2.12.1: account each
    // exact MPDU whose expected BlockAck did not arrive before retrying it.
    HcfRetryService::recoverBlockAckRequestFailure(
            edcaf->getInProgressFrames(), edcaf->getRecoveryProcedure(),
            failedFrameIds, requireValidSequenceNumber);
}

void Hcf::originatorProcessFailedFrame(Packet *failedPacket)
{
    if (testActionPort.originatorProcessFailedFrame) {
        testActionPort.originatorProcessFailedFrame(failedPacket);
        return;
    }
    Enter_Method("originatorProcessFailedFrame");
    if (txRxInterceptor != nullptr) {
        auto result = txRxInterceptor->processFailedFrame(failedPacket);
        IHcfTxRxInterceptor::validateResult(result);
        if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON)
            return;
    }
    aggregationService.discardTransmission(failedPacket);
    auto failedHeader = failedPacket->peekAtFront<Ieee80211MacHeader>();
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader)) {
            ASSERT(dataHeader->getAckPolicy() == NORMAL_ACK || dataHeader->getAckPolicy() == BLOCK_ACK);
            if (isHtImplicitBlockAckEnabled() &&
                    dataHeader->getAckPolicy() == NORMAL_ACK &&
                    edcaf->getAckHandler()->getQoSDataAckStatus(dataHeader) ==
                            QosAckHandler::Status::WAITING_FOR_BLOCK_ACK) {
                auto failedFrameIds = edcaf->getAckHandler()->
                        processFailedHtImplicitBlockAck(
                                dataHeader->getReceiverAddress(),
                                dataHeader->getTid());
                auto retiredFrames = HcfRetryService::recoverHtImplicitBlockAckTimeout(
                        edcaf->getInProgressFrames(),
                        edcaf->getAckHandler(),
                        edcaf->getRecoveryProcedure(),
                        dataAndMgmtRateControl, failedFrameIds);
                for (auto frame : retiredFrames) {
                    PacketDropDetails details;
                    details.setReason(RETRY_LIMIT_REACHED);
                    details.setLimit(-1);
                    emit(cComponent::registerSignal("packetDropped"), frame, &details);
                    emit(cComponent::registerSignal("linkBroken"), frame);
                }
                return;
            }
        }
        HcfOriginatorFrameDispatchActions actions(this, edcaf,
                edcaf->getAccessCategory());
        frameDispatchService.dispatchOriginatorFailure(failedPacket,
                failedHeader, actions);
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

Hcf::HtAmpduAckContext Hcf::classifyHtAmpduAckContext(
        unsigned int numAggregateMembers,
        const std::vector<Ptr<const Ieee80211MacHeader>>& headers)
{
    return HtHcfFeature::classifyAmpduAckContext(numAggregateMembers, headers) ==
            HtHcfFeature::AmpduAckContext::IMPLICIT_BLOCK_ACK ?
            HtAmpduAckContext::IMPLICIT_BLOCK_ACK : HtAmpduAckContext::ORDINARY;
}

void Hcf::originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket)
{
    if (testActionPort.originatorProcessReceivedFrame) {
        testActionPort.originatorProcessReceivedFrame(receivedPacket,
                lastTransmittedPacket);
        return;
    }
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingReceivedFrame()) {
        vhtRuntime->originatorProcessReceivedFrame(receivedPacket, lastTransmittedPacket);
        return;
    }
    if (receivedPacket == nullptr)
        return;
    if (txRxInterceptor != nullptr) {
        auto result = txRxInterceptor->processReceivedResponse(receivedPacket, lastTransmittedPacket);
        IHcfTxRxInterceptor::validateResult(result);
        if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON)
            return;
    }
    // HT sounding feedback is an Action frame completing a headerless NDP
    // exchange.  The generic originator path expects the last transmission to
    // have a MAC header, which is not true for the HT-NDP packet.
    if (HtHcfFeature::isSoundingFeedback(receivedPacket))
        return;
    auto receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
    if (isHeMuContainerPacket(lastTransmittedPacket) && !dynamicPtrCast<const Ieee80211BlockAck>(receivedHeader))
        return;
    Enter_Method("originatorProcessReceivedFrame");
    EV_INFO << "Processing received frame " << receivedPacket->getName() << " as originator in frame sequence.\n";
    emit(cComponent::registerSignal("packetReceivedFromPeer"), receivedPacket);
    Ptr<const Ieee80211MacHeader> lastTransmittedHeader;
    if (auto metadata = lastTransmittedPacket->findTag<Ieee80211HeMuContainerReq>();
            metadata != nullptr) {
        auto metadataHeader = makeShared<Ieee80211DataHeader>();
        metadataHeader->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        metadataHeader->setDurationField(metadata->getDurationField());
        lastTransmittedHeader = metadataHeader;
    }
    else
        lastTransmittedHeader = lastTransmittedPacket->peekAtFront<Ieee80211MacHeader>();
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        HcfOriginatorFrameDispatchActions actions(this, edcaf, ac);
        frameDispatchService.dispatchOriginatorReceived(receivedPacket,
                receivedHeader, lastTransmittedPacket, lastTransmittedHeader,
                actions);
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

void Hcf::originatorProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    HcfOriginatorFrameDispatchActions actions(this, edcaf, ac);
    frameDispatchService.dispatchOriginatorReceivedManagement(header, actions);
}

void Hcf::processReceivedBlockAck(Edcaf *edcaf,
        const Ptr<const Ieee80211BlockAck>& blockAck, AccessCategory ac)
{
    EV_INFO << blockAck->getClassName() << " has arrived" << std::endl;
    std::vector<HcfOriginatorService::Frame> candidateFrames;
    for (int i = 0; i < edcaf->getInProgressFrames()->getLength(); i++) {
        auto packet = edcaf->getInProgressFrames()->getFrames(i);
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront<Ieee80211MacHeader>());
        if (dataHeader != nullptr)
            candidateFrames.push_back(HcfOriginatorService::makeFrame(packet,
                    dataHeader));
    }
    HcfOriginatorActions actions(this, edcaf, ac);
    originatorService.processBlockAckReceived(blockAck, candidateFrames, actions);
}

void Hcf::processReceivedAck(Edcaf *edcaf,
        const Ptr<const Ieee80211AckFrame>& ackFrame,
        Packet *lastTransmittedPacket,
        const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader)
{
    auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
            lastTransmittedHeader);
    if (dataOrMgmtHeader == nullptr)
        throw cRuntimeError("Unknown frame"); // TODO qos, nonqos frame
    auto frame = HcfOriginatorService::makeFrame(lastTransmittedPacket,
            dataOrMgmtHeader);
    HcfOriginatorActions actions(this, edcaf, edcaf->getAccessCategory());
    originatorService.processAckReceived(frame, ackFrame, actions);
}

void Hcf::originatorProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, Packet *lastTransmittedPacket, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    HcfOriginatorFrameDispatchActions actions(this, edcaf, ac);
    frameDispatchService.dispatchOriginatorReceivedControl(packet, header,
            lastTransmittedPacket, lastTransmittedHeader, actions);
}

void Hcf::originatorProcessReceivedDataFrame(const Ptr<const Ieee80211DataHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac)
{
    throw cRuntimeError("Unknown data frame");
}

static bool isPendingQueueEligible(queueing::IPacketQueue *pendingQueue, IOriginatorBlockAckAgreementHandler *baHandler)
{
    if (pendingQueue->isEmpty())
        return false;
    for (int i = 0; i < pendingQueue->getNumPackets(); i++) {
        auto packet = pendingQueue->getPacket(i);
        auto macHeader = packet->peekAtFront<Ieee80211MacHeader>();
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(macHeader);
        if (dataHeader != nullptr && isOriginatorBlockAckAgreementPending(baHandler, dataHeader->getReceiverAddress(), dataHeader->getTid()))
            continue;
        return true;
    }
    return false;
}

bool Hcf::hasCommonFrameToTransmit(AccessCategory ac) const
{
    if (testActionPort.hasCommonFrameToTransmit)
        return testActionPort.hasCommonFrameToTransmit(ac);
    if (auto twtManager = mac->getTwtManager(); twtManager != nullptr && !twtManager->isStationAwake())
        return false;
    auto edcaf = edca->getEdcaf(ac);
    if (edcaf)
        return isPendingQueueEligible(edcaf->getPendingQueue(), originatorBlockAckAgreementHandler.get()) || edcaf->getInProgressFrames()->hasInProgressFrames();
    else
        throw cRuntimeError("Hcca is unimplemented");
}

bool Hcf::hasFrameToTransmit(AccessCategory ac)
{
    return heRuntime != nullptr ? heRuntime->hasFrameToTransmit(ac) :
            hasCommonFrameToTransmit(ac);
}

bool Hcf::hasFrameToTransmit()
{
    if (heRuntime != nullptr)
        return heRuntime->hasFrameToTransmit();
    if (auto twtManager = mac->getTwtManager(); twtManager != nullptr && !twtManager->isStationAwake())
        return false;
    auto edcaf = edca->getChannelOwner();
    if (edcaf)
        return isPendingQueueEligible(edcaf->getPendingQueue(), originatorBlockAckAgreementHandler.get()) || edcaf->getInProgressFrames()->hasInProgressFrames();
    else
        throw cRuntimeError("Hcca is unimplemented");
}

void Hcf::legacyPreambleReceived(const Packet *packet)
{
    if (heRuntime != nullptr)
        heRuntime->legacyPreambleReceived(packet);
}

void Hcf::originatorProcessBlockAckResult(
        const Ptr<const Ieee80211BlockAck>& blockAck,
        const std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>>& ackedFrames,
        AccessCategory ac)
{
    if (testActionPort.observeOriginatorBlockAckResult)
        testActionPort.observeOriginatorBlockAckResult(blockAck, ackedFrames, ac);
    if (heRuntime != nullptr)
        heRuntime->originatorProcessBlockAckResult(blockAck, ackedFrames, ac);
}

void Hcf::twtServicePeriodChanged()
{
    Enter_Method("twtServicePeriodChanged");
    if (heRuntime != nullptr)
        heRuntime->getHePeerStateService().handleTwtBoundary();
    // Service-period changes can coincide with a TXOP or a response wait.
    // Queue eligibility changes are picked up when that frame sequence ends.
    if (isFrameSequenceRunning() || edca->getChannelOwner() != nullptr)
        return;
    for (int ac = AC_BK; ac <= AC_VO; ac++) {
        auto accessCategory = static_cast<AccessCategory>(ac);
        auto edcaf = edca->getEdcaf(accessCategory);
        if (edcaf != nullptr && hasFrameToTransmit(accessCategory))
            edca->requestChannelAccess(accessCategory, this);
    }
}

void Hcf::sendUp(const std::vector<Packet *>& completeFrames)
{
    for (auto frame : completeFrames)
        mac->sendUpFrame(frame);
}

void Hcf::transmitFrame(Packet *packet, simtime_t ifs)
{
    if (testActionPort.transmitFrame) {
        testActionPort.transmitFrame(packet, ifs);
        return;
    }
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingTransmitFrame()) {
        vhtRuntime->transmitFrame(packet, ifs);
        return;
    }
    Enter_Method("transmitFrame");
    if (htFeature->transmitNdpIfRequested(packet, ifs, this))
        return;
    if (txRxInterceptor != nullptr) {
        auto result = txRxInterceptor->processTransmitRequest(packet, ifs);
        IHcfTxRxInterceptor::validateResult(result);
        if (result.disposition != IHcfTxRxInterceptor::Disposition::CONTINUE_COMMON)
            return;
    }
    if (frameDecorator)
        frameDecorator(packet);
    auto channelOwner = edca->getChannelOwner();
    if (channelOwner == nullptr)
        throw cRuntimeError("Hcca is unimplemented");

    auto txop = channelOwner->getTxopProcedure();
    auto protection = txop->getProtectionMechanism();
    if (protection != TxopProcedure::ProtectionMechanism::SINGLE_PROTECTION &&
            protection != TxopProcedure::ProtectionMechanism::MULTIPLE_PROTECTION)
        throw cRuntimeError("Undefined protection mechanism");

    if (auto metadata = packet->findTag<Ieee80211HeMuContainerReq>();
            metadata != nullptr) {
        auto header = makeShared<Ieee80211DataHeader>();
        header->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        header->setType(ST_DATA_WITH_QOS);
        header->setDurationField(metadata->getDurationField());
        auto modeReq = packet->findTag<Ieee80211ModeReq>();
        if (modeReq == nullptr)
            rateSelection->computeMode(packet, header, txop);
        tx->transmitFrame(packet, header, ifs, this);
        return;
    }

    auto header = packet->peekAtFront<Ieee80211MacHeader>();
    HcfTransmissionPreparationService::Request request;
    request.packet = packet;
    request.header = header;
    request.ifs = ifs;
    request.protectionMechanism = protection ==
            TxopProcedure::ProtectionMechanism::SINGLE_PROTECTION ?
            HcfTransmissionPreparationService::ProtectionMechanism::SINGLE :
            HcfTransmissionPreparationService::ProtectionMechanism::MULTIPLE;
    request.durationFinalized = packet->findTag<DurationFinalizedReq>() != nullptr;
    request.durationExemptForSingleProtection =
            dynamicPtrCast<const Ieee80211TriggerFrame>(header) != nullptr ||
            dynamicPtrCast<const Ieee80211MultiStaBlockAck>(header) != nullptr;
    TransmissionPreparationActions actions(this, channelOwner, txop,
            getFrameSequenceContext(), request.protectionMechanism);
    transmissionPreparationService.prepareAndTransmit(request, actions);
}

void Hcf::transmitControlResponseFrame(Packet *responsePacket, const Ptr<const Ieee80211MacHeader>& responseHeader, Packet *receivedPacket, const Ptr<const Ieee80211MacHeader>& receivedHeader)
{
    Enter_Method("transmitControlResponseFrame");
    responsePacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    HcfRecipientFrameDispatchActions actions(this);
    auto responseMode = frameDispatchService.selectImmediateResponseMode(
            responsePacket, responseHeader, receivedPacket, receivedHeader,
            actions);
    // IEEE Std 802.11-2024, 10.3.2.9, 10.3.2.11 and 10.25.3: CTS, Ack and
    // BlockAck immediate responses are transmitted after SIFS.
    setFrameMode(responsePacket, responseHeader, responseMode);
    recordSelectedMode(responsePacket, responseMode);
    emit(cComponent::registerSignal("datarateSelected"),
            responseMode->getDataMode()->getNetBitrate().get<bps>(), responsePacket);
    EV_DEBUG << "Datarate for " << responsePacket->getName() << " is set to " << responseMode->getDataMode()->getNetBitrate() << ".\n";
    tx->transmitFrame(responsePacket, responseHeader, modeSet->getSifsTime(), this);
    delete responsePacket;
}

void Hcf::recipientProcessTransmittedControlResponseFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    emit(cComponent::registerSignal("packetSentToPeer"), packet);
    HcfRecipientFrameDispatchActions actions(this);
    frameDispatchService.dispatchTransmittedControlResponse(header, actions);
    resumeContention();
}

void Hcf::processMgmtFrame(Packet *mgmtPacket, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader)
{
    Enter_Method("processMgmtFrame");
    mgmtPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    processUpperFrame(mgmtPacket, mgmtHeader);
}

void Hcf::setFrameMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, const IIeee80211Mode *mode) const
{
    if (testActionPort.observeSetFrameMode)
        testActionPort.observeSetFrameMode(packet, header, mode);
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingSetFrameMode()) {
        vhtRuntime->setFrameMode(packet, header, mode);
        return;
    }
    ASSERT(mode != nullptr);
    packet->addTagIfAbsent<Ieee80211ModeReq>()->setMode(mode);
}

bool Hcf::isReceptionInProgress()
{
    if (testActionPort.isReceptionInProgress)
        return testActionPort.isReceptionInProgress();
    return rx->isReceptionInProgress();
}

bool Hcf::isForUs(const Ptr<const Ieee80211MacHeader>& header) const
{
    auto roles = interpretIeee80211AddressRoles(header);
    return roles.receiverAddress == mac->getAddress() || (roles.receiverAddress.isMulticast() && !isSentByUs(header));
}

bool Hcf::isSentByUs(const Ptr<const Ieee80211MacHeader>& header) const
{
    auto roles = interpretIeee80211AddressRoles(header);
    return roles.hasTransmitterAddress && roles.transmitterAddress == mac->getAddress();
}

void Hcf::corruptedFrameReceived()
{
    Enter_Method("corruptedFrameReceived");
    if (!exchangeEngine->handleCorruptedFrame(makeExchangeActions()))
        EV_DEBUG << "Ignoring received corrupt frame.\n";
}

Hcf::Hcf() = default;

Hcf::~Hcf()
{
    heRuntime.reset();
    vhtRuntime.reset();
    // Drop all non-owning feature/provider references while the NED-owned
    // feature-set submodule hierarchy is still alive.
    if (exchangeEngine != nullptr)
        exchangeEngine->cancelTimers(makeExchangeActions());
}

HeHcfRuntime& Hcf::getHeRuntime() const
{
    if (heRuntime == nullptr)
        throw cRuntimeError("HE runtime is not configured");
    return *heRuntime;
}

void Hcf::setVhtDlMuTxOpFactoryForTesting(
        VhtHcfFeature::ITxOpFactory *factory)
{
    if (vhtRuntime == nullptr)
        throw cRuntimeError("VHT runtime is not configured");
    vhtRuntime->setTxOpFactoryForTesting(factory);
}

queueing::IPacketQueue *Hcf::getPerStaQueue(const MacAddress& staAddr, AccessCategory ac)
{
    if (heRuntime != nullptr)
        return heRuntime->getPerStaQueue(staAddr, ac);
    return edca->getEdcaf(ac)->getPendingQueue();
}

void Hcf::invalidatePeerDerivedState(const MacAddress& peer)
{
    htFeature->invalidatePeer(peer);
    if (vhtRuntime != nullptr)
        vhtRuntime->invalidatePeer(peer);
    if (heRuntime != nullptr)
        heRuntime->invalidatePeerDerivedState(peer);
}

} // namespace ieee80211
} // namespace inet
