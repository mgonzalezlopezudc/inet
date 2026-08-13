//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h"

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

HeDlMuPreparationSnapshot HeHcfRuntime::captureHeDlMuPreparationSnapshot(
        AccessCategory ac) const
{
    return this->captureDlPreparationSnapshot(ac);
}

IIeee80211HeDlScheduler::ScheduleContext HeHcfRuntime::collectScheduleContext(
        AccessCategory ac) const
{
    return HeDlMuExchangeProvider::buildScheduleContext(
            captureHeDlMuPreparationSnapshot(ac));
}

bool HeHcfRuntime::stagePerStaFrameForSingleUserTransmission(AccessCategory ac)
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

bool HeHcfRuntime::tryStartDlMuFrameSequence(AccessCategory ac)
{
    HeDlMuExchangeProvider::StartupParameters parameters;
    parameters.maxAmpduMpduCount = par("maxAmpduMpduCount");
    parameters.maxHeMuPsduLength = par("maxHeMuPsduLength");
    parameters.maxHeMuPpduDuration = par("maxHeMuPpduDuration");
    return getHeDlMuExchangeProvider().tryStart(ac,
            captureHeDlMuPreparationSnapshot(ac), *dlScheduler, parameters);
}

bool HeHcfRuntime::stageHeDlMuPacket(HcfQueueToken queueToken,
        HcfPacketIdentity packetIdentity, AccessCategory ac)
{
    return getHeQueueService().stagePacket(queueToken, packetIdentity,
            edca->getEdcaf(ac)->getPendingQueue());
}

bool HeHcfRuntime::startHeDlMuSingleUserIfEligible(AccessCategory ac)
{
    if (edca->getEdcaf(ac)->getInProgressFrames()->getFrameToTransmit() == nullptr)
        return false;
    hcf->startSingleUserExchange(ac);
    return true;
}

void HeHcfRuntime::startHeSoundingExchange(
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

void HeHcfRuntime::configureHeDlMuProtection(AccessCategory ac)
{
    auto txop = edca->getEdcaf(ac)->getTxopProcedure();
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
}

void HeHcfRuntime::startHeDlMuExchange(AccessCategory ac, const HeDlMuPlan& plan,
        uint64_t transactionToken, HeDlMuTxOpFs::AckMethod ackMethod,
        const HeDlMuExchangeProvider::StartupParameters& parameters)
{
    auto edcaf = edca->getEdcaf(ac);
    auto frameSequence = new HeDlMuTxOpFs(plan, modeSet,
            edcaf->getPendingQueue(), edcaf->getAckHandler(),
            getFrameSequenceCallbackForLegacyAdapter(),
            this, transactionToken,
            parameters.maxAmpduMpduCount, parameters.maxHeMuPsduLength,
            parameters.maxHeMuPpduDuration, ackMethod);
    startExchangeFrameSequence(frameSequence,
            buildContext(ac));
}

queueing::IPacketQueue *HeHcfRuntime::resolveHeDlMuQueue(HcfQueueToken token) const
{
    return resolveHeQueue(token);
}

Packet *HeHcfRuntime::getReservedHeDlMuPacket(uint64_t transactionToken,
        const MacAddress& peer) const
{
    return getHeDlMuExchangeProvider().getReservedHeDlMuPacket(
            transactionToken, peer);
}
bool HeHcfRuntime::isReservedHeDlMuPacket(uint64_t transactionToken,
        const MacAddress& peer, const Packet *packet) const
{
    return getHeDlMuExchangeProvider().isReservedHeDlMuPacket(
            transactionToken, peer, packet);
}

IOriginatorBlockAckAgreementHandler *HeHcfRuntime::getHeDlMuBlockAckHandler() const
{
    return getOriginatorBlockAckAgreementHandler();
}

IOriginatorMacDataService *HeHcfRuntime::getHeDlMuOriginatorDataService() const
{
    return getOriginatorMacDataService();
}

IQosRateSelection *HeHcfRuntime::getHeDlMuRateSelection() const
{
    return check_and_cast<IQosRateSelection *>(getSubmodule("rateSelection"));
}

MacAddress HeHcfRuntime::getHeDlMuTransmitterAddress() const { return mac->getAddress(); }
int HeHcfRuntime::getHeDlMuFcsMode() const { return mac->getFcsMode(); }
uint8_t HeHcfRuntime::getHeDlMuBssColor() const { return mac->getMib()->heOperation.bssColor; }
uint16_t HeHcfRuntime::getHeDlMuAssociationId(const MacAddress& peer) const { return getAssociationId(peer); }

std::optional<Ieee80211NegotiatedHeCapabilities>
HeHcfRuntime::getHeDlMuNegotiatedCapabilities(const MacAddress& peer) const
{
    return mac->getMib()->getNegotiatedHeCapabilities(peer);
}

void HeHcfRuntime::heDlMuPlanFinalized(uint64_t token,
        const std::vector<HeDlMuMember>& members)
{
    getHeDlMuExchangeProvider().finalizeReservation(token, members);
}

void HeHcfRuntime::heDlMuPlanCommitted(uint64_t token, Packet *container,
        const std::vector<HeDlMuMember>& members)
{
    getHeDlMuExchangeProvider().heDlMuPlanCommitted(token, container, members);
}
void HeHcfRuntime::heDlMuMemberTransmitted(uint64_t token, const HeDlMuMember& member)
{
    if (!getHeDlMuExchangeProvider().heDlMuMemberTransmitted(token, member, false))
        return;
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
void HeHcfRuntime::heDlMuUserOutcome(uint64_t token, const MacAddress& peer,
        HeDlMuUserOutcome outcome)
{
    getHeDlMuExchangeProvider().heDlMuUserOutcome(token, peer, outcome, false);
}
void HeHcfRuntime::heDlMuPlanningFailed(uint64_t token, AccessCategory ac)
{
    if (!getHeDlMuExchangeProvider().heDlMuPlanningFailed(token, ac, false))
        return;
    EV_WARN << "DL MU planning failed for AC " << ac
            << "; provider scheduled an exact next-TXOP single-user fallback\n";
}
} // namespace ieee80211
} // namespace inet
