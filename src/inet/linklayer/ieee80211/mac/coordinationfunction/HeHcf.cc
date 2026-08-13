//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfObservationSink.h"

#include <algorithm>
#include <sstream>

#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Ieee80211HeLinkPhyContextAdapter.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfTxRxInterceptor.h"

// HE HCF coordination function.
//
// This module implements the 802.11ax AP and non-AP STA behavior for DL/UL OFDMA
// and MU-MIMO TXOPs.  The relevant normative text is in IEEE 802.11-2024:
//   - Clause 26.1 / 26.2: HE introduction, channel access and TXOP rules.
//   - Clause 26.5: UL multi-user operation (Trigger frames, HE TB PPDU response).
//   - Clause 26.5.4: Uplink OFDMA random access (UORA).
//   - Clause 26.4.4: frame exchange rules for HE MU and HE TB PPDUs.
//   - Clause 27.3.11: HE PPDU formats (HE SU, HE ER SU, HE MU, HE TB).
//   - Clause 27.3.11.13: HE MU PPDU format and HE-SIG-B.
//   - Clauses 27.3.12.5.5 and 27.3.13: HE-TB encoding and duration.
//
// Implementation notes / deviations from the standard:
//   - DL MU scheduling is restricted to QoS data frames that already have an
//     active originator Block Ack agreement.  The standard does not require a
//     Block Ack agreement before scheduling a DL MU PPDU, but INET's model
//     uses A-MPDU aggregation and per-TID reordering, so this is an
//     implementation-enforced precondition, not a normative one.
//   - Per-STA queue banks are used to keep frames destined to different STAs
//     separable; this is an INET-specific queuing architecture.
//   - The BSRP trigger allocation assigns one RU per associated STA plus the
//     remaining RUs as random-access RUs.  The standard permits many other RU
//     allocation strategies for BSRP; this is a simple approximation.
//   - UORA is modeled with a per-BSS global OCW and uniform RA-RU selection.
//     The standard uses per-AC OCW state and a more involved selection rule;
//     the current model is conservative in that it follows the OCW update
//     procedure but collapses per-AC state.
//   - UL basic trigger responses use single-TID A-MPDUs only; multi-TID
//     aggregation in HE TB PPDUs is not implemented.

namespace inet {
namespace ieee80211 {

Define_Module(HeHcf);
Register_Class(HeTbResponseEvent);

void HeHcfRuntime::initialize()
{
        check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"))->
                configure(&getSoundingService());
        getSoundingService().configure(this);
        getHeDlMuExchangeProvider().configure(this,
                &getSoundingService());
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

void HeHcfRuntime::initializeLinkLayer()
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

void HeHcfRuntime::emitHeTbResponse(HeTbResponseEvent& event)
{
    ASSERT(event.triggerId != 0);
    HcfObservationSink::heTbResponseCommitted(hcf, &event);
}

void HeHcfRuntime::updatePeerOperatingMode(const MacAddress& peer,
        const Ieee80211HeOperatingMode& mode)
{
    getHePeerStateService().updateOperatingMode(peer, mode);
}

AccessCategory HeHcfRuntime::mapTidToAccessCategory(Tid tid) const
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


const char *HeHcfRuntime::getPendingUlTriggerName() const
{
    return ulTriggerService.getPendingTriggerName();
}

int HeHcfRuntime::getStationQueueBankCount() const
{
    return getHeQueueService().getStationQueueBankCount();
}

std::string HeHcfRuntime::getCsiTableSummary() const
{
    return getHePeerStateService().getCsiTableSummary();
}

std::string HeHcfRuntime::getHeHcfSummary() const
{
    std::stringstream stream;
    stream << "DL scheduler=" << (dlScheduler == nullptr ? "none" : "configured")
           << ", UL coordinator=" << (ulCoordinator != nullptr && ulCoordinator->isEnabled() ? "enabled" : "disabled")
           << ", pendingTrigger=" << getPendingUlTriggerName()
           << ", queueBanks=" << getStationQueueBankCount()
           << ", triggeredUL=" << getHeTriggeredUlExchangeService().getExchangeCount()
           << ", dlMuMimo=" << (enableDlMuMimo ? "enabled" : "disabled")
           << ", csiEntries=" << getHePeerStateService().getCsiManager().getEntryCount();
    return stream.str();
}

void HeHcfRuntime::finish()
{
    shutdown();
}

bool HeHcfRuntime::handleMessage(cMessage *msg)
{
    finalizeRetiredQueueBanksIfSafe();
    if (msg == getHeTriggeredUlExchangeService().getResponseTimer()) {
        getHeTriggeredUlExchangeService().handleTimeout();
        return true;
    }
    return ulTriggerService.handleTimer(msg, hcf);
}

bool HeHcfRuntime::canRequestHeUlTrigger() const
{
    return mac->isApInHeFamily() && exchangeEngine->canRequestChannelAccess() &&
            !isFrameSequenceRunning() &&
            edca->getChannelOwner() == nullptr && !tx->isBusy();
}

bool HeHcfRuntime::isNdpFeedbackReportEnabled() const
{
    return par("enableNdpFeedbackReport").boolValue();
}

const Ieee80211Mib *HeHcfRuntime::getHeUlMib() const
{
    return mac->getMib();
}

void HeHcfRuntime::requestHeUlChannelAccess(AccessCategory accessCategory)
{
    EV_INFO << "Requesting channel access for HE UL "
            << getPendingUlTriggerName() << " Trigger\n";
    exchangeEngine->channelAccessRequested();
    edca->requestChannelAccess(accessCategory, hcf);
}

uint16_t HeHcfRuntime::getHeUlAssociationId(const MacAddress& address) const
{
    return getAssociationId(address);
}

uint32_t HeHcfRuntime::allocateHeUlTriggerId()
{
    return ulTriggerService.allocateTriggerId();
}

void HeHcfRuntime::heUlMuPlanCommitted(const HeUlMuPlan& plan, uint32_t triggerId)
{
    ulTriggerService.planCommitted(plan, triggerId);
}

const Ptr<Ieee80211CompressedBlockAck> HeHcfRuntime::processHeUlTriggeredBlockAckReq(
        Packet *packet, const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
        uint16_t associationId)
{
    return processTriggeredUlBlockAckReq(packet, blockAckReq, associationId);
}

void HeHcfRuntime::processHeUlTriggeredFrame(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& header, uint16_t associationId)
{
    processTriggeredUlFrame(packet, header, associationId);
}

queueing::IPacketQueue *HeHcfRuntime::getPerStaQueue(const MacAddress& staAddr, AccessCategory ac)
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

StationQueueBank *HeHcfRuntime::ensureAssociatedQueueBank(const MacAddress& peer, uint64_t associationEpoch)
{
    auto snapshot = getHePeerStateService().getPeerSnapshot(peer);
    if (snapshot.getAssociationEpoch() != associationEpoch)
        return nullptr;
    return getHeQueueService().ensureAssociatedQueueBank(peer, associationEpoch);
}

void HeHcfRuntime::finalizeRetiredQueueBanksIfSafe()
{
    getHeQueueService().finalizeRetiredQueueBanksIfSafe(isFrameSequenceRunning());
}

void HeHcfRuntime::retireDeferredPackets()
{
    getHePeerStateService().releaseDeferredRetirements();
}

void HeHcfRuntime::frameSequenceFinished()
{
    frameSequenceCompleted();
}

void HeHcfRuntime::frameSequenceCompleted()
{
    heUlMuExchangeActive = false;
    retireDeferredPackets();
}

StationQueueBank *HeHcfRuntime::getStationQueueBank(const MacAddress& staAddr) const
{
    return getHeQueueService().getStationQueueBank(staAddr);
}

void HeHcfRuntime::invalidatePeerDerivedState(const MacAddress& peer)
{
    getHePeerStateService().invalidatePeer(peer,
            HePeerStateService::InvalidationReason::ASSOCIATION_CHANGED);
}

bool HeHcfRuntime::releaseChannelIfNoFallbackFrame(AccessCategory ac)
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

void HeHcfRuntime::startFrameSequence(AccessCategory ac)
{
    auto context = buildGrantSelectionContext(ac, hasFrameToTransmit(ac));
    auto snapshot = context.findProviderSnapshot<
            HeTxopCoordinatorService::GrantSnapshot>();
    if (snapshot == nullptr)
        throw cRuntimeError("HE legacy grant wrapper did not capture an exact snapshot");
    commitSelectedExchange(snapshot->exchangeClass, context);
}

HcfContext HeHcfRuntime::buildGrantSelectionContext(AccessCategory ac,
        bool hasEligibleFrame)
{
    finalizeRetiredQueueBanksIfSafe();
    ASSERT(modeSet != nullptr);
    const bool heMode = modeSet->hasPhyFamily(
            physicallayer::Ieee80211PhyFamily::HE);
    std::optional<HeUlPreparationSnapshot> ulSnapshot;
    HeDlMuExchangeProvider::StartupParameters parameters;
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

void HeHcfRuntime::commitSelectedExchange(HcfExchangeClass exchangeClass,
        const HcfContext& context)
{
    if (!this->commitSelectedExchange(exchangeClass, context,
            [this] (AccessCategory ac) { hcf->startSingleUserExchange(ac); },
            [this] (const auto& start) { return ulTriggerService.commitStart(start); },
            [this] (const auto& start) { ulTriggerService.rollbackStart(start); },
            [this] (AccessCategory ac) { return startHeDlMuSingleUserIfEligible(ac); }))
    {
        auto selectedAccessCategory = context.getSelectionAccessCategory();
        if (!selectedAccessCategory.has_value())
            throw cRuntimeError("HCF provider commit lacks an access category projection");
        if (exchangeClass != HcfExchangeClass::CHANNEL_RELEASE)
            throw cRuntimeError("HE runtime cannot commit exchange class %d",
                    static_cast<int>(exchangeClass));
        auto edcaf = edca->getEdcaf(*selectedAccessCategory);
        exchangeEngine->preparationCompletedWithoutSequence(makeExchangeActions());
        edcaf->releaseChannel(hcf);
        edcaf->getTxopProcedure()->endTxop();
    }
}

void HeHcfRuntime::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
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

bool HeHcfRuntime::hasFrameToTransmit(AccessCategory ac)
{
    if (hcf->hasCommonFrameToTransmit(ac))
        return true;
    return getHeQueueService().hasFrameToTransmit(ac);
}

bool HeHcfRuntime::hasFrameToTransmit()
{
    auto edcaf = edca->getChannelOwner();
    return edcaf != nullptr && hasFrameToTransmit(edcaf->getAccessCategory());
}

uint16_t HeHcfRuntime::getAssociationId(const MacAddress& address) const
{
    return getHePeerStateService().getAssociationId(address);
}

bool HeHcfRuntime::getPeerOperatingMode(const MacAddress& address, Ieee80211HeOperatingMode& mode) const
{
    return getHePeerStateService().getOperatingMode(address, mode);
}

HePeerStateService& HeHcfRuntime::getHePeerStateService() const
{
    return this->getPeerStateService();
}

HeQueueService& HeHcfRuntime::getHeQueueService() const
{
    return this->getQueueService();
}

HeDlMuExchangeProvider& HeHcfRuntime::getHeDlMuExchangeProvider() const
{
    return this->getDlMuExchangeProvider();
}

HeTriggeredUlExchangeService& HeHcfRuntime::getHeTriggeredUlExchangeService() const
{
    return this->getTriggeredUlExchangeService();
}

queueing::IPacketQueue *HeHcfRuntime::resolveHeQueue(HcfQueueToken token) const
{
    return getHeQueueService().resolveQueue(token);
}

void HeHcfRuntime::twtServicePeriodChanged()
{
    getHePeerStateService().handleTwtBoundary();
}

} // namespace ieee80211
} // namespace inet
