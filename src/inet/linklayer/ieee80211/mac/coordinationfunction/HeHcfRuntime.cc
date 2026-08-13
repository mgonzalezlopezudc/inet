//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h"

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

Ptr<const Ieee80211DataHeader> getEligibleHoLDataHeader(queueing::IPacketQueue *queue);
bool isMuEligibleDataHeader(const Ptr<const Ieee80211DataHeader>& dataHeader,
        IOriginatorBlockAckAgreementHandler *baHandler);
bool hasEligibleExistingFrame(InProgressFrames *inProgress, IAckHandler *ackHandler);

HeHcfRuntime::HeHcfRuntime(const HcfHeRuntimeServices& services,
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
        throw cRuntimeError("HE HCF runtime has incomplete lifecycle bindings");
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

HeHcfRuntime::~HeHcfRuntime()
{
}

void HeHcfRuntime::start(cModule *queueBanksModule)
{
    if (started)
        return;
    getQueueService().configure(queueBanksModule);
    getPeerStateService().start();
    started = true;
}

void HeHcfRuntime::shutdown()
{
    if (!started)
        return;
    getTriggeredUlExchangeService().shutdown();
    getPeerStateService().stop();
    getQueueService().clear();
    started = false;
}

HePeerStateService& HeHcfRuntime::getPeerStateService() const
{
    return *services.peerStateService;
}

HeQueueService& HeHcfRuntime::getQueueService() const
{
    return *services.queueService;
}

HeDlMuExchangeProvider& HeHcfRuntime::getDlMuExchangeProvider() const
{
    return *services.dlMuExchangeProvider;
}

HeTriggeredUlExchangeService& HeHcfRuntime::getTriggeredUlExchangeService() const
{
    return *services.triggeredUlExchangeService;
}

HeSoundingService& HeHcfRuntime::getSoundingService() const
{
    return *services.soundingService;
}

const IIeee80211HeLinkPhyContext& HeHcfRuntime::getLinkPhyContext() const
{
    return *linkPhyContext;
}

HeDlMuPreparationSnapshot HeHcfRuntime::captureDlPreparationSnapshot(
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

HcfContext HeHcfRuntime::buildGrantSelectionContext(AccessCategory ac,
        bool heMode, bool hasEligibleFrame,
        const HeDlMuExchangeProvider::StartupParameters& parameters,
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
        return getDlMuExchangeProvider().prepareStart(ac, getDlSnapshot(),
                *bindings.dlScheduler, parameters);
    };
    actions.prepareSingleUser = [&] {
        if (commonFrameAvailable)
            return std::optional<HeDlMuExchangeProvider::PreparedStart>();
        return getDlMuExchangeProvider().prepareSingleUserStart(ac,
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
    auto snapshot = txopCoordinator.prepareGrant(ac, heMode,
            getDlMuExchangeProvider().hasForcedSingleUser(ac),
            hasExecutableFrame, actions);
    HcfContext context(ac, {snapshot.exchangeClass});
    context.setProviderSnapshot(snapshot);
    return context;
}

bool HeHcfRuntime::commitSelectedExchange(HcfExchangeClass exchangeClass,
        const HcfContext& context,
        const std::function<void(AccessCategory)>& startCommon,
        const std::function<bool(const HeUlTriggerService::PreparedStart&)>& commitUl,
        const std::function<bool(AccessCategory)>& startSingleUserFallback)
{
    auto snapshot = context.findProviderSnapshot<HeTxopCoordinatorService::GrantSnapshot>();
    if (snapshot == nullptr || snapshot->exchangeClass != exchangeClass)
        return false;
    const auto ac = snapshot->accessCategory;
    try {
        switch (exchangeClass) {
            case HcfExchangeClass::FORCED_SINGLE_USER:
                if (!getDlMuExchangeProvider().consumeForcedSingleUser(ac))
                    throw cRuntimeError("Forced HE single-user grant became stale before commit");
                startCommon(ac);
                return true;
            case HcfExchangeClass::HE_UL_TRIGGER:
                if (!snapshot->ulTrigger || !commitUl(*snapshot->ulTrigger))
                    throw cRuntimeError("Prepared HE UL Trigger grant became stale before commit");
                return true;
            case HcfExchangeClass::HE_SOUNDING:
            case HcfExchangeClass::RECOVERY_SINGLE_USER:
            case HcfExchangeClass::HE_DL_MULTIUSER:
                if (!snapshot->dlStart ||
                        !getDlMuExchangeProvider().commitStart(*snapshot->dlStart))
                    throw cRuntimeError("Prepared HE grant did not start its exact exchange class");
                return true;
            case HcfExchangeClass::SINGLE_USER:
                if (snapshot->dlStart) {
                    if (!getDlMuExchangeProvider().commitStart(*snapshot->dlStart) &&
                            !startSingleUserFallback(ac))
                        throw cRuntimeError("Prepared HE single-user fallback became stale before commit");
                }
                else
                    startCommon(ac);
                return true;
            default:
                return false;
        }
    }
    catch (...) {
        if (snapshot->dlStart)
            getDlMuExchangeProvider().rollbackStart(*snapshot->dlStart);
        throw;
    }
}

} // namespace ieee80211
} // namespace inet
