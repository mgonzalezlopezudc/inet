//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_HCF_H
#define __INET_HCF_H

#include <memory>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "inet/linklayer/ieee80211/mac/common/ModeSetListener.h"
#include "inet/linklayer/ieee80211/mac/contract/IChannelAccess.h"
#include "inet/linklayer/ieee80211/mac/contract/Ieee80211MgmtExchangeResult.h"
#include "inet/linklayer/ieee80211/mac/contract/IBlockAckAgreementHandlerCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosCoordinationFunction.h"
#include "inet/linklayer/ieee80211/mac/contract/ICtsPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/ICtsProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/contract/IRx.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorMacDataService.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorQoSAckPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IRtsPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IProcedureCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckAgreementPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientQosAckPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/contract/IRtsProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/linklayer/ieee80211/mac/contract/IHcfTxRxInterceptor.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfAggregationService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFrameDispatchService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtHcfFeature.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfMacSapTracker.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfOriginatorService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfRecipientService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfTransmissionPreparationService.h"
#include "inet/linklayer/ieee80211/mac/originator/TxopProcedure.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.h"

namespace inet {
namespace ieee80211 {

class Ieee80211Mac;
class StationQueueBank;
class Edca;
class Edcaf;
class Hcca;
class HcfOriginatorActions;
class HcfVhtRuntime;
class HeHcfRuntime;
class HcfRecipientActions;
class HcfRecipientFrameDispatchActions;
class HcfOriginatorFrameDispatchActions;
class FrameSequenceContext;
class InProgressFrames;
class SingleProtectionMechanism;

/**
 * Implements IEEE 802.11 Hybrid Coordination Function.
 */
class INET_API Hcf : public IQosCoordinationFunction, public IChannelAccess::ICallback, public ITx::ICallback, public IProcedureCallback, public IBlockAckAgreementHandlerCallback, public ModeSetListener
{
  public:
    struct RuntimeBindings {
        Ieee80211Mac *mac = nullptr;
        IRx *rx = nullptr;
        ITx *tx = nullptr;
        IQosRateSelection *rateSelection = nullptr;
        Edca *edca = nullptr;
        IOriginatorMacDataService *originatorDataService = nullptr;
        IRecipientQosMacDataService *recipientDataService = nullptr;
        IRateControl *dataAndMgmtRateControl = nullptr;
        IOriginatorQoSAckPolicy *originatorAckPolicy = nullptr;
        IRecipientQosAckPolicy *recipientAckPolicy = nullptr;
        IRtsPolicy *rtsPolicy = nullptr;
        SingleProtectionMechanism *singleProtectionMechanism = nullptr;
        IIeee80211MgmtExchangeResultHandler *mgmtExchangeResultHandler = nullptr;
    };

    struct TestActionPort {
        std::function<const FrameSequenceContext *()> getFrameSequenceContext;
        std::function<bool()> isReceptionInProgress;
        std::function<void()> resumeContention;
        std::function<void(Packet *, simtime_t)> transmitFrame;
        std::function<void(Packet *)> originatorProcessRtsProtectionFailed;
        std::function<void(Packet *)> originatorProcessTransmittedFrame;
        std::function<void(Packet *, Packet *)> originatorProcessReceivedFrame;
        std::function<void(Packet *)> originatorProcessFailedFrame;
        std::function<void()> frameSequenceFinished;
        std::function<void(Packet *, const Ptr<const Ieee80211MacHeader>&)>
                observeRecipientFrame;
        std::function<queueing::IPacketQueue *(const MacAddress&,
                AccessCategory)> resolvePerStaQueue;
        std::function<bool(AccessCategory)> hasCommonFrameToTransmit;
        std::function<void(Packet *, const Ptr<const Ieee80211MacHeader>&,
                const physicallayer::IIeee80211Mode *)> observeSetFrameMode;
        std::function<void(Packet *, const physicallayer::IIeee80211Mode *)> recordSelectedMode;
        std::function<void(const Ptr<const Ieee80211BlockAck>&,
                const std::set<std::pair<MacAddress,
                        std::pair<Tid, SequenceControlField>>>&,
                AccessCategory)> observeOriginatorBlockAckResult;
    };


    static simsignal_t edcaCollisionDetectedSignal;
    static simsignal_t blockAckAgreementAddedSignal;
    static simsignal_t blockAckAgreementDeletedSignal;
    static simsignal_t ampduCreatedSignal;
    static simsignal_t ampduNumMpdusSignal;

  private:
    class TransmissionPreparationActions;

    friend class HcfOriginatorActions;
    friend class HcfRecipientActions;
    friend class HcfRecipientFrameDispatchActions;
    friend class HcfOriginatorFrameDispatchActions;
    friend class HcfVhtRuntime;
    friend class HeHcfRuntime;
    friend class HeHcf;

    void handleBlockAckInactivityTimeout();
    void processDispatchedTransmittedData(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header,
            HcfOriginatorService::ExpectedResponse expectedResponse,
            Edcaf *edcaf, AccessCategory accessCategory);
    void processDispatchedFailure(Packet *packet,
            const Ptr<const Ieee80211DataOrMgmtHeader>& header,
            HcfOriginatorService::FailureKind failureKind,
            Edcaf *edcaf, AccessCategory accessCategory);
    void claimIngressPacket(Packet *packet);
    void returnIngressPacketToCaller(Packet *packet, cComponent *caller) noexcept;
    bool hasCommonFrameToTransmit(AccessCategory accessCategory) const;

  private:
    enum class HtAmpduAckContext {
        ORDINARY,
        IMPLICIT_BLOCK_ACK,
    };

    Ieee80211Mac *mac = nullptr;
    IRateControl *dataAndMgmtRateControl = nullptr;
    std::unique_ptr<HtHcfFeature> htFeature = std::make_unique<HtHcfFeature>();

    // Transmission and Reception
    IRx *rx = nullptr;
    ITx *tx = nullptr;

    IQosRateSelection *rateSelection = nullptr;

    // Channel Access Methods
    Edca *edca = nullptr;
    Hcca *hcca = nullptr;

    // MAC Data Service
    IOriginatorMacDataService *originatorDataService = nullptr;
    IRecipientQosMacDataService *recipientDataService = nullptr;

    // MAC Procedures
    std::unique_ptr<IRecipientAckProcedure> recipientAckProcedure;
    IOriginatorQoSAckPolicy *originatorAckPolicy = nullptr;
    IRecipientQosAckPolicy *recipientAckPolicy = nullptr;
    std::unique_ptr<IRtsProcedure> rtsProcedure;
    IRtsPolicy *rtsPolicy = nullptr;
    std::unique_ptr<ICtsProcedure> ctsProcedure;
    ICtsPolicy *ctsPolicy = nullptr;
    std::unique_ptr<IOriginatorBlockAckProcedure> originatorBlockAckProcedure;
    std::unique_ptr<IRecipientBlockAckProcedure> recipientBlockAckProcedure;

    // Block Ack Agreement Handlers
    std::unique_ptr<IOriginatorBlockAckAgreementHandler> originatorBlockAckAgreementHandler;
    IOriginatorBlockAckAgreementPolicy *originatorBlockAckAgreementPolicy = nullptr;
    std::unique_ptr<IRecipientBlockAckAgreementHandler> recipientBlockAckAgreementHandler;
    IRecipientBlockAckAgreementPolicy *recipientBlockAckAgreementPolicy = nullptr;

    // Tx Opportunity
    TxopProcedure *hccaTxop = nullptr;

    // Queues
    InProgressFrames *hccaInProgressFrame = nullptr;

    // Active HCF exchange, including frame-sequence and timer ownership
    std::unique_ptr<HcfExchangeEngine> exchangeEngine;

    // Protection mechanisms
    SingleProtectionMechanism *singleProtectionMechanism = nullptr;

    std::string lastSelectedModePacketName;
    std::string lastSelectedModeName;
    double lastSelectedModeNetBitrate = -1;
    double lastSelectedModeBandwidth = -1;
    int lastSelectedModeNumSpatialStreams = -1;
    HcfAggregationService aggregationService;
    Packet *activeIngressPacket = nullptr;
    HcfMacSapTracker macSapTracker;
    HcfOriginatorService originatorService;
    HcfRecipientService recipientService;
    HcfFrameDispatchService frameDispatchService;
    HcfTransmissionPreparationService transmissionPreparationService;
    std::unique_ptr<HcfVhtRuntime> vhtRuntime;
    std::unique_ptr<HeHcfRuntime> heRuntime;
    IIeee80211MgmtExchangeResultHandler *mgmtExchangeResultHandler = nullptr;
    IHcfTxRxInterceptor *txRxInterceptor = nullptr;
    std::function<void(Packet *)> frameDecorator;
    TestActionPort testActionPort;

    void notifyMgmtExchangeResult(Packet *packet,
            Ieee80211MgmtExchangeResultKind kind);

  public:
    void installRuntimeBindingsForTesting(const RuntimeBindings& bindings)
    {
        if (bindings.mac != nullptr) mac = bindings.mac;
        if (bindings.rx != nullptr) rx = bindings.rx;
        if (bindings.tx != nullptr) tx = bindings.tx;
        if (bindings.rateSelection != nullptr) rateSelection = bindings.rateSelection;
        if (bindings.edca != nullptr) edca = bindings.edca;
        if (bindings.originatorDataService != nullptr) originatorDataService = bindings.originatorDataService;
        if (bindings.recipientDataService != nullptr) recipientDataService = bindings.recipientDataService;
        if (bindings.dataAndMgmtRateControl != nullptr) dataAndMgmtRateControl = bindings.dataAndMgmtRateControl;
        if (bindings.originatorAckPolicy != nullptr) originatorAckPolicy = bindings.originatorAckPolicy;
        if (bindings.recipientAckPolicy != nullptr) recipientAckPolicy = bindings.recipientAckPolicy;
        if (bindings.rtsPolicy != nullptr) rtsPolicy = bindings.rtsPolicy;
        if (bindings.singleProtectionMechanism != nullptr) singleProtectionMechanism = bindings.singleProtectionMechanism;
        if (bindings.mgmtExchangeResultHandler != nullptr) mgmtExchangeResultHandler = bindings.mgmtExchangeResultHandler;
    }
    void installExchangeEngineForTesting(std::unique_ptr<HcfExchangeEngine> engine)
    {
        exchangeEngine = std::move(engine);
    }
    void configureHtFeatureForTesting(
            physicallayer::Ieee80211ModeSet *newModeSet)
    {
        modeSet = newModeSet;
        htFeature->configure(mac, [this]() { return modeSet; }, nullptr, tx,
                rateSelection, [](const MacAddress&, int exponent,
                        physicallayer::Ieee80211PhyFamily) { return exponent; },
                nullptr, originatorAckPolicy, false, 2,
                Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING,
                SIMTIME_ZERO);
    }
    Ieee80211Mac *getMacForTesting() const { return mac; }
    Edca *getEdcaForTesting() const { return edca; }
    static bool isHtImplicitBlockAckAggregateForTesting(
            unsigned int numAggregateMembers,
            const std::vector<Ptr<const Ieee80211MacHeader>>& headers)
    {
        return classifyHtAmpduAckContext(numAggregateMembers, headers) ==
                HtAmpduAckContext::IMPLICIT_BLOCK_ACK;
    }
    IFrameSequence *createHtSoundingSequenceForTesting(
            const MacAddress& peer,
            const physicallayer::IIeee80211Mode *mode)
    {
        return htFeature->createSoundingSequence(peer, mode);
    }
    void completeExchangeTransmissionForTesting()
    {
        exchangeEngine->transmissionComplete(makeExchangeActions());
    }
    void processExchangeResponseForTesting(Packet *packet)
    {
        exchangeEngine->processResponse(packet, makeExchangeActions());
    }
    void installRecipientBlockAckHandlerForTesting(
            std::unique_ptr<IRecipientBlockAckAgreementHandler> handler);
    void installOriginatorBlockAckHandlerForTesting(
            std::unique_ptr<IOriginatorBlockAckAgreementHandler> handler);
    void processRecipientBlockAckRequestForTesting(Packet *packet,
            const Ptr<const Ieee80211MultiTidBlockAckReq>& header,
            MultiTidBlockAckResponseFormat responseFormat,
            uint16_t responseAid);
    void installTestActionPort(TestActionPort port)
        { testActionPort = std::move(port); }

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID,
            cObject *obj, cObject *details) override;
    virtual void forEachChild(cVisitor *v) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
    HcfExchangeEngine::Actions makeExchangeActions();
    void handleDeferredStartRxTimeout();
    void processResponseAndCancelStartRxTimerIfCompleted(Packet *packet, IReceiveStep *receiveStep);
    void replaceFrameSequenceHandler(std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler);
    void startExchangeFrameSequence(IFrameSequence *frameSequence, FrameSequenceContext *context);
    bool isFrameSequenceRunning() const;
    const FrameSequenceContext *getFrameSequenceContext() const;
    const IFrameSequence *getFrameSequenceForLegacyAdapter() const;
    IFrameSequenceHandler::ICallback *getFrameSequenceCallbackForLegacyAdapter() const;
    cMessage *getStartRxTimerForTest() const;
    bool hasDeferredStartRxTimeoutForTest() const;
    void clearExchangeTimerStateForTest();
    static HtAmpduAckContext classifyHtAmpduAckContext(
            unsigned int numAggregateMembers,
            const std::vector<Ptr<const Ieee80211MacHeader>>& headers);
    bool processTransmittedAmpdu(Packet *packet, Edcaf *edcaf, AccessCategory ac);
    TxopProcedure::InitialProtection selectInitialProtection(Packet *frame,
            const physicallayer::IIeee80211Mode *firstMode) const;
    void releaseChannel(AccessCategory ac);
    const physicallayer::IIeee80211Mode *selectHtSoundingMode(AccessCategory ac) const;
    bool tryStartHtSounding(AccessCategory ac);
    void startSingleUserExchange(AccessCategory ac);

    void startFrameSequence(AccessCategory ac);
    void resumeContention();
    void handleEdcafInternalCollision(Edcaf *edcaf);
    void handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs);

    void sendUp(const std::vector<Packet *>& completeFrames);
    FrameSequenceContext *buildContext(AccessCategory ac);
    std::vector<Packet *> getHtImplicitBlockAckFrames(Edcaf *edcaf) const;
    int getMaxAmpduLengthExponent(const MacAddress& peer, int defaultExponent,
            physicallayer::Ieee80211PhyFamily phyFamily) const;
    bool hasFrameToTransmit();
    bool hasFrameToTransmit(AccessCategory ac);
    bool isReceptionInProgress();
    bool shouldRestartWideChannelAccess(Edcaf *edcaf);
    bool isLegacyHtMultiTidBlockAckEnabled() const;
    bool processHeaderlessNdpIndication(Packet *packet);
    bool processFeaturePhyIndication(Packet *packet);
    bool processHtHeaderlessNdpIndication(Packet *packet)
        { return htFeature->processHeaderlessNdpIndication(packet, this); }
    bool processHtNdpAnnouncement(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header)
        { return htFeature->processNdpAnnouncement(packet, header); }
    void sendStandaloneHtMfb()
        { htFeature->sendStandaloneMfbForCompatibility(this); }
    void attachPendingHtMcsControl(Packet *packet,
            const physicallayer::IIeee80211Mode *mode)
        { htFeature->attachPendingMcsControl(packet, mode); }
    HtSoundingPendingState::Snapshot getPendingHtSoundingSnapshot() const
        { return htFeature->getPendingSoundingSnapshot(); }
    void setPendingHtMfb(const MacAddress& peer, const Ieee80211HtMcsControl& control)
        { htFeature->setPendingMfb(peer, control); }
    void setHtTransmitterForCompatibility(ITx *value)
        { htFeature->setTransmitterForCompatibility(value); }

    // Recipient
    void recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);
    void recipientProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);
    void recipientProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header);
    void recipientProcessTransmittedControlResponseFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);

    // Originator
    void originatorProcessTransmittedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& mgmtHeader, AccessCategory ac);
    void originatorProcessTransmittedControlFrame(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac);
    void originatorProcessTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, AccessCategory ac);
    void originatorProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac);
    void originatorProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, Packet *lastTransmittedPacket, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac);
    void processReceivedAck(Edcaf *edcaf,
            const Ptr<const Ieee80211AckFrame>& ackFrame,
            Packet *lastTransmittedPacket,
            const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader);
    void processReceivedBlockAck(Edcaf *edcaf,
            const Ptr<const Ieee80211BlockAck>& blockAck, AccessCategory ac);
    void processFailedBlockAckReq(Edcaf *edcaf,
            const Ptr<const Ieee80211BlockAckReq>& blockAckReq,
            bool requireValidSequenceNumber);
    void originatorProcessReceivedDataFrame(const Ptr<const Ieee80211DataHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac);
    void originatorProcessBlockAckResult(
            const Ptr<const Ieee80211BlockAck>& blockAck,
            const std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>>& ackedFrames,
            AccessCategory ac);

    void setFrameMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, const physicallayer::IIeee80211Mode *mode) const;
    void recordSelectedMode(Packet *packet, const physicallayer::IIeee80211Mode *mode);
    bool isSentByUs(const Ptr<const Ieee80211MacHeader>& header) const;
    bool isForUs(const Ptr<const Ieee80211MacHeader>& header) const;

    static constexpr uint32_t MAX_FINITE_BUFFER_STATUS_BYTES =
            HcfMacSapTracker::MAX_FINITE_BUFFER_STATUS_BYTES;
    static void addBufferedTrafficServiceBytes(uint32_t& total, uint64_t amount);
    uint64_t allocateServiceDataUnitId();
    void tagMacSapServiceDataUnit(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header);
    uint32_t calculateBufferedTrafficServiceBytes(Edcaf *edcaf, const MacAddress& peer,
            int tid, const std::vector<Packet *>& additionalPackets) const;
    uint32_t getBufferedTrafficServiceBytes(
            Edcaf *edcaf, const MacAddress& peer, int tid = -1) const;

    // Per-STA queue bank routing (HE OFDMA scheduling)
    queueing::IPacketQueue *getPerStaQueue(const MacAddress& staAddr, AccessCategory ac);

  protected:
    // Call-scoped actions invoked by HcfExchangeEngine
    void originatorProcessRtsProtectionFailed(Packet *packet);
    void originatorProcessTransmittedFrame(Packet *packet);
    void originatorProcessReceivedFrame(Packet *packet, Packet *lastTransmittedPacket);
    void originatorProcessFailedFrame(Packet *packet);
    void frameSequenceFinished();
    void transmitFrame(Packet *packet, simtime_t ifs);

    // IChannelAccess::ICallback
    virtual void channelGranted(IChannelAccess *channelAccess) override;

    // ITx::ICallback
    virtual void transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;

    // IProcedureCallback
    virtual void transmitControlResponseFrame(Packet *responsePacket, const Ptr<const Ieee80211MacHeader>& responseHeader, Packet *receivedPacket, const Ptr<const Ieee80211MacHeader>& receivedHeader) override;
    virtual void processMgmtFrame(Packet *mgmtPacket, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader) override;

    // IProcedureCallback
    virtual void scheduleInactivityTimer(simtime_t timeout) override;

    std::string getFrameSequenceInfo() const;

  public:
    Hcf();
    virtual ~Hcf();

    HeHcfRuntime& getHeRuntime() const;
    void setVhtDlMuTxOpFactoryForTesting(VhtHcfFeature::ITxOpFactory *factory);

    virtual void setMgmtExchangeResultHandler(IIeee80211MgmtExchangeResultHandler *handler) override { mgmtExchangeResultHandler = handler; }

    /**
     * Returns true when the local, non-negotiated HT implicit Block Ack
     * feature is enabled. Ieee80211Mac uses this at the deaggregation boundary
     * so HCF can inspect the complete A-MPDU.
     */
    virtual bool isHtImplicitBlockAckEnabled() const;

    virtual bool isAllowedToProcessIntactHtAmpdu() const override { return isHtImplicitBlockAckEnabled(); }
    virtual bool isExpectingIntactAmpduResponse() const override;
    virtual void legacyPreambleReceived(const Packet *packet) override;

    IOriginatorMacDataService *getOriginatorMacDataService() const { return originatorDataService; }
    IOriginatorBlockAckAgreementHandler *getOriginatorBlockAckAgreementHandler() const { return originatorBlockAckAgreementHandler.get(); }
    virtual void invalidatePeerDerivedState(const MacAddress& peer) override;
    queueing::IPacketQueue *resolvePerStaQueue(const MacAddress& staAddr, AccessCategory ac) {
      return testActionPort.resolvePerStaQueue ?
              testActionPort.resolvePerStaQueue(staAddr, ac) :
              getPerStaQueue(staAddr, ac);
    }
    virtual void twtServicePeriodChanged() override;

    void registerTxRxInterceptor(IHcfTxRxInterceptor *interceptor) { txRxInterceptor = interceptor; }
    void registerFrameDecorator(std::function<void(Packet *)> decorator) { frameDecorator = std::move(decorator); }

    // ICoordinationFunction
    virtual void processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header) override;
    virtual void processLowerFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void corruptedFrameReceived() override;
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
