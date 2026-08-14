//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfFeature.h"

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Ieee80211HeLinkPhyContextAdapter.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementPolicy.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/queue/OrigEnqueueTimeTag_m.h"

namespace inet {
namespace ieee80211 {

cPar& HeHcfFeature::par(const char *name) const { return hcf->par(name); }
cModule *HeHcfFeature::getSubmodule(const char *name) const { return hcf->getSubmodule(name); }
simtime_t HeHcfFeature::simTime() const { return omnetpp::simTime(); }
void HeHcfFeature::take(cOwnedObject *object) { hcf->take(object); }
void HeHcfFeature::scheduleAt(simtime_t time, cMessage *timer) { hcf->scheduleAt(time, timer); }
void HeHcfFeature::cancelEvent(cMessage *timer) { hcf->cancelEvent(timer); }
void HeHcfFeature::cancelAndDelete(cMessage *timer) { hcf->cancelAndDelete(timer); }
void HeHcfFeature::sendUp(const std::vector<Packet *>& packets) { hcf->sendUp(packets); }
bool HeHcfFeature::isFrameSequenceRunning() const { return hcf->isFrameSequenceRunning(); }
HcfExchangeEngine::Actions HeHcfFeature::makeExchangeActions() { return hcf->makeExchangeActions(); }
FrameSequenceContext *HeHcfFeature::buildContext(AccessCategory ac) { return hcf->buildContext(ac); }
void HeHcfFeature::startExchangeFrameSequence(IFrameSequence *sequence, FrameSequenceContext *context)
    { hcf->startExchangeFrameSequence(sequence, context); }
IFrameSequenceHandler::ICallback *HeHcfFeature::getFrameSequenceCallbackForLegacyAdapter() const
    { return hcf->getFrameSequenceCallbackForLegacyAdapter(); }
uint32_t HeHcfFeature::calculateBufferedTrafficServiceBytes(Edcaf *edcaf,
        const MacAddress& peer, int tid, const std::vector<Packet *>& packets) const
    { return hcf->calculateBufferedTrafficServiceBytes(edcaf, peer, tid, packets); }
void HeHcfFeature::addBufferedTrafficServiceBytes(uint32_t& total, uint64_t amount)
    { HcfMacSapTracker::addBufferedTrafficServiceBytes(total, amount); }
void HeHcfFeature::originatorProcessTransmittedDataFrame(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header, AccessCategory ac)
    { hcf->originatorProcessTransmittedDataFrame(packet, header, ac); }
void HeHcfFeature::originatorProcessTransmittedManagementFrame(
        const Ptr<const Ieee80211MgmtHeader>& header, AccessCategory ac)
    { hcf->originatorProcessTransmittedManagementFrame(header, ac); }
void HeHcfFeature::processFailedBlockAckReq(Edcaf *edcaf,
        const Ptr<const Ieee80211BlockAckReq>& header, bool retry)
    { hcf->processFailedBlockAckReq(edcaf, header, retry); }
void HeHcfFeature::processReceivedBlockAck(Edcaf *edcaf,
        const Ptr<const Ieee80211BlockAck>& header, AccessCategory ac)
    { hcf->processReceivedBlockAck(edcaf, header, ac); }

Ptr<const Ieee80211DataHeader> getEligibleHoLDataHeader(queueing::IPacketQueue *queue);
bool isMuEligibleDataHeader(const Ptr<const Ieee80211DataHeader>& dataHeader,
        IOriginatorBlockAckAgreementHandler *baHandler);
bool hasEligibleExistingFrame(InProgressFrames *inProgress, IAckHandler *ackHandler);

HeHcfFeature::HeHcfFeature(const HcfHeFeatureServices& services,
        const Bindings& bindings, simtime_t csiValidityDuration,
        double defaultCsiLeakage, const std::string& csiLeakageOverrides) :
    services(services), hcf(bindings.owner), bindings(bindings),
    dlScheduler(bindings.dlScheduler), ulCoordinator(bindings.ulCoordinator),
    mac(bindings.mac), edca(bindings.edca), tx(bindings.owner->tx),
    modeSet(bindings.owner->modeSet), rateSelection(bindings.owner->rateSelection),
    originatorDataService(bindings.owner->originatorDataService),
    recipientDataService(bindings.owner->recipientDataService),
    originatorBlockAckAgreementHandler(bindings.owner->originatorBlockAckAgreementHandler),
    originatorBlockAckAgreementPolicy(bindings.owner->originatorBlockAckAgreementPolicy),
    recipientBlockAckAgreementHandler(bindings.owner->recipientBlockAckAgreementHandler),
    originatorBlockAckProcedure(bindings.owner->originatorBlockAckProcedure),
    recipientBlockAckProcedure(bindings.owner->recipientBlockAckProcedure),
    exchangeEngine(bindings.owner->exchangeEngine.get()),
    dataAndMgmtRateControl(bindings.owner->dataAndMgmtRateControl)
{
    if (!services.isComplete() || bindings.owner == nullptr ||
            bindings.mac == nullptr || bindings.edca == nullptr ||
            !bindings.isFrameSequenceRunning || !bindings.invalidateBasePeer)
        throw cRuntimeError("HE HCF feature has incomplete lifecycle bindings");
    dlMuExchangeCoordinator.configure(this, services.soundingService);
    linkPhyContext = std::make_unique<Ieee80211HeLinkPhyContextAdapter>(
            bindings.owner, bindings.mac);
    HePeerStateService::Ports ports;
    ports.retireAssociation = [this] (const auto& peer, uint64_t epoch) {
        getQueueService().retireAssociation(peer, epoch, this->bindings.edca,
                this->bindings.isFrameSequenceRunning());
    };
    ports.ensureAssociation = [this] (const auto& peer, uint64_t epoch) {
        getQueueService().ensureAssociatedQueueBank(peer, epoch);
    };
    ports.finalizeRetiredAssociations = [this] () {
        getQueueService().finalizeRetiredQueueBanksIfSafe(
                this->bindings.isFrameSequenceRunning());
    };
    ports.releaseDeferredRetirements = [this] () {
        getQueueService().retireDeferredPackets(this->bindings.edca);
    };
    ports.invalidateBaseHcf = bindings.invalidateBasePeer;
    ports.invalidateDlScheduler = [this] (const auto& peer) {
        if (this->bindings.dlScheduler != nullptr)
            this->bindings.dlScheduler->invalidatePeer(peer);
    };
    ports.invalidateUlCoordinator = [this] (const auto& peer) {
        if (this->bindings.ulCoordinator != nullptr)
            this->bindings.ulCoordinator->invalidatePeer(peer);
    };
    getPeerStateService().configure(bindings.owner, bindings.mac, ports,
            csiValidityDuration, defaultCsiLeakage, csiLeakageOverrides);
}

HeHcfFeature::~HeHcfFeature()
{
    dlMuExchangeCoordinator.shutdown();
}

void HeHcfFeature::start(cModule *queueBanksModule)
{
    if (started)
        return;
    getQueueService().configure(queueBanksModule);
    getPeerStateService().start();
    started = true;
}

void HeHcfFeature::shutdown()
{
    dlMuExchangeCoordinator.shutdown();
    if (!started)
        return;
    getTriggeredUlExchangeService().shutdown();
    getPeerStateService().stop();
    getQueueService().clear();
    started = false;
}

HePeerStateService& HeHcfFeature::getPeerStateService() const
{
    return *services.peerStateService;
}

HeQueueService& HeHcfFeature::getQueueService() const
{
    return *services.queueService;
}

HeDlMuExchangeCoordinator& HeHcfFeature::getDlMuExchangeCoordinator()
{
    return dlMuExchangeCoordinator;
}

const HeDlMuExchangeCoordinator& HeHcfFeature::getDlMuExchangeCoordinator() const
{
    return dlMuExchangeCoordinator;
}

HeTriggeredUlExchangeService& HeHcfFeature::getTriggeredUlExchangeService() const
{
    return *services.triggeredUlExchangeService;
}

HeSoundingService& HeHcfFeature::getSoundingService() const
{
    return *services.soundingService;
}

const IIeee80211HeLinkPhyContext& HeHcfFeature::getLinkPhyContext() const
{
    return *linkPhyContext;
}

HeTxopCoordinatorService::GrantSnapshot HeHcfFeature::buildGrantSelectionContext(
        AccessCategory ac, bool heMode, bool hasEligibleFrame,
        const HeDlMuExchangeCoordinator::StartupParameters& parameters,
        const std::function<std::optional<HeUlTriggerService::PreparedStart>()>& prepareUl,
        const std::function<HeDlMuPreparationSnapshot()>& captureDl,
        const std::function<bool()>& hasCommonFrame)
{
    std::optional<HeDlMuPreparationSnapshot> dlSnapshot;
    auto getDlSnapshot = [&] () -> const HeDlMuPreparationSnapshot& {
        if (!dlSnapshot)
            dlSnapshot.emplace(captureDl());
        return *dlSnapshot;
    };
    const bool commonFrameAvailable = hasCommonFrame();
    HeTxopCoordinatorService::PreparationActions actions;
    actions.prepareUlTrigger = prepareUl;
    actions.prepareDlStart = [&] {
        return getDlMuExchangeCoordinator().prepareStart(ac, getDlSnapshot(),
                *bindings.dlScheduler, parameters);
    };
    actions.prepareSingleUser = [&] {
        if (commonFrameAvailable)
            return std::optional<HeDlMuExchangeCoordinator::PreparedStart>();
        return getDlMuExchangeCoordinator().prepareSingleUserStart(ac,
                getDlSnapshot());
    };
    bool hasExecutableFrame = commonFrameAvailable;
    if (heMode) {
        const auto& grantSnapshot = getDlSnapshot();
        hasExecutableFrame = hasExecutableFrame ||
                std::any_of(grantSnapshot.packets.begin(), grantSnapshot.packets.end(),
                        [] (const HeDlMuCandidateSnapshot& packet) {
                            return packet.queueIndex == 0 && packet.twtEligible &&
                                    !packet.addbaRequestInProgress;
                        });
    }
    return txopCoordinator.prepareGrant(ac, heMode,
            getDlMuExchangeCoordinator().hasForcedSingleUser(ac),
            hasExecutableFrame, actions);
}

HeDlMuPreparationSnapshot HeHcfFeature::captureDlPreparationSnapshot(
        AccessCategory ac) const
{
    HeDlMuPreparationSnapshot snapshot;
    snapshot.accessCategory = ac;
    snapshot.now = simTime();
    auto& context = snapshot.common;
    const auto phy = getLinkPhyContext().getSnapshot();
    context.channelNumber = phy.getChannelNumber();
    context.channelCenterFrequency = phy.getChannelCenterFrequency();
    context.channelBandwidth = phy.getChannelBandwidth();
    context.totalTransmitPower = phy.getEffectiveTransmitPower();
    context.receiverSensitivity = phy.getReceiveSensitivity();
    context.noiseFigureDb = phy.getNoiseFigureDb();
    context.maxAmpduMpduCount = bindings.owner->par("maxAmpduMpduCount");
    context.packetExtensionDurationUs = phy.getPacketExtensionDurationUs();
    context.puncturedSubchannels = phy.getPuncturedSubchannels();
    context.puncturedSubchannelMask = phy.getPuncturedSubchannelMask();
    context.guardInterval = phy.getGuardInterval();
    context.ltfType = phy.getLtfType();
    context.localHeCapabilities = phy.getLocalHeCapabilities();
    context.enableDlMuMimo = bindings.owner->par("enableDlMuMimo");
    snapshot.sequentialBar = bindings.owner->par("dlMuAckMethod").stdstringValue() == "sequentialBar";
    auto mib = bindings.mac->getMib();
    snapshot.localLdpc = mib->localHeCapabilities.ldpc;
    snapshot.heAccessPoint = bindings.mac->isApInHeFamily();
    snapshot.localDlMuMimoBeamformer = mib->localHeCapabilities.dlMuMimoBeamformer;
    auto edcaf = bindings.edca->getEdcaf(ac);
    auto txop = edcaf->getTxopProcedure();
    if (txop->getLimit() > SIMTIME_ZERO)
        context.txopLimit = std::max(SIMTIME_ZERO,
                txop->getLimit() - txop->getDuration());
    auto ackHandler = edcaf->getAckHandler();
    snapshot.hasRecoveryFrame = hasEligibleExistingFrame(
            edcaf->getInProgressFrames(), ackHandler);
    snapshot.pendingQueueEmpty = edcaf->getPendingQueue()->isEmpty();
    snapshot.hasSingleUserFrameToTransmit =
            edcaf->getInProgressFrames()->getFrameToTransmit() != nullptr;
    snapshot.pendingHeadMuEligible = snapshot.pendingQueueEmpty ||
            isMuEligibleDataHeader(getEligibleHoLDataHeader(
                    edcaf->getPendingQueue()), bindings.blockAckHandler);
    auto queues = getQueueService().getQueueSnapshots(ac,
            edcaf->getPendingQueue());
    for (const auto& queue : queues) {
        for (size_t i = 0; i < queue.packets.size(); ++i) {
            auto packetObject = queue.packets[i];
            auto header = packetObject->peekAtFront<Ieee80211MacHeader>();
            auto destination = header->getReceiverAddress();
            HeDlMuCandidateSnapshot packet;
            packet.packetIdentity = HcfPacketIdentity(packetObject->getId());
            packet.queueToken = queue.token;
            packet.queuePeer = queue.peer;
            packet.queueIndex = i;
            packet.peer = destination;
            packet.accessCategory = ac;
            packet.packetBytes = packetObject->getByteLength();
            auto enqueueTag = packetObject->findTag<OrigEnqueueTimeTag>();
            packet.enqueueTime = enqueueTag == nullptr ? packetObject->getArrivalTime() :
                    enqueueTag->getEnqueueTime();
            packet.unicast = !destination.isMulticast() && !destination.isBroadcast();
            packet.twtEligible = !isTwtSleeping(bindings.mac, destination);
            auto data = dynamicPtrCast<const Ieee80211DataHeader>(header);
            if (data != nullptr) {
                packet.qosData = data->getType() == ST_DATA_WITH_QOS;
                packet.tid = data->getTid();
                packet.sequenceNumberValid = data->getSequenceNumber().isValid();
                packet.retryEligible = !packet.sequenceNumberValid ||
                        ackHandler->isEligibleToTransmit(data);
                auto agreement = bindings.blockAckHandler == nullptr ? nullptr :
                        bindings.blockAckHandler->getAgreement(destination, data->getTid());
                packet.activeBlockAck = agreement != nullptr &&
                        agreement->getIsAddbaResponseReceived();
                packet.addbaRequired = bindings.blockAckHandler != nullptr &&
                        bindings.blockAckPolicy != nullptr &&
                        bindings.blockAckPolicy->isAddbaReqNeeded(packetObject, data);
                packet.addbaRequestInProgress = agreement != nullptr &&
                        agreement->getIsAddbaRequestInProgress() &&
                        (agreement->getExpirationTime() < SIMTIME_ZERO ||
                                simTime() < agreement->getExpirationTime());
                packet.blockAckBufferSize = agreement == nullptr ? -1 :
                        agreement->getBufferSize();
                packet.occupiedBlockAckSlots =
                        ackHandler->getNumOccupiedBlockAckSequencePositions(
                                destination, data->getTid());
            }
            const auto peer = getLinkPhyContext().getPeerSnapshot(destination,
                    SimTime(bindings.owner->par("linkEstimateMaxAge")));
            packet.hasAdvertisement = peer.getHasAdvertisement();
            if (packet.hasAdvertisement)
                packet.advertisement = peer.getAdvertisement();
            packet.hasNegotiatedCapabilities = peer.getHasNegotiatedCapabilities();
            if (packet.hasNegotiatedCapabilities)
                packet.negotiatedCapabilities = peer.getNegotiatedCapabilities();
            Ieee80211HeOperatingMode operatingMode;
            if (getPeerStateService().getOperatingMode(destination, operatingMode))
                packet.operatingModeRxNss = operatingMode.rxNss;
            packet.hasFreshCsi = getPeerStateService().getCsiManager().hasFreshCsi(
                    destination, context.channelBandwidth);
            packet.pathLossDb = peer.getPathLossDb();
            packet.hasFreshPathLoss = peer.getHasFreshPathLoss();
            snapshot.packets.push_back(packet);
        }
    }
    context.numApAntennas = phy.getAntennaCount();
    std::set<MacAddress> peers;
    for (const auto& packet : snapshot.packets)
        if (packet.unicast)
            peers.insert(packet.peer);
    for (const auto& station : peers)
        for (const auto& other : peers)
            if (station != other)
                context.csiLeakages[{station, other}] =
                        getPeerStateService().getCsiManager().getLeakage(
                                station, other, context.channelBandwidth);
    return snapshot;
}
void HeHcfFeature::initialize()
{
        check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"))->
                configure(&getSoundingService());
        getSoundingService().configure(this);
        getHeTriggeredUlExchangeService().configure(this);
        dlScheduler = check_and_cast<IIeee80211HeDlScheduler *>(getSubmodule("dlScheduler"));
        ulCoordinator = check_and_cast<HeUlCoordinator *>(getSubmodule("ulCoordinator"));
        ulTriggerService.configure(this, this, ulCoordinator, par("ulTriggerCheckInterval"));
        txRxInterceptor = std::make_unique<HeHcfTxRxInterceptor>(this);
        hcf->registerTxRxInterceptor(txRxInterceptor.get());
        hcf->registerFrameDecorator([this] (Packet *packet) {
            HeFrameDecorationPolicy::Request decoration;
            decoration.associated = mac->getMib()->getLocalAssociationId() > 0;
            decoration.sendOperatingModeIndication = par("sendOperatingModeIndication");
            decoration.operatingModeControlSupported = mac->getMib()->localHeCapabilities.omControl;
            decoration.operatingModeChannelWidth = par("operatingModeChannelWidth");
            decoration.operatingModeRxNss = par("operatingModeRxNss");
            decoration.operatingModeUlMuDisable = par("operatingModeUlMuDisable");
            frameDecorationPolicy.decorate(packet, decoration,
                    [this] (Tid tid) { return edca->mapTidToAc(tid); },
                    [this] (const MacAddress& peer, Tid tid, AccessCategory accessCategory) {
                        return getBufferedTrafficServiceBytes(edca->getEdcaf(accessCategory), peer, tid);
                    });
        });
        hcf->replaceFrameSequenceHandler(std::make_unique<HeFrameSequenceHandler>());

        enableDlMuMimo = par("enableDlMuMimo").boolValue();
}

void HeHcfFeature::initializeLinkLayer()
{
    modeSet = hcf->modeSet;
    if (mac->isApInHeFamily()) {
        start(getSubmodule("queueBanks"));
        for (const auto& station : mac->getMib()->getPeerAssociationSnapshots()) {
            if (station.hasMemberStatus() && station.getMemberStatus() == Ieee80211Mib::ASSOCIATED) {
                if (station.getAssociationEpoch() == 0)
                    mac->getMib()->commitPeerAssociation(station.getAddress());
                else
                    ensureAssociatedQueueBank(station.getAddress(), station.getAssociationEpoch());
            }
        }
        ulTriggerService.start(hcf);
    }
}

void HeHcfFeature::emitHeTbResponse(HeTbResponseEvent& event)
{
    ASSERT(event.triggerId != 0);
    hcf->emit(cComponent::registerSignal("heTbResponseCommitted"), &event);
    hcf->emit(cComponent::registerSignal("heTbResponseTriggerId"),
            static_cast<unsigned long>(event.triggerId));
    hcf->emit(cComponent::registerSignal("heTbResponseReason"),
            static_cast<long>(event.reason));
    hcf->emit(cComponent::registerSignal("heTbResponseHadPendingPayload"),
            event.hadPendingPayload ? 1L : 0L);
    hcf->emit(cComponent::registerSignal("heTbResponsePendingBytes"), event.pendingBytes);
    hcf->emit(cComponent::registerSignal("heTbResponseSelectedBytes"), event.selectedBytes);
    hcf->emit(cComponent::registerSignal("heTbResponseReportedBytes"), event.reportedBytes);
}

void HeHcfFeature::updatePeerOperatingMode(const MacAddress& peer,
        const Ieee80211HeOperatingMode& mode)
{
    getHePeerStateService().updateOperatingMode(peer, mode);
}

AccessCategory HeHcfFeature::mapTidToAccessCategory(Tid tid) const
{
    switch (tid) {
        case 1:
        case 2: return AC_BK;
        case 0:
        case 3: return AC_BE;
        case 4:
        case 5: return AC_VI;
        case 6:
        case 7: return AC_VO;
        default: throw omnetpp::cRuntimeError("Invalid TID for HE UL scheduling: %d", tid);
    }
}


const char *HeHcfFeature::getPendingUlTriggerName() const
{
    return ulTriggerService.getPendingTriggerName();
}

void HeHcfFeature::finish()
{
    shutdown();
}

bool HeHcfFeature::handleMessage(cMessage *msg)
{
    finalizeRetiredQueueBanksIfSafe();
    if (msg == getHeTriggeredUlExchangeService().getResponseTimer()) {
        getHeTriggeredUlExchangeService().handleTimeout();
        return true;
    }
    return ulTriggerService.handleTimer(msg, hcf);
}

bool HeHcfFeature::canRequestHeUlTrigger() const
{
    return mac->isApInHeFamily() && exchangeEngine->canRequestChannelAccess() &&
            !isFrameSequenceRunning() &&
            edca->getChannelOwner() == nullptr && !tx->isBusy();
}

bool HeHcfFeature::isNdpFeedbackReportEnabled() const
{
    return par("enableNdpFeedbackReport").boolValue();
}

const Ieee80211Mib *HeHcfFeature::getHeUlMib() const
{
    return mac->getMib();
}

void HeHcfFeature::requestHeUlChannelAccess(AccessCategory accessCategory)
{
    EV_INFO << "Requesting channel access for HE UL "
            << getPendingUlTriggerName() << " Trigger\n";
    exchangeEngine->channelAccessRequested();
    edca->requestChannelAccess(accessCategory, hcf);
}

uint16_t HeHcfFeature::getHeUlAssociationId(const MacAddress& address) const
{
    return getAssociationId(address);
}

uint32_t HeHcfFeature::allocateHeUlTriggerId()
{
    return ulTriggerService.allocateTriggerId();
}

void HeHcfFeature::heUlMuPlanCommitted(const HeUlMuPlan& plan, uint32_t triggerId)
{
    ulTriggerService.planCommitted(plan, triggerId);
}

const Ptr<Ieee80211CompressedBlockAck> HeHcfFeature::processHeUlTriggeredBlockAckReq(
        Packet *packet, const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
        uint16_t associationId)
{
    return processTriggeredUlBlockAckReq(packet, blockAckReq, associationId);
}

void HeHcfFeature::processHeUlTriggeredFrame(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header, uint16_t associationId)
{
    processTriggeredUlFrame(packet, header, associationId);
}

void HeHcfFeature::processHeUlTriggeredManagementFrame(Packet *packet,
        const Ptr<const Ieee80211MgmtHeader>& header, uint16_t)
{
    // The HE-TB exchange owns the response acknowledgment. Deliver the
    // management frame upward and update management state through the normal
    // dispatcher; that dispatcher does not schedule a legacy ACK.
    sendUp(recipientDataService->managementFrameReceived(packet, header));
    hcf->recipientProcessReceivedManagementFrame(header);
}

queueing::IPacketQueue *HeHcfFeature::getPerStaQueue(const MacAddress& staAddr, AccessCategory ac)
{
    finalizeRetiredQueueBanksIfSafe();
    auto peer = getHePeerStateService().getPeerSnapshot(staAddr);
    if (peer.getAssociationEpoch() != 0) {
        auto queue = getHeQueueService().getPerStaQueue(staAddr,
                peer.getAssociationEpoch(), ac);
        if (queue != nullptr)
            return queue;
    }
    return edca->getEdcaf(ac)->getPendingQueue();
}

StationQueueBank *HeHcfFeature::ensureAssociatedQueueBank(const MacAddress& peer, uint64_t associationEpoch)
{
    auto snapshot = getHePeerStateService().getPeerSnapshot(peer);
    if (snapshot.getAssociationEpoch() != associationEpoch)
        return nullptr;
    return getHeQueueService().ensureAssociatedQueueBank(peer, associationEpoch);
}

void HeHcfFeature::finalizeRetiredQueueBanksIfSafe()
{
    getHeQueueService().finalizeRetiredQueueBanksIfSafe(isFrameSequenceRunning());
}

void HeHcfFeature::retireDeferredPackets()
{
    getHePeerStateService().releaseDeferredRetirements();
}

void HeHcfFeature::frameSequenceCompleted()
{
    heUlMuExchangeActive = false;
    retireDeferredPackets();
}

void HeHcfFeature::frameSequenceAborted()
{
    dlMuExchangeCoordinator.abortActiveExchange();
    heUlMuExchangeActive = false;
    retireDeferredPackets();
}

StationQueueBank *HeHcfFeature::getStationQueueBank(const MacAddress& staAddr) const
{
    return getHeQueueService().getStationQueueBank(staAddr);
}

void HeHcfFeature::invalidatePeerDerivedState(const MacAddress& peer)
{
    getHePeerStateService().invalidatePeer(peer,
            HePeerStateService::InvalidationReason::ASSOCIATION_CHANGED);
}

bool HeHcfFeature::releaseChannelIfNoFallbackFrame(AccessCategory ac)
{
    auto fallbackEdcaf = edca->getEdcaf(ac);
    if (fallbackEdcaf->getPendingQueue()->isEmpty() &&
            fallbackEdcaf->getInProgressFrames()->getLength() == 0)
        stagePerStaFrameForSingleUserTransmission(ac);
    if (fallbackEdcaf->getInProgressFrames()->getFrameToTransmit() != nullptr)
        return false;

    EV_WARN << "Channel granted without an eligible SU, DL-MU, or UL trigger frame; releasing channel.\n";
    exchangeEngine->preparationCompletedWithoutSequence(makeExchangeActions());
    fallbackEdcaf->releaseChannel(hcf);
    fallbackEdcaf->getTxopProcedure()->endTxop();
    return true;
}

void HeHcfFeature::startFrameSequence(AccessCategory ac)
{
    auto grant = buildGrantSelectionContext(ac, hasFrameToTransmit(ac));
    switch (grant.startKind) {
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::CHANNEL_RELEASE:
            hcf->releaseChannel(ac);
            return;
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::COMMON_SINGLE_USER:
            hcf->startSingleUserExchange(ac);
            return;
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::FORCED_SINGLE_USER:
            if (!getDlMuExchangeCoordinator().consumeForcedSingleUser(ac))
                throw cRuntimeError("Forced HE single-user grant became stale");
            hcf->startSingleUserExchange(ac);
            return;
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::UL_TRIGGER:
            if (!grant.ulTrigger || !ulTriggerService.commitStart(*grant.ulTrigger))
                throw cRuntimeError("Prepared HE UL grant became stale");
            return;
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::SOUNDING:
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::RECOVERY_SINGLE_USER:
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::DL_MULTIUSER:
            if (!grant.dlStart || !getDlMuExchangeCoordinator().commitStart(*grant.dlStart))
                throw cRuntimeError("Prepared HE DL grant became stale");
            return;
        case HeTxopCoordinatorService::GrantSnapshot::StartKind::PREPARED_SINGLE_USER:
            if (!grant.dlStart)
                throw cRuntimeError("Prepared HE single-user grant lacks its coordinator start");
            if (!getDlMuExchangeCoordinator().commitStart(*grant.dlStart) &&
                    !startHeDlMuSingleUserIfEligible(ac))
                throw cRuntimeError("Prepared HE single-user fallback became stale");
            return;
    }
}

HeTxopCoordinatorService::GrantSnapshot HeHcfFeature::buildGrantSelectionContext(AccessCategory ac,
        bool hasEligibleFrame)
{
    finalizeRetiredQueueBanksIfSafe();
    ASSERT(modeSet != nullptr);
    const bool heMode = modeSet->hasPhyFamily(
            physicallayer::Ieee80211PhyFamily::HE);
    std::optional<HeUlPreparationSnapshot> ulSnapshot;
    HeDlMuExchangeCoordinator::StartupParameters parameters;
    parameters.maxAmpduMpduCount = par("maxAmpduMpduCount");
    parameters.maxHeMuPsduLength = par("maxHeMuPsduLength");
    parameters.maxHeMuPpduDuration = par("maxHeMuPpduDuration");
    return this->buildGrantSelectionContext(ac, heMode,
            hasEligibleFrame, parameters,
            [this, ac, &ulSnapshot] {
                if (!mac->isApInHeFamily())
                    return std::optional<HeUlTriggerService::PreparedStart>();
                if (!ulSnapshot)
                    ulSnapshot.emplace(captureHeUlPreparationSnapshot(ac));
                return ulTriggerService.prepareStart(ac, *ulSnapshot);
            },
            [this, ac] { return captureHeDlMuPreparationSnapshot(ac); },
            [this, ac] { return hcf->hasCommonFrameToTransmit(ac); });
}

void HeHcfFeature::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
{
    std::vector<Edcaf *> collidedEdcafsWithFrame;
    for (auto edcaf : internallyCollidedEdcafs) {
        // 10.23 internal collision handling is still performed by HCF, but HE
        // AP queues may be per-STA.  Stage the oldest per-STA fallback frame so
        // the base HCF collision logic sees the same pending-frame surface.
        if (edcaf->getPendingQueue()->isEmpty() && edcaf->getInProgressFrames()->getLength() == 0)
            stagePerStaFrameForSingleUserTransmission(edcaf->getAccessCategory());
        if (edcaf->getInProgressFrames()->getFrameToTransmit() != nullptr)
            collidedEdcafsWithFrame.push_back(edcaf);
    }
    for (auto edcaf : collidedEdcafsWithFrame)
        hcf->handleEdcafInternalCollision(edcaf);
}

bool HeHcfFeature::hasFrameToTransmit(AccessCategory ac)
{
    if (hcf->hasCommonFrameToTransmit(ac))
        return true;
    return getHeQueueService().hasFrameToTransmit(ac);
}

bool HeHcfFeature::hasFrameToTransmit()
{
    auto edcaf = edca->getChannelOwner();
    return edcaf != nullptr && hasFrameToTransmit(edcaf->getAccessCategory());
}

uint16_t HeHcfFeature::getAssociationId(const MacAddress& address) const
{
    return getHePeerStateService().getAssociationId(address);
}

bool HeHcfFeature::getPeerOperatingMode(const MacAddress& address, Ieee80211HeOperatingMode& mode) const
{
    return getHePeerStateService().getOperatingMode(address, mode);
}

HePeerStateService& HeHcfFeature::getHePeerStateService() const
{
    return this->getPeerStateService();
}

HeQueueService& HeHcfFeature::getHeQueueService() const
{
    return this->getQueueService();
}

const HeDlMuExchangeCoordinator& HeHcfFeature::getHeDlMuExchangeCoordinator() const
{
    return this->getDlMuExchangeCoordinator();
}

HeTriggeredUlExchangeService& HeHcfFeature::getHeTriggeredUlExchangeService() const
{
    return this->getTriggeredUlExchangeService();
}

queueing::IPacketQueue *HeHcfFeature::resolveHeQueue(HcfQueueToken token) const
{
    return getHeQueueService().resolveQueue(token);
}

void HeHcfFeature::twtServicePeriodChanged()
{
    getHePeerStateService().handleTwtBoundary();
}
} // namespace ieee80211
} // namespace inet
