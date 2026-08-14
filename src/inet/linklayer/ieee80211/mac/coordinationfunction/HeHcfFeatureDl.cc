//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfFeature.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfFeature.h"

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
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuPlan.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF downlink MU support.

namespace inet {
namespace ieee80211 {

Ptr<const Ieee80211DataHeader> getEligibleHoLDataHeader(queueing::IPacketQueue *queue)
{
    int n = queue->getNumPackets();
    for (int i = 0; i < n; ++i) {
        inet::Packet *pkt = queue->getPacket(i);
        const auto& header = pkt->peekAtFront<inet::ieee80211::Ieee80211MacHeader>();
        auto dataHeader = inet::dynamicPtrCast<const inet::ieee80211::Ieee80211DataHeader>(header);
        if (dataHeader != nullptr && !dataHeader->getReceiverAddress().isMulticast() && !dataHeader->getReceiverAddress().isBroadcast())
            return dataHeader;
    }
    return inet::Ptr<const inet::ieee80211::Ieee80211DataHeader>();
}

bool isMuEligibleDataHeader(const inet::Ptr<const Ieee80211DataHeader>& dataHeader, IOriginatorBlockAckAgreementHandler *baHandler)
{
    // INET-specific precondition: DL MU candidates must be QoS data frames with
    // an active Block Ack agreement so that A-MPDU aggregation and bitmap-based
    // acknowledgment can be used.  IEEE 802.11-2024 26.5.1 permits DL MU
    // operation more generally; this implementation narrows it to 26.6.2/
    // 26.6.3-style A-MPDU packing and 26.4 BlockAck handling.
    return dataHeader != nullptr &&
           dataHeader->getType() == ST_DATA_WITH_QOS &&
           hasActiveOriginatorBlockAckAgreement(baHandler, dataHeader->getReceiverAddress(), dataHeader->getTid());
}

bool hasEligibleExistingFrame(InProgressFrames *inProgress, IAckHandler *ackHandler)
{
    for (int i = 0; i < inProgress->getLength(); ++i) {
        auto header = inProgress->getFrames(i)->peekAtFront<Ieee80211DataOrMgmtHeader>();
        if (ackHandler->isRetransmission(header))
            return true;
    }
    return false;
}

HeDlMuPreparationSnapshot HeHcfFeature::captureHeDlMuPreparationSnapshot(
        AccessCategory ac) const
{
    return this->captureDlPreparationSnapshot(ac);
}

IIeee80211HeDlScheduler::ScheduleContext HeHcfFeature::collectScheduleContext(
        AccessCategory ac) const
{
    return HeDlMuExchangeCoordinator::buildScheduleContext(
            captureHeDlMuPreparationSnapshot(ac));
}

bool HeHcfFeature::stagePerStaFrameForSingleUserTransmission(AccessCategory ac)
{
    auto snapshot = captureHeDlMuPreparationSnapshot(ac);
    const HeDlMuCandidateSnapshot *oldest = nullptr;
    for (const auto& packet : snapshot.packets)
        if (!packet.queuePeer.isUnspecified() && packet.queueIndex == 0 &&
                packet.twtEligible && !packet.addbaRequestInProgress &&
                (oldest == nullptr || packet.enqueueTime < oldest->enqueueTime))
            oldest = &packet;
    return oldest != nullptr && stageHeDlMuPacket(oldest->queueToken,
            oldest->packetIdentity, ac);
}

bool HeHcfFeature::stageHeDlMuPacket(HcfQueueToken queueToken,
        HcfPacketIdentity packetIdentity, AccessCategory ac)
{
    return getHeQueueService().stagePacket(queueToken, packetIdentity,
            edca->getEdcaf(ac)->getPendingQueue());
}

bool HeHcfFeature::startHeDlMuSingleUserIfEligible(AccessCategory ac)
{
    if (edca->getEdcaf(ac)->getInProgressFrames()->getFrameToTransmit() == nullptr)
        return false;
    hcf->startSingleUserExchange(ac);
    return true;
}

HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot
HeHcfFeature::captureHeDlMuProtection(AccessCategory ac) const
{
    auto txop = edca->getEdcaf(ac)->getTxopProcedure();
    auto state = txop->getProtectionStateSnapshot();
    HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot snapshot;
    switch (state.mechanism) {
        case TxopProcedure::SINGLE_PROTECTION:
            snapshot.mechanism = HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::Mechanism::SINGLE_PROTECTION;
            break;
        case TxopProcedure::MULTIPLE_PROTECTION:
            snapshot.mechanism = HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::Mechanism::MULTIPLE_PROTECTION;
            break;
        case TxopProcedure::UNDEFINED_PROTECTION:
            snapshot.mechanism = HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::Mechanism::UNDEFINED_PROTECTION;
            break;
    }
    snapshot.protection = state.protection == TxopProcedure::InitialProtection::LEGACY_RTS_CTS ?
            HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::InitialProtection::LEGACY_RTS_CTS :
            HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::InitialProtection::NONE;
    snapshot.configured = state.configured;
    snapshot.completed = state.completed;
    return snapshot;
}

void HeHcfFeature::startHeSoundingExchange(
        const HeSoundingService::StartAction& action, AccessCategory ac)
{
    configureHeDlMuProtection(ac);
    auto& csiManager = getHePeerStateService().getCsiManager();
    EV_INFO << "At least one MU-capable backlogged STA lacks fresh CSI. Initiating sounding sequence.\n";
    auto sequence = new HeSoundingFs(mac->getMib(), action.targets, modeSet,
            &csiManager, action.channelCenterFrequency, action.channelBandwidth,
            action.dialogToken, action.triggerId);
    startExchangeFrameSequence(sequence, buildContext(ac));
}

void HeHcfFeature::configureHeDlMuProtection(AccessCategory ac)
{
    auto txop = edca->getEdcaf(ac)->getTxopProcedure();
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
}

void HeHcfFeature::restoreHeDlMuProtection(AccessCategory ac,
        const HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot& snapshot)
{
    auto txop = edca->getEdcaf(ac)->getTxopProcedure();
    TxopProcedure::ProtectionState::Snapshot state;
    switch (snapshot.mechanism) {
        case HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::Mechanism::SINGLE_PROTECTION:
            state.mechanism = TxopProcedure::SINGLE_PROTECTION;
            break;
        case HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::Mechanism::MULTIPLE_PROTECTION:
            state.mechanism = TxopProcedure::MULTIPLE_PROTECTION;
            break;
        case HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::Mechanism::UNDEFINED_PROTECTION:
            state.mechanism = TxopProcedure::UNDEFINED_PROTECTION;
            break;
    }
    state.protection = snapshot.protection ==
            HeDlMuExchangeCoordinator::HeDlMuProtectionSnapshot::InitialProtection::LEGACY_RTS_CTS ?
            TxopProcedure::InitialProtection::LEGACY_RTS_CTS :
            TxopProcedure::InitialProtection::NONE;
    state.configured = snapshot.configured;
    state.completed = snapshot.completed;
    txop->restoreProtectionStateSnapshot(state);
}

bool HeHcfFeature::startHeDlMuExchange(AccessCategory ac, const HeDlMuPlan& plan,
        uint64_t transactionToken, HeDlMuTxOpFs::AckMethod ackMethod,
        const HeDlMuExchangeCoordinator::StartupParameters& parameters,
        IHeDlMuExecutionServices *services, IHeDlMuExchangeEvents *events)
{
    auto edcaf = edca->getEdcaf(ac);
    auto frameSequence = std::make_unique<HeDlMuTxOpFs>(plan, modeSet,
            edcaf->getPendingQueue(), edcaf->getAckHandler(),
            getFrameSequenceCallbackForLegacyAdapter(),
            services, events, transactionToken,
            parameters.maxAmpduMpduCount, parameters.maxHeMuPsduLength,
            parameters.maxHeMuPpduDuration, ackMethod);
    auto context = std::unique_ptr<FrameSequenceContext>(buildContext(ac));
    if (!frameSequence->prepare(context.get()))
        return false;
    configureHeDlMuProtection(ac);
    frameSequence->commit(context.get());
    startExchangeFrameSequence(frameSequence.release(), context.release());
    return true;
}

queueing::IPacketQueue *HeHcfFeature::resolveHeDlMuQueue(HcfQueueToken token) const
{
    return resolveHeQueue(token);
}

Packet *HeHcfFeature::getReservedHeDlMuPacket(uint64_t transactionToken,
        const MacAddress& peer) const
{
    return getHeDlMuExchangeCoordinator().getReservedHeDlMuPacket(
            transactionToken, peer);
}
bool HeHcfFeature::isReservedHeDlMuPacket(uint64_t transactionToken,
        const MacAddress& peer, const Packet *packet) const
{
    return getHeDlMuExchangeCoordinator().isReservedHeDlMuPacket(
            transactionToken, peer, packet);
}

IOriginatorBlockAckAgreementHandler *HeHcfFeature::getHeDlMuBlockAckHandler() const
{
    return getOriginatorBlockAckAgreementHandler();
}

IOriginatorMacDataService *HeHcfFeature::getHeDlMuOriginatorDataService() const
{
    return getOriginatorMacDataService();
}

IQosRateSelection *HeHcfFeature::getHeDlMuRateSelection() const
{
    return check_and_cast<IQosRateSelection *>(getSubmodule("rateSelection"));
}

MacAddress HeHcfFeature::getHeDlMuTransmitterAddress() const { return mac->getAddress(); }
int HeHcfFeature::getHeDlMuFcsMode() const { return mac->getFcsMode(); }
uint8_t HeHcfFeature::getHeDlMuBssColor() const { return mac->getMib()->heOperation.bssColor; }
uint16_t HeHcfFeature::getHeDlMuAssociationId(const MacAddress& peer) const { return getAssociationId(peer); }

std::optional<Ieee80211NegotiatedHeCapabilities>
HeHcfFeature::getHeDlMuNegotiatedCapabilities(const MacAddress& peer) const
{
    return mac->getMib()->getNegotiatedHeCapabilities(peer);
}

void HeHcfFeature::notifyHeDlMuMemberTransmitted(
        HeDlMuExchangeId token, const HeDlMuMember& member)
{
    (void)token;
    if (member.packet == nullptr || member.accessCategory < AC_BK ||
            member.accessCategory >= AC_NUMCATEGORIES)
        return;
    auto header = member.packet->peekAtFront<Ieee80211MacHeader>();
    auto edcaf = edca->getEdcaf(member.accessCategory);
    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
        originatorProcessTransmittedDataFrame(member.packet, dataHeader,
                member.accessCategory);
        edcaf->getAckHandler()->transitionToWaitingForBlockAck(dataHeader);
    }
    else if (auto managementHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(header))
        originatorProcessTransmittedManagementFrame(managementHeader,
                member.accessCategory);
}
void HeHcfFeature::notifyHeDlMuUserOutcome(HeDlMuExchangeId token,
        const MacAddress& peer,
        HeDlMuUserOutcome outcome)
{
    (void)token;
    (void)peer;
    (void)outcome;
}
} // namespace ieee80211
} // namespace inet
