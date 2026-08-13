//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEHCFRUNTIME_H
#define __INET_HEHCFRUNTIME_H

#include <functional>
#include <memory>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeFrameDecorationPolicy.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfTxRxInterceptor.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTxopCoordinatorService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlTriggerService.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h"
#include "inet/linklayer/ieee80211/mac/contract/IHeDlMuExchangeCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IHeDlMuSnapshotSource.h"

namespace inet {
namespace ieee80211 {

class Edca;
class HeUlCoordinator;
class Ieee80211Mac;
class IIeee80211HeDlScheduler;
class IOriginatorBlockAckAgreementHandler;
class IOriginatorBlockAckAgreementPolicy;

/** HCF-owned HE service lifecycle and immutable link/PHY projection. */
class INET_API HeHcfRuntime : public HeDlMuExchangeProvider::IActions,
        public IHeDlMuSnapshotSource, public HeUlTriggerService::IActions,
        public IHeUlMuExchangeCallback, public IHeUlMuSnapshotSource,
        public HeTriggeredUlExchangeService::IActions,
        public HeSoundingService::IActions,
        public HeHcfTxRxInterceptor::IActions
{
  public:
    struct Bindings {
        Hcf *owner = nullptr;
        Ieee80211Mac *mac = nullptr;
        Edca *edca = nullptr;
        IIeee80211HeDlScheduler *dlScheduler = nullptr;
        HeUlCoordinator *ulCoordinator = nullptr;
        IOriginatorBlockAckAgreementHandler *blockAckHandler = nullptr;
        IOriginatorBlockAckAgreementPolicy *blockAckPolicy = nullptr;
        std::function<bool()> isFrameSequenceRunning;
        std::function<void(const MacAddress&)> invalidateBasePeer;
    };

  private:
    HcfHeRuntimeServices services;
    Hcf *hcf;
    Bindings bindings;
    std::unique_ptr<IIeee80211HeLinkPhyContext> linkPhyContext;
    bool started = false;
    HeTxopCoordinatorService txopCoordinator;
    IIeee80211HeDlScheduler *dlScheduler = nullptr;
    HeUlCoordinator *ulCoordinator = nullptr;
    HeUlTriggerService ulTriggerService;
    HeFrameDecorationPolicy frameDecorationPolicy;
    std::unique_ptr<HeHcfTxRxInterceptor> txRxInterceptor;
    bool enableDlMuMimo = false;
    bool heUlMuExchangeActive = false;

    Ieee80211Mac *mac = nullptr;
    Edca *edca = nullptr;
    ITx *tx = nullptr;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    IQosRateSelection *rateSelection = nullptr;
    IOriginatorMacDataService *originatorDataService = nullptr;
    IRecipientQosMacDataService *recipientDataService = nullptr;
    std::unique_ptr<IOriginatorBlockAckAgreementHandler>& originatorBlockAckAgreementHandler;
    IOriginatorBlockAckAgreementPolicy *originatorBlockAckAgreementPolicy = nullptr;
    std::unique_ptr<IRecipientBlockAckAgreementHandler>& recipientBlockAckAgreementHandler;
    std::unique_ptr<IOriginatorBlockAckProcedure>& originatorBlockAckProcedure;
    std::unique_ptr<IRecipientBlockAckProcedure>& recipientBlockAckProcedure;
    HcfExchangeEngine *exchangeEngine = nullptr;
    IRateControl *dataAndMgmtRateControl = nullptr;

    cPar& par(const char *name) const { return hcf->par(name); }
    cModule *getSubmodule(const char *name) const { return hcf->getSubmodule(name); }
    simtime_t simTime() const { return omnetpp::simTime(); }
    void take(cOwnedObject *object) { hcf->take(object); }
    void scheduleAt(simtime_t time, cMessage *timer) { hcf->scheduleAt(time, timer); }
    void cancelEvent(cMessage *timer) { hcf->cancelEvent(timer); }
    void cancelAndDelete(cMessage *timer) { hcf->cancelAndDelete(timer); }
    void sendUp(const std::vector<Packet *>& packets) { hcf->sendUp(packets); }
    bool isFrameSequenceRunning() const { return hcf->isFrameSequenceRunning(); }
    auto makeExchangeActions() { return hcf->makeExchangeActions(); }
    FrameSequenceContext *buildContext(AccessCategory ac) { return hcf->buildContext(ac); }
    void startExchangeFrameSequence(IFrameSequence *sequence, FrameSequenceContext *context)
        { hcf->startExchangeFrameSequence(sequence, context); }
    IFrameSequenceHandler::ICallback *getFrameSequenceCallbackForLegacyAdapter() const
        { return hcf->getFrameSequenceCallbackForLegacyAdapter(); }
    uint32_t calculateBufferedTrafficServiceBytes(Edcaf *edcaf,
            const MacAddress& peer, int tid,
            const std::vector<Packet *>& packets) const
        { return hcf->calculateBufferedTrafficServiceBytes(edcaf, peer, tid, packets); }
    static void addBufferedTrafficServiceBytes(uint32_t& total, uint64_t amount)
        { HcfMacSapTracker::addBufferedTrafficServiceBytes(total, amount); }
    void originatorProcessTransmittedDataFrame(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header, AccessCategory ac)
        { hcf->originatorProcessTransmittedDataFrame(packet, header, ac); }
    void originatorProcessTransmittedManagementFrame(
            const Ptr<const Ieee80211MgmtHeader>& header, AccessCategory ac)
        { hcf->originatorProcessTransmittedManagementFrame(header, ac); }
    void processFailedBlockAckReq(Edcaf *edcaf,
            const Ptr<const Ieee80211BlockAckReq>& header, bool retry)
        { hcf->processFailedBlockAckReq(edcaf, header, retry); }
    void processReceivedBlockAck(Edcaf *edcaf,
            const Ptr<const Ieee80211BlockAck>& header, AccessCategory ac)
        { hcf->processReceivedBlockAck(edcaf, header, ac); }
    IOriginatorBlockAckAgreementHandler *getOriginatorBlockAckAgreementHandler() const
        { return originatorBlockAckAgreementHandler.get(); }
    IOriginatorMacDataService *getOriginatorMacDataService() const
        { return originatorDataService; }

  public:
    HeHcfRuntime(const HcfHeRuntimeServices& services, const Bindings& bindings,
            simtime_t csiValidityDuration, double defaultCsiLeakage,
            const std::string& csiLeakageOverrides);
    ~HeHcfRuntime();

    void initialize();
    void initializeLinkLayer();
    bool handleMessage(cMessage *msg);
    void finish();
    void start(cModule *queueBanksModule);
    void shutdown();
    HePeerStateService& getPeerStateService() const;
    HeQueueService& getQueueService() const;
    HeDlMuExchangeProvider& getDlMuExchangeProvider() const;
    HeTriggeredUlExchangeService& getTriggeredUlExchangeService() const;
    HeSoundingService& getSoundingService() const;
    HeUlCoordinator& getUlCoordinator() const { return *ulCoordinator; }
    const IIeee80211HeLinkPhyContext& getLinkPhyContext() const;
    HeDlMuPreparationSnapshot captureDlPreparationSnapshot(
            AccessCategory accessCategory) const;
    HcfContext buildGrantSelectionContext(AccessCategory accessCategory,
            bool heMode, bool hasEligibleFrame,
            const HeDlMuExchangeProvider::StartupParameters& parameters,
            const std::function<std::optional<HeUlTriggerService::PreparedStart>()>& prepareUl,
            const std::function<HeDlMuPreparationSnapshot()>& captureDl,
            const std::function<bool()>& hasCommonFrame);
    bool commitSelectedExchange(HcfExchangeClass exchangeClass,
            const HcfContext& context,
            const std::function<void(AccessCategory)>& startCommon,
            const std::function<bool(const HeUlTriggerService::PreparedStart&)>& commitUl,
            const std::function<bool(AccessCategory)>& startSingleUserFallback);

    uint32_t getBufferedTrafficServiceBytes(Edcaf *edcaf,
            const MacAddress& peer, int tid = -1) const;
    queueing::IPacketQueue *getPerStaQueue(const MacAddress& staAddr, AccessCategory ac);
    const char *getPendingUlTriggerName() const;
    bool allAssociatedStationsSupportPreamblePuncturing() const;
    bool supportsPreamblePuncturing(const IIeee80211HeUlScheduler::RuAllocation& allocation) const;
    static HeUlScheduleFinalizationResult finalizeUlSchedule(
            const IIeee80211HeUlScheduler::Schedule&, Hz, Hz,
            IIeee80211HeUlTriggerPolicy::TriggerType);
    void retireDeferredPackets();
    StationQueueBank *ensureAssociatedQueueBank(const MacAddress&, uint64_t);
    void finalizeRetiredQueueBanksIfSafe();
    void emitHeTbResponse(HeTbResponseEvent&);
    void updatePeerOperatingMode(const MacAddress&, const Ieee80211HeOperatingMode&);
    bool reportHeDlMuTxResult(Packet *, AccessCategory, bool);
    void originatorProcessBlockAckResult(const Ptr<const Ieee80211BlockAck>&,
            const std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>>&,
            AccessCategory);
    void processReceivedTriggerFrame(Packet *, const Ptr<const Ieee80211TriggerFrame>&);
    void processReceivedMultiStaBlockAck(Packet *, const Ptr<const Ieee80211MultiStaBlockAck>&);
    void legacyPreambleReceived(const Packet *);
    IIeee80211HeDlScheduler::ScheduleContext collectScheduleContext(AccessCategory) const;
    HeDlMuPreparationSnapshot captureHeDlMuPreparationSnapshot(AccessCategory) const override;
    bool stagePerStaFrameForSingleUserTransmission(AccessCategory);
    HeUlPreparationSnapshot captureHeUlPreparationSnapshot(AccessCategory) const override;
    bool tryStartDlMuFrameSequence(AccessCategory);
    bool releaseChannelIfNoFallbackFrame(AccessCategory);
    void startFrameSequence(AccessCategory);
    HcfContext buildGrantSelectionContext(AccessCategory, bool);
    void commitSelectedExchange(HcfExchangeClass, const HcfContext&);
    void handleInternalCollision(std::vector<Edcaf *>);
    bool hasFrameToTransmit();
    bool hasFrameToTransmit(AccessCategory);
    void twtServicePeriodChanged();
    StationQueueBank *getStationQueueBank(const MacAddress&) const;
    HePeerStateService& getHePeerStateService() const;
    HeQueueService& getHeQueueService() const;
    HeDlMuExchangeProvider& getHeDlMuExchangeProvider() const;
    HeTriggeredUlExchangeService& getHeTriggeredUlExchangeService() const;
    queueing::IPacketQueue *resolveHeQueue(HcfQueueToken) const;
    void invalidatePeerDerivedState(const MacAddress&);
    bool processHeSoundingFrame(Packet *, const Ptr<const Ieee80211MacHeader>&) override;
    void rejectUnexpectedHeTb(Packet *) override;
    void processHeTrigger(Packet *, const Ptr<const Ieee80211TriggerFrame>&) override;
    void processHeMultiStaBlockAck(Packet *, const Ptr<const Ieee80211MultiStaBlockAck>&) override;
    void observeBufferStatus(const Ptr<const Ieee80211DataHeader>&) override;
    bool isOperatingModeControlSupported() const override;
    void applyOperatingMode(const MacAddress&, const Ieee80211HeOperatingMode&) override;
    AccessCategory mapTidToAccessCategory(Tid) const override;
    void startMuEdcaTimer(AccessCategory) override;
    bool isHeUlMuExchangeActive() const override { return heUlMuExchangeActive; }
    void notifyHeUlMuPacketTransmitted(Packet *) override;
    bool isHeDlMuContainer(const Packet *) const override;
    void routeHeDlMuContainer(Packet *) override;
    HeHcfTxRxInterceptor::LocalRole getLocalRole() const override;
    uint16_t getLocalAssociationId() const override;
    bool hasActiveHeDlMuMembers() const override;
    void processHeDlMuFailedFrame(Packet *) override;
    MacAddress getLocalAddress() const override;
    MacAddress getBssid() const override;
    void transmitHeNdp(Packet *, const Ptr<const Ieee80211MacHeader>&, simtime_t) override;
    uint16_t getAssociationId(const MacAddress&) const;
    const Ptr<Ieee80211CompressedBlockAck> processTriggeredUlBlockAckReq(Packet *,
            const Ptr<const Ieee80211CompressedBlockAckReq>&, uint16_t);
    bool getPeerOperatingMode(const MacAddress&, Ieee80211HeOperatingMode&) const;
    bool stageHeDlMuPacket(HcfQueueToken, HcfPacketIdentity, AccessCategory) override;
    bool startHeDlMuSingleUserIfEligible(AccessCategory) override;
    void startHeSoundingExchange(const HeSoundingService::StartAction&, AccessCategory) override;
    void configureHeDlMuProtection(AccessCategory) override;
    void startHeDlMuExchange(AccessCategory, const HeDlMuPlan&, uint64_t,
            HeDlMuTxOpFs::AckMethod, const HeDlMuExchangeProvider::StartupParameters&) override;
    queueing::IPacketQueue *resolveHeDlMuQueue(HcfQueueToken) const override;
    Packet *getReservedHeDlMuPacket(uint64_t, const MacAddress&) const override;
    bool isReservedHeDlMuPacket(uint64_t, const MacAddress&, const Packet *) const override;
    IOriginatorBlockAckAgreementHandler *getHeDlMuBlockAckHandler() const override;
    IOriginatorMacDataService *getHeDlMuOriginatorDataService() const override;
    IQosRateSelection *getHeDlMuRateSelection() const override;
    MacAddress getHeDlMuTransmitterAddress() const override;
    int getHeDlMuFcsMode() const override;
    uint8_t getHeDlMuBssColor() const override;
    uint16_t getHeDlMuAssociationId(const MacAddress&) const override;
    std::optional<Ieee80211NegotiatedHeCapabilities> getHeDlMuNegotiatedCapabilities(const MacAddress&) const override;
    void heDlMuPlanFinalized(uint64_t, const std::vector<HeDlMuMember>&) override;
    void heDlMuPlanCommitted(uint64_t, Packet *, const std::vector<HeDlMuMember>&) override;
    void heDlMuMemberTransmitted(uint64_t, const HeDlMuMember&) override;
    void heDlMuUserOutcome(uint64_t, const MacAddress&, HeDlMuUserOutcome) override;
    void heDlMuPlanningFailed(uint64_t, AccessCategory) override;
    void processTriggeredUlFrame(Packet *, const Ptr<const Ieee80211DataHeader>&, uint16_t);
    void processTriggeredUlManagementFrame(Packet *, const Ptr<const Ieee80211MgmtHeader>&, uint16_t);
    bool canRequestHeUlTrigger() const override;
    bool isNdpFeedbackReportEnabled() const override;
    const Ieee80211Mib *getHeUlMib() const override;
    void requestHeUlChannelAccess(AccessCategory) override;
    void configureHeUlMuProtection(AccessCategory) override;
    void startHeUlMuExchange(AccessCategory, const HeUlMuPlan&, IHeUlMuExchangeCallback *) override;
    uint16_t getHeUlAssociationId(const MacAddress&) const override;
    uint32_t allocateHeUlTriggerId() override;
    void heUlMuPlanCommitted(const HeUlMuPlan&, uint32_t) override;
    const Ptr<Ieee80211CompressedBlockAck> processHeUlTriggeredBlockAckReq(Packet *, const Ptr<const Ieee80211CompressedBlockAckReq>&, uint16_t) override;
    void processHeUlTriggeredFrame(Packet *, const Ptr<const Ieee80211DataHeader>&, uint16_t) override;
    void processHeUlTriggeredManagementFrame(Packet *, const Ptr<const Ieee80211MgmtHeader>&, uint16_t) override;
    simtime_t getTriggeredUlCurrentTime() const override;
    void scheduleTriggeredUlTimer(simtime_t, cMessage *) override;
    void cancelTriggeredUlTimer(cMessage *) override;
    void cancelAndDeleteTriggeredUlTimer(cMessage *) override;
    MacAddress getTriggeredUlBssid() const override;
    MacAddress getTriggeredUlLocalAddress() const override;
    uint16_t getTriggeredUlAssociationId() const override;
    uint64_t getTriggeredUlAssociationEpoch() const override;
    std::unique_ptr<ISequenceNumberAssignment> cloneTriggeredUlSequenceState() const override;
    void commitTriggeredUlSequenceState(const ISequenceNumberAssignment&) override;
    Ptr<Ieee80211CompressedBlockAckReq> materializeTriggeredUlBlockAckRequest(const HeTriggeredUlExchangeService::BlockAckRequestSelection&) override;
    void commitTriggeredUlBlockAckRequest(const Ptr<Ieee80211CompressedBlockAckReq>&, AccessCategory) override;
    std::vector<HeTriggeredUlExchangeService::BlockAckCandidateSnapshot> captureTriggeredUlBlockAckCandidates() const;
    void validateTriggeredUlPackets(HcfQueueToken, const std::vector<Packet *>&) const override;
    std::vector<Packet *> commitTriggeredUlPackets(HcfQueueToken, const std::vector<Packet *>&, const std::vector<Packet *>&) override;
    void rollbackTriggeredUlPackets(HcfQueueToken, const std::vector<Packet *>&, const std::vector<Packet *>&, const std::vector<Packet *>&) override;
    void takeTriggeredUlPacket(Packet *) override;
    std::unique_ptr<ITx::PreparedTransmission> prepareTriggeredUlHandoff(Packet *, const Ptr<const Ieee80211MacHeader>&) override;
    void commitTriggeredUlHandoff(std::unique_ptr<ITx::PreparedTransmission>) noexcept override;
    Ptr<Ieee80211CompressedBlockAck> prepareTriggeredUlMuBarBlockAck(const Ieee80211HeTriggerUserInfo&, const MacAddress&) override;
    Hz getTriggeredUlCenterFrequency() const override;
    uint8_t getTriggeredUlBssColor() const override;
    FcsMode getTriggeredUlFcsMode() const override;
    simtime_t getTriggeredUlSifsTime() const override;
    AccessCategory mapTriggeredUlTidToAccessCategory(Tid) const override;
    HeTriggeredUlExchangeService::RandomAccessPreparation prepareTriggeredUlRandomAccess(AccessCategory, int) override;
    void setTriggeredUlRandomAccessPeer(const MacAddress& peer) override;
    int commitTriggeredUlRandomAccess(const HeTriggeredUlExchangeService::RandomAccessPreparation&) override;
    void emitTriggeredUlResponse(HeTbResponseEvent&) override;
    void reportTriggeredUlRandomAccessResult(bool) override;
    void processTriggeredUlBlockAckRequestFailure(const Ptr<const Ieee80211CompressedBlockAckReq>&, AccessCategory) override;
    void processTriggeredUlBlockAckRequestSuccess(const Ptr<const Ieee80211CompressedBlockAck>&, AccessCategory) override;
    void retireTriggeredUlPacket(Packet *, HcfPacketIdentity) override;
    void retryTriggeredUlPacket(Packet *, HcfPacketIdentity, HcfQueueToken,
            const MacAddress&, uint16_t, uint64_t) override;
    void startTriggeredUlMuEdcaTimer(AccessCategory) override;
    void frameSequenceCompleted();
};

} // namespace ieee80211
} // namespace inet

#endif
