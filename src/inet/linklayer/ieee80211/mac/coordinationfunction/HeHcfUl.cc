//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfObservationSink.h"

#include <algorithm>
#include <optional>
#include <sstream>

#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
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
#include "inet/linklayer/ieee80211/mac/contract/ISequenceNumberAssignment.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF uplink MU support.

namespace inet {
namespace ieee80211 {

uint32_t HeHcfRuntime::getBufferedTrafficServiceBytes(
        Edcaf *edcaf, const MacAddress& peer, int tid) const
{
    std::vector<Packet *> triggeredPackets;
    auto pendingQueue = edcaf->getPendingQueue();
    for (const auto& entry : getHeTriggeredUlExchangeService().getExchanges())
        if (resolveHeQueue(entry.second.sourceQueueToken) == pendingQueue)
            triggeredPackets.insert(triggeredPackets.end(),
                    entry.second.packets.begin(), entry.second.packets.end());
    return calculateBufferedTrafficServiceBytes(
            edcaf, peer, tid, triggeredPackets);
}

std::optional<physicallayer::Ieee80211HeTxopDuration>
getIeee80211HeSolicitingTxopDuration(const Packet *packet)
{
    auto indication = packet == nullptr ? nullptr :
            packet->findTag<physicallayer::Ieee80211HeRxVectorInd>();
    if (indication == nullptr || indication->getRxVector() == nullptr)
        return std::nullopt;
    return indication->getRxVector()->getCommon().getTxopDuration();
}

double computeIeee80211HeTriggerPathLossDb(int apTxPowerDbm20Mhz,
        W receivedPower, Hz receivedBandwidth)
{
    if (receivedPower <= W(0) || receivedBandwidth < MHz(20))
        throw cRuntimeError("Cannot compute HE Trigger path loss from nonpositive power or bandwidth below 20 MHz");
    const double receivedPowerDbm = math::mW2dBmW(receivedPower.get<mW>());
    const double receivedPowerDbm20Mhz = receivedPowerDbm -
            10 * std::log10(receivedBandwidth.get() / 20e6);
    return apTxPowerDbm20Mhz - receivedPowerDbm20Mhz;
}

W computeIeee80211HeTbTransmitPower(W maximumPower, int targetReceivePowerDbm,
        double pathLossDb, bool useMaximumTransmitPower)
{
    if (useMaximumTransmitPower)
        return maximumPower;
    if (!std::isfinite(pathLossDb))
        return maximumPower;
    W requestedPower = mW(math::dBmW2mW(targetReceivePowerDbm + pathLossDb));
    return std::min(requestedPower, maximumPower);
}

static AccessCategory aciToAccessCategory(uint8_t aci)
{
    switch (aci) {
        case 0: return AC_BE;
        case 1: return AC_BK;
        case 2: return AC_VI;
        case 3: return AC_VO;
        default: throw cRuntimeError("Invalid Preferred AC in Basic Trigger");
    }
}

std::optional<std::string> validateIeee80211HeUlTrigger(
        const Ieee80211TriggerFrame& trigger, Hz centerFrequency)
{
    using namespace physicallayer;
    const auto triggerType = trigger.getTriggerType();
    if (triggerType != IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER)
        return "unsupported Trigger type";
    const auto bandwidth = Hz(trigger.getChannelBandwidthMhz() * 1e6);
    if (bandwidth != MHz(20) && bandwidth != MHz(40) &&
            bandwidth != MHz(80) && bandwidth != MHz(160))
        return "unsupported Trigger bandwidth";
    if (trigger.getUlLength() > 4095 || trigger.getUlLength() % 3 != 1 ||
            trigger.getCommonDuration() <= SIMTIME_ZERO ||
            trigger.getCommonDuration() > SimTime(5.484, SIMTIME_MS))
        return "invalid UL Length or common duration";
    const bool validGiLtf =
            (trigger.getGuardInterval() == HE_GI_1_6_US &&
             (trigger.getLtfType() == HE_LTF_1X || trigger.getLtfType() == HE_LTF_2X)) ||
            (trigger.getGuardInterval() == HE_GI_3_2_US &&
             trigger.getLtfType() == HE_LTF_4X);
    if (!validGiLtf || (trigger.getNumberOfHeLtfSymbols() != 1 &&
            trigger.getNumberOfHeLtfSymbols() != 2 &&
            trigger.getNumberOfHeLtfSymbols() != 4 &&
            trigger.getNumberOfHeLtfSymbols() != 6 &&
            trigger.getNumberOfHeLtfSymbols() != 8) ||
            trigger.getPreFecPaddingFactor() < 1 ||
            trigger.getPreFecPaddingFactor() > 4 ||
            trigger.getApTxPowerDbm() < -20 || trigger.getApTxPowerDbm() > 40)
        return "invalid Trigger common signaling";
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        if (trigger.getUsersArraySize() != 0 || trigger.getNfrpFeedbackType() != 0 ||
                trigger.getNfrpStartingAid() > 4095 ||
                trigger.getGuardInterval() != HE_GI_3_2_US ||
                trigger.getLtfType() != HE_LTF_4X ||
                trigger.getNumberOfHeLtfSymbols() != 2)
            return "invalid NFRP Trigger fields";
        try {
            if (trigger.getNfrpStartingAid() +
                    IIeee80211HeUlScheduler::getNfrpScheduledStaCount(
                            bandwidth, trigger.getNfrpMultiplexingFlag()) > 4096)
                return "NFRP AID range exceeds 12 bits";
        }
        catch (const std::exception&) {
            return "invalid NFRP bandwidth";
        }
        return std::nullopt;
    }
    if (trigger.getUsersArraySize() == 0)
        return "Basic/BSRP Trigger contains no User Info records";

    auto catalog = getHeRuAllocationCatalog(centerFrequency, bandwidth);
    std::set<uint16_t> scheduledAids;
    std::map<std::pair<int, int>, std::vector<const Ieee80211HeTriggerUserInfo *>> usersPerRu;
    std::vector<Ieee80211HeRu> physicalRus;
    for (unsigned int i = 0; i < trigger.getUsersArraySize(); ++i) {
        const auto& user = trigger.getUsers(i);
        auto canonical = std::find_if(catalog.begin(), catalog.end(), [&] (const auto& ru) {
            return ru.index == user.ruIndex && ru.toneSize == user.ruToneSize &&
                    ru.toneOffset == user.ruToneOffset;
        });
        if (canonical == catalog.end())
            return "User Info RU is not canonical";
        if (user.mcs > 11 || user.numberOfSpatialStreams < 1 ||
                user.numberOfSpatialStreams > 8 || user.streamStartIndex > 7 ||
                user.streamStartIndex + user.numberOfSpatialStreams > 8)
            return "invalid User Info MCS or spatial streams";
        if (!user.useMaximumTransmitPower &&
                (user.targetRssiDbm < -110 || user.targetRssiDbm > -20))
            return "invalid User Info target RSSI";
        if (user.randomAccess) {
            if (user.aid != 0 || user.muMimo ||
                    user.numberOfSpatialStreams != 1 || user.streamStartIndex != 0)
                return "invalid associated-STA random-access User Info";
        }
        else if (user.aid == 0 || user.aid > 2007 ||
                !scheduledAids.insert(user.aid).second)
            return "invalid or duplicate scheduled AID";
        if (user.coding == HE_CODING_BCC &&
                (user.mcs > 9 || user.numberOfSpatialStreams > 4 ||
                 user.ruToneSize >= 484))
            return "invalid BCC User Info";
        auto geometry = std::make_pair(user.ruToneSize, user.ruToneOffset);
        if (usersPerRu[geometry].empty())
            physicalRus.push_back(*canonical);
        usersPerRu[geometry].push_back(&user);
    }
    if (!validateHeRuLayout(physicalRus, bandwidth))
        return "overlapping or out-of-band Trigger RU layout";
    const auto fullRu = getHeEqualRuLayout(centerFrequency, bandwidth, 1).front();
    bool fullBandwidthUlMuMimo = physicalRus.size() == 1;
    for (const auto& entry : usersPerRu) {
        const auto& users = entry.second;
        if (users.size() == 1) {
            if (users.front()->muMimo || users.front()->streamStartIndex != 0)
                return "single-user RU cannot use MU-MIMO or a nonzero starting stream";
            fullBandwidthUlMuMimo = false;
            continue;
        }
        if (users.size() > 8 || entry.first.first != fullRu.toneSize ||
                entry.first.second != fullRu.toneOffset)
            return "UL MU-MIMO requires at most eight users on the full-bandwidth RU";
        std::set<int> streams;
        for (const auto user : users) {
            if (!user->muMimo || user->randomAccess ||
                    user->numberOfSpatialStreams > 4)
                return "shared RU is not scheduled UL MU-MIMO";
            for (int stream = user->streamStartIndex;
                    stream < user->streamStartIndex + user->numberOfSpatialStreams; ++stream)
                if (!streams.insert(stream).second)
                    return "UL MU-MIMO spatial streams overlap";
        }
        if (streams.empty() || streams.size() > 8 || *streams.begin() != 0 ||
                *streams.rbegin() + 1 != static_cast<int>(streams.size()))
            return "UL MU-MIMO spatial streams are gapped or exceed eight streams";
    }
    if (trigger.getLtfType() == HE_LTF_1X && !fullBandwidthUlMuMimo)
        return "1x HE-LTF requires full-bandwidth UL MU-MIMO";
    return std::nullopt;
}

bool HeHcfRuntime::allAssociatedStationsSupportPreamblePuncturing() const
{
    const auto stations = mac->getMib()->getPeerAssociationSnapshots();
    return std::all_of(stations.begin(), stations.end(), [&] (const auto& station) {
                auto capabilities = mac->getMib()->getNegotiatedHeCapabilities(station.getAddress());
                return !station.hasMemberStatus() || station.getMemberStatus() != Ieee80211Mib::ASSOCIATED ||
                        (capabilities && capabilities->localRxPeerTx.valid &&
                         capabilities->localRxPeerTx.preamblePuncturing);
            });
}

bool HeHcfRuntime::supportsPreamblePuncturing(const IIeee80211HeUlScheduler::RuAllocation& allocation) const
{
    if (allocation.randomAccess)
        return allAssociatedStationsSupportPreamblePuncturing();
    auto capabilities = mac->getMib()->getNegotiatedHeCapabilities(allocation.staAddress);
    return capabilities && capabilities->localRxPeerTx.valid && capabilities->localRxPeerTx.preamblePuncturing;
}

HeUlScheduleFinalizationResult HeHcfRuntime::finalizeUlSchedule(
        const IIeee80211HeUlScheduler::Schedule& proposedSchedule,
        Hz centerFrequency, Hz channelBandwidth,
        IIeee80211HeUlTriggerPolicy::TriggerType triggerType)
{
    return HeUlTriggerService::finalizeSchedule(proposedSchedule, centerFrequency,
            channelBandwidth, triggerType);
}

HeUlPreparationSnapshot HeHcfRuntime::captureHeUlPreparationSnapshot(
        AccessCategory accessCategory) const
{
    HeUlPreparationSnapshot snapshot;
    snapshot.accessCategory = accessCategory;
    snapshot.now = simTime();
    snapshot.phy.emplace(getLinkPhyContext().getSnapshot());
    snapshot.maxHeTbPpduDuration = SimTime(par("maxHeTbPpduDuration"));
    snapshot.reportMaxAge = ulCoordinator->getReportMaxAge();
    snapshot.targetRssiMarginDb = par("ulTargetRssiMargin").doubleValue();
    snapshot.enableUlMuMimo = par("enableUlMuMimo").boolValue();
    auto schedulerModule = getSubmodule("ulScheduler");
    snapshot.maxMuStations = schedulerModule == nullptr ?
            physicallayer::getHeMaxRuCount(snapshot.phy->getChannelBandwidth()) :
            schedulerModule->par("maxMuStations").intValue();
    auto edcaf = edca->getEdcaf(accessCategory);
    auto txop = edcaf == nullptr ? nullptr : edcaf->getTxopProcedure();
    if (txop != nullptr && txop->getLimit() > SIMTIME_ZERO)
        snapshot.txopLimit = std::max(SIMTIME_ZERO,
                txop->getLimit() - txop->getDuration());

    for (const auto& station : mac->getMib()->getPeerAssociationSnapshots()) {
        if (!station.hasMemberStatus() ||
                station.getMemberStatus() != Ieee80211Mib::ASSOCIATED)
            continue;
        HeUlPeerPreparationSnapshot peer;
        peer.stationAddress = station.getAddress();
        peer.associationId = mac->getMib()->getAssociationId(peer.stationAddress);
        peer.twtEligible = !isTwtSleeping(mac, peer.stationAddress);
        Ieee80211HeOperatingMode operatingMode;
        peer.ulMuDisabled = getPeerOperatingMode(peer.stationAddress, operatingMode) &&
                operatingMode.ulMuDisable;
        peer.negotiatedCapabilities =
                mac->getMib()->getNegotiatedHeCapabilities(peer.stationAddress);
        const auto link = getLinkPhyContext().getPeerSnapshot(peer.stationAddress,
                SimTime(par("linkEstimateMaxAge")));
        peer.pathLossDb = link.getPathLossDb();
        peer.hasFreshPathLoss = link.getHasFreshPathLoss();
        auto report = ulCoordinator->getBufferStatusSnapshot(
                peer.associationId, peer.stationAddress);
        if (report) {
            HeUlBufferStatusSnapshot copied;
            copied.stationAddress = report->stationAddress;
            copied.backlogBytes = report->backlogBytes;
            copied.backlogEstimates = report->backlogEstimates;
            copied.tid = report->tid;
            copied.updateTime = report->updateTime;
            copied.lastService = report->lastService;
            peer.bufferStatus = copied;
        }
        snapshot.peers.push_back(std::move(peer));
    }
    return snapshot;
}

bool HeHcfRuntime::tryStartUlMuFrameSequence(AccessCategory accessCategory)
{
    if (!mac->isApInHeFamily())
        return false;
    return ulTriggerService.tryStart(accessCategory,
            captureHeUlPreparationSnapshot(accessCategory));
}

void HeHcfRuntime::configureHeUlMuProtection(AccessCategory accessCategory)
{
    auto txop = edca->getEdcaf(accessCategory)->getTxopProcedure();
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
}

void HeHcfRuntime::startHeUlMuExchange(AccessCategory accessCategory,
        const HeUlMuPlan& plan, IHeUlMuExchangeCallback *callback)
{
    startExchangeFrameSequence(
            new HeUlMuTxOpFs(callback, plan, modeSet, mac->getAddress()),
            buildContext(accessCategory));
    heUlMuExchangeActive = true;
}

void HeHcfRuntime::processTriggeredUlFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& header, uint16_t aid)
{
    HcfObservationSink::packetReceivedFromPeer(hcf, packet);
    if (header->getBufferStatusPresent())
    {
        Ieee80211HeQueueSizeEstimate estimate;
        estimate.kind = static_cast<Ieee80211HeQueueSizeKind>(
                header->getBufferStatusQueueSizeKind());
        estimate.lowerBoundBytes = header->getBufferStatusQueueSizeLowerBound();
        estimate.upperBoundBytes = header->getBufferStatusQueueSizeUpperBound();
        estimate.hasUpperBound = header->getBufferStatusQueueSizeHasUpperBound();
        if (estimate.lowerBoundBytes == 0 &&
                header->getBufferStatusQueueSize() != 0) {
            estimate.lowerBoundBytes = header->getBufferStatusQueueSize();
            estimate.upperBoundBytes = estimate.lowerBoundBytes;
            estimate.hasUpperBound = true;
        }
        ulCoordinator->updateBufferStatus(aid, header->getTransmitterAddress(),
                static_cast<AccessCategory>(header->getBufferStatusAc()),
                header->getBufferStatusTid(), estimate);
    }
    if (header->getType() == ST_QOS_NULL) {
        delete packet;
        return;
    }
    if (recipientBlockAckAgreementHandler != nullptr) {
        auto agreement = recipientBlockAckAgreementHandler->getAgreement(header->getTid(), header->getTransmitterAddress());
        if (agreement != nullptr)
            recipientBlockAckAgreementHandler->qosFrameReceived(header, hcf);
    }
    // The Trigger exchange acknowledges all collected responses with one Multi-STA
    // Block Ack. Deliver the data through the normal QoS receive service without
    // invoking Hcf::recipientProcessReceivedFrame(), which would schedule a
    // legacy per-frame Ack while the collection sequence is still running.
    // This exchange carries its own per-user acknowledgment record. Do not
    // hold the decoded MPDU in the legacy single-user Block Ack reorder
    // buffer, whose sequence window may be advancing independently through
    // ordinary EDCA transmissions.
    sendUp(recipientDataService->dataFrameReceived(packet, header, nullptr));
}
Ptr<Ieee80211CompressedBlockAck> buildHeMuBarCompressedBlockAck(
        const Ieee80211HeTriggerUserInfo& user, RecipientBlockAckAgreement *agreement,
        const MacAddress& receiverAddress, const MacAddress& transmitterAddress)
{
    ASSERT(agreement != nullptr);
    ASSERT(user.muBarCompressedBitmap && !user.muBarMultiTid);
    // The MU-BAR User Info field embeds a Compressed BlockAckReq whose
    // Starting Sequence Number selects the 64-MPDU response bitmap window
    // (9.3.1.22.4 and 9.3.1.9).  Using the agreement's initial window here
    // makes every later response repeat the first bitmap and eventually
    // exhausts the originator's BA window.
    auto agreementSnapshot = agreement->getSnapshot();
    auto startingSequenceNumber = SequenceNumberCyclic(user.muBarStartingSequenceNumber);
    std::vector<uint8_t> bytes(8, 0);
    BitVector bitmap(bytes);
    for (int i = 0; i < 64; ++i) {
        bool ackState = agreementSnapshot.record.getAckState(startingSequenceNumber + i, 0);
        bitmap.setBit(i, ackState);
    }
    if (!agreement->isSnapshotCurrent(agreementSnapshot))
        throw cRuntimeError("Recipient Block Ack agreement changed while building MU-BAR response");
    auto blockAck = makeShared<Ieee80211CompressedBlockAck>();
    blockAck->setReceiverAddress(receiverAddress);
    blockAck->setTransmitterAddress(transmitterAddress);
    blockAck->setCompressedBitmap(true);
    blockAck->setStartingSequenceNumber(startingSequenceNumber);
    blockAck->setTidInfo(user.muBarTidInfo);
    blockAck->setBlockAckBitmap(bitmap);
    blockAck->setDurationField(SIMTIME_ZERO);
    return blockAck;
}

const Ptr<Ieee80211CompressedBlockAck> HeHcfRuntime::processTriggeredUlBlockAckReq(
        Packet *packet,
        const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
        uint16_t aid)
{
    ASSERT(packet != nullptr);
    ASSERT(blockAckReq != nullptr);
    auto agreement = recipientBlockAckAgreementHandler == nullptr ? nullptr :
            recipientBlockAckAgreementHandler->getAgreement(
                    blockAckReq->getTidInfo(),
                    blockAckReq->getTransmitterAddress());
    if (agreement == nullptr) {
        delete packet;
        return nullptr;
    }
    sendUp(recipientDataService->controlFrameReceived(packet, blockAckReq,
            recipientBlockAckAgreementHandler.get()));
    auto blockAck = recipientBlockAckProcedure->buildBlockAck(
            blockAckReq, recipientBlockAckAgreementHandler.get());
    delete packet;
    auto compressedBlockAck = dynamicPtrCast<Ieee80211CompressedBlockAck>(blockAck);
    if (compressedBlockAck == nullptr)
        throw cRuntimeError("A compressed HE-TB BAR produced a non-compressed BlockAck");
    compressedBlockAck->setTransmitterAddress(mac->getAddress());
    EV_INFO << "Processed correlated HE-TB compressed BAR: AID=" << aid
            << ", TID=" << blockAckReq->getTidInfo()
            << ", SSN=" << blockAckReq->getStartingSequenceNumber().get() << "\n";
    return compressedBlockAck;
}

void HeHcfRuntime::processReceivedTriggerFrame(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger)
{
    const auto phy = getLinkPhyContext().getSnapshot();
    HeTriggeredUlExchangeService::TriggerProcessingSnapshot snapshot;
    snapshot.packet = packet;
    snapshot.trigger = trigger;
    snapshot.reception.bssid = mac->getMib()->getBssid();
    snapshot.reception.associationId = mac->getMib()->getLocalAssociationId();
    snapshot.reception.associationEpoch = getHePeerStateService().getPeerSnapshot(
            snapshot.reception.bssid).getAssociationEpoch();
    snapshot.reception.ulEnabled = ulCoordinator->isEnabled();
    snapshot.reception.accessPoint = mac->isApInHeFamily();
    snapshot.reception.twtSleeping = isTwtSleeping(mac, snapshot.reception.bssid);
    snapshot.reception.ndpFeedbackEnabled =
            mac->getMib()->localHeCapabilities.ndpFeedbackReport;
    snapshot.reception.centerFrequency = phy.getChannelCenterFrequency();
    snapshot.reception.channelBandwidth = phy.getChannelBandwidth();
    snapshot.solicitingTxopDuration = getIeee80211HeSolicitingTxopDuration(packet);
    if (auto signalPower = packet->findTag<SignalPowerInd>()) {
        auto modeInd = packet->findTag<physicallayer::Ieee80211ModeInd>();
        if (modeInd != nullptr && modeInd->getMode() != nullptr)
            snapshot.triggerPathLossDb = computeIeee80211HeTriggerPathLossDb(
                    trigger->getApTxPowerDbm(), signalPower->getPower(),
                    modeInd->getMode()->getDataMode()->getBandwidth());
    }
    snapshot.negotiatedCapabilities =
            mac->getMib()->getNegotiatedHeCapabilities(snapshot.reception.bssid);
    snapshot.localAddress = mac->getAddress();
    snapshot.maximumTransmitPower = phy.getMaximumTransmitPower();
    snapshot.bssColor = mac->getMib()->heOperation.bssColor;
    snapshot.fcsMode = mac->getFcsMode();
    snapshot.ulMuDisabled = par("operatingModeUlMuDisable").boolValue();
    snapshot.currentTime = simTime();
    snapshot.sifsTime = modeSet->getSifsTime();
    snapshot.slotTime = modeSet->getSlotTime();
    snapshot.maximumBlockAckTxTime =
            modeSet->getSlowestMandatoryMode()->getDuration(
                    B(18 + 12 * trigger->getUsersArraySize() + 4));
    snapshot.responseSelection.triggerType =
            static_cast<IIeee80211HeUlTriggerPolicy::TriggerType>(
                    trigger->getTriggerType());

    for (int ac = AC_BK; ac <= AC_VO; ++ac) {
        auto accessCategory = static_cast<AccessCategory>(ac);
        auto edcaf = edca->getEdcaf(accessCategory);
        auto queue = edcaf->getPendingQueue();
        HeTriggeredUlExchangeService::AccessQueueSnapshot queueSnapshot;
        queueSnapshot.accessCategory = accessCategory;
        queueSnapshot.queueToken = getHeQueueService().getQueueToken(queue,
                snapshot.reception.bssid, snapshot.reception.associationEpoch,
                accessCategory);
        for (int i = 0; i < queue->getNumPackets(); ++i)
            queueSnapshot.packets.push_back(queue->getPacket(i));
        auto frames = edcaf->getInProgressFrames();
        for (int i = 0; i < frames->getLength() &&
                queueSnapshot.inProgressTid < 0; ++i) {
            auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                    frames->getFrames(i)->peekAtFront());
            if (header != nullptr && header->getType() == ST_DATA_WITH_QOS &&
                    header->getReceiverAddress() == snapshot.reception.bssid)
                queueSnapshot.inProgressTid = header->getTid();
        }
        snapshot.responseSelection.queues.push_back(std::move(queueSnapshot));
        addBufferedTrafficServiceBytes(snapshot.totalBufferedBytes,
                getBufferedTrafficServiceBytes(edcaf, snapshot.reception.bssid));
        for (int tid = 0; tid != 8; ++tid) {
            HeTriggeredUlExchangeService::TidTrafficSnapshot traffic;
            traffic.accessCategory = accessCategory;
            traffic.tid = tid;
            traffic.bufferedBytes = getBufferedTrafficServiceBytes(
                    edcaf, snapshot.reception.bssid, tid);
            auto agreement = originatorBlockAckAgreementHandler == nullptr ?
                    nullptr : originatorBlockAckAgreementHandler->getAgreement(
                            snapshot.reception.bssid, tid);
            traffic.hasBlockAckAgreement = agreement != nullptr;
            if (agreement != nullptr) {
                auto agreementSnapshot = agreement->getSnapshot();
                int occupiedSlots = edcaf->getAckHandler()->
                        getNumOccupiedBlockAckSequencePositions(
                                snapshot.reception.bssid, tid);
                traffic.availableBlockAckSlots = std::max(0,
                        agreementSnapshot.bufferSize - occupiedSlots);
            }
            snapshot.traffic.push_back(traffic);
        }
    }
    snapshot.blockAckCandidates = captureTriggeredUlBlockAckCandidates();
    getHeTriggeredUlExchangeService().processTrigger(std::move(snapshot));
}
void HeHcfRuntime::processReceivedMultiStaBlockAck(Packet *packet, const Ptr<const Ieee80211MultiStaBlockAck>& multiStaBlockAck)
{
    getHeTriggeredUlExchangeService().processMultiStaBlockAck(packet,
            multiStaBlockAck);
}

simtime_t HeHcfRuntime::getTriggeredUlCurrentTime() const
{
    return simTime();
}

void HeHcfRuntime::scheduleTriggeredUlTimer(simtime_t time, cMessage *timer)
{
    omnetpp::cMethodCallContextSwitcher context(hcf);
    context.methodCallSilent();
    scheduleAt(time, timer);
}

void HeHcfRuntime::cancelTriggeredUlTimer(cMessage *timer)
{
    omnetpp::cMethodCallContextSwitcher context(hcf);
    context.methodCallSilent();
    cancelEvent(timer);
}

void HeHcfRuntime::cancelAndDeleteTriggeredUlTimer(cMessage *timer)
{
    omnetpp::cMethodCallContextSwitcher context(hcf);
    context.methodCallSilent();
    cancelAndDelete(timer);
}

MacAddress HeHcfRuntime::getTriggeredUlBssid() const
{
    return mac->getMib()->getBssid();
}

MacAddress HeHcfRuntime::getTriggeredUlLocalAddress() const
{
    return mac->getAddress();
}

uint16_t HeHcfRuntime::getTriggeredUlAssociationId() const
{
    return mac->getMib()->getLocalAssociationId();
}

uint64_t HeHcfRuntime::getTriggeredUlAssociationEpoch() const
{
    return getHePeerStateService().getPeerSnapshot(
            mac->getMib()->getBssid()).getAssociationEpoch();
}

std::unique_ptr<ISequenceNumberAssignment>
HeHcfRuntime::cloneTriggeredUlSequenceState() const
{
    return originatorDataService->cloneSequenceNumberState();
}

void HeHcfRuntime::commitTriggeredUlSequenceState(
        const ISequenceNumberAssignment& state)
{
    originatorDataService->commitSequenceNumberState(state);
}

Ptr<Ieee80211CompressedBlockAckReq>
HeHcfRuntime::materializeTriggeredUlBlockAckRequest(
        const HeTriggeredUlExchangeService::BlockAckRequestSelection& selection)
{
    if (originatorBlockAckProcedure == nullptr)
        return nullptr;
    return dynamicPtrCast<Ieee80211CompressedBlockAckReq>(
            originatorBlockAckProcedure->buildCompressedBlockAckReqFrame(
                    selection.receiverAddress, selection.tid,
                    selection.startingSequenceNumber));
}

std::vector<HeTriggeredUlExchangeService::BlockAckCandidateSnapshot>
HeHcfRuntime::captureTriggeredUlBlockAckCandidates() const
{
    std::vector<HeTriggeredUlExchangeService::BlockAckCandidateSnapshot> result;
    const auto receiverAddress = mac->getMib()->getBssid();
    for (int ac = AC_BK; ac <= AC_VO; ++ac) {
        const auto accessCategory = static_cast<AccessCategory>(ac);
        auto outstandingFrames = edca->getEdcaf(accessCategory)->
                getInProgressFrames()->getOutstandingFrames();
        for (auto packet : outstandingFrames) {
            HeTriggeredUlExchangeService::BlockAckCandidateSnapshot candidate;
            candidate.accessCategory = accessCategory;
            auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                    packet->peekAtFront<Ieee80211MacHeader>());
            candidate.hasDataHeader = header != nullptr;
            if (header != nullptr) {
                candidate.qosData = header->getType() == ST_DATA_WITH_QOS;
                candidate.receiverAddress = header->getReceiverAddress();
                candidate.tid = header->getTid();
                candidate.fragmentNumber = header->getFragmentNumber();
                candidate.sequenceNumber = header->getSequenceNumber();
                auto agreement = originatorBlockAckAgreementHandler == nullptr ?
                        nullptr : originatorBlockAckAgreementHandler->getAgreement(
                                receiverAddress, header->getTid());
                candidate.hasAgreement = agreement != nullptr;
                if (agreement != nullptr) {
                    auto agreementSnapshot = agreement->getSnapshot();
                    candidate.addbaResponseReceived =
                            agreementSnapshot.isAddbaResponseReceived;
                    candidate.agreementStartingSequenceNumber =
                            agreementSnapshot.startingSequenceNumber;
                }
            }
            result.push_back(candidate);
        }
    }
    return result;
}

void HeHcfRuntime::commitTriggeredUlBlockAckRequest(
        const Ptr<Ieee80211CompressedBlockAckReq>& blockAckReq,
        AccessCategory accessCategory)
{
    edca->getEdcaf(accessCategory)->getAckHandler()->
            processTransmittedBlockAckReq(blockAckReq);
}

void HeHcfRuntime::validateTriggeredUlPackets(HcfQueueToken queueToken,
        const std::vector<Packet *>& packets) const
{
    auto reservation = getHeQueueService().preparePacketReservation(
            queueToken, packets);
    getHeQueueService().rollbackPacketReservation(reservation);
}

std::vector<Packet *> HeHcfRuntime::commitTriggeredUlPackets(
        HcfQueueToken queueToken, const std::vector<Packet *>& originals,
        const std::vector<Packet *>& prepared)
{
    auto reservation = getHeQueueService().preparePacketReservation(
            queueToken, originals);
    return getHeQueueService().commitPacketReservation(reservation, prepared);
}

void HeHcfRuntime::rollbackTriggeredUlPackets(HcfQueueToken queueToken,
        const std::vector<Packet *>& originals,
        const std::vector<Packet *>& backups,
        const std::vector<Packet *>& queueOrder)
{
    getHeQueueService().restoreCommittedPackets(queueToken, originals,
            backups, queueOrder);
}

void HeHcfRuntime::takeTriggeredUlPacket(Packet *packet) { take(packet); }
std::unique_ptr<ITx::PreparedTransmission> HeHcfRuntime::prepareTriggeredUlHandoff(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header)
{
    return tx->prepareTransmission(packet, header, modeSet->getSifsTime(), hcf);
}
void HeHcfRuntime::commitTriggeredUlHandoff(
        std::unique_ptr<ITx::PreparedTransmission> prepared) noexcept
{
    tx->commitTransmission(std::move(prepared));
}

Ptr<Ieee80211CompressedBlockAck> HeHcfRuntime::prepareTriggeredUlMuBarBlockAck(
        const Ieee80211HeTriggerUserInfo& user,
        const MacAddress& originator)
{
    auto agreement = recipientBlockAckAgreementHandler == nullptr ? nullptr :
            recipientBlockAckAgreementHandler->getAgreement(
                    user.muBarTidInfo, originator);
    return agreement == nullptr ? nullptr : buildHeMuBarCompressedBlockAck(
            user, agreement, originator, mac->getAddress());
}
Hz HeHcfRuntime::getTriggeredUlCenterFrequency() const { return getLinkPhyContext().getSnapshot().getChannelCenterFrequency(); }
uint8_t HeHcfRuntime::getTriggeredUlBssColor() const { return mac->getMib()->heOperation.bssColor; }
FcsMode HeHcfRuntime::getTriggeredUlFcsMode() const { return mac->getFcsMode(); }
simtime_t HeHcfRuntime::getTriggeredUlSifsTime() const { return modeSet->getSifsTime(); }
AccessCategory HeHcfRuntime::mapTriggeredUlTidToAccessCategory(Tid tid) const { return edca->mapTidToAc(tid); }
HeTriggeredUlExchangeService::RandomAccessPreparation
HeHcfRuntime::prepareTriggeredUlRandomAccess(AccessCategory accessCategory,
        int randomAccessRuCount)
{
    auto prepared = ulCoordinator->prepareRandomAccessRu(accessCategory,
            randomAccessRuCount);
    return {prepared.accessCategory, prepared.randomAccessRuCount,
            prepared.originalBackoff, prepared.resultingBackoff,
            prepared.attempt};
}

int HeHcfRuntime::commitTriggeredUlRandomAccess(
        const HeTriggeredUlExchangeService::RandomAccessPreparation& preparation)
{
    HeUlCoordinator::PreparedRandomAccessSelection prepared;
    prepared.accessCategory = preparation.accessCategory;
    prepared.randomAccessRuCount = preparation.randomAccessRuCount;
    prepared.originalBackoff = preparation.originalBackoff;
    prepared.resultingBackoff = preparation.resultingBackoff;
    prepared.attempt = preparation.attempt;
    return ulCoordinator->commitRandomAccessRu(prepared);
}
void HeHcfRuntime::emitTriggeredUlResponse(HeTbResponseEvent& event) { emitHeTbResponse(event); }

void HeHcfRuntime::reportTriggeredUlRandomAccessResult(bool successful)
{
    ulCoordinator->reportRandomAccessResult(successful);
}

void HeHcfRuntime::processTriggeredUlBlockAckRequestFailure(
        const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
        AccessCategory accessCategory)
{
    processFailedBlockAckReq(edca->getEdcaf(accessCategory), blockAckReq, false);
}

void HeHcfRuntime::processTriggeredUlBlockAckRequestSuccess(
        const Ptr<const Ieee80211CompressedBlockAck>& blockAck,
        AccessCategory accessCategory)
{
    processReceivedBlockAck(edca->getEdcaf(accessCategory), blockAck,
            accessCategory);
}

void HeHcfRuntime::retireTriggeredUlPacket(Packet *packet,
        HcfPacketIdentity identity)
{
    if (packet == nullptr || HcfPacketIdentity(packet->getId()) != identity)
        throw cRuntimeError("Stale HE-TB packet identity on Block Ack retirement");
    auto header = packet->peekAtFront<Ieee80211DataHeader>();
    edca->getEdcaf(edca->mapTidToAc(header->getTid()))->getAckHandler()->
            retireFrame(header);
    delete packet;
}

void HeHcfRuntime::retryTriggeredUlPacket(Packet *packet,
        HcfPacketIdentity identity, HcfQueueToken sourceQueueToken,
        const MacAddress& bssid, uint16_t associationId,
        uint64_t associationEpoch)
{
    if (packet == nullptr || HcfPacketIdentity(packet->getId()) != identity)
        throw cRuntimeError("Stale HE-TB packet identity on retry");
    auto header = packet->peekAtFront<Ieee80211DataHeader>();
    auto edcaf = edca->getEdcaf(edca->mapTidToAc(header->getTid()));
    if (bssid != getTriggeredUlBssid() ||
            associationId != getTriggeredUlAssociationId() ||
            associationEpoch != getTriggeredUlAssociationEpoch()) {
        edcaf->getAckHandler()->retireFrame(
                packet->peekAtFront<Ieee80211DataHeader>());
        delete packet;
        return;
    }
    HcfRetryService::prepareTriggeredUlRetry(packet,
            edcaf->getRecoveryProcedure());
    if (!getHeQueueService().reinsertPacket(sourceQueueToken, identity, packet))
        throw cRuntimeError("Committed HE-TB queue token became invalid within the same association epoch");
}

void HeHcfRuntime::startTriggeredUlMuEdcaTimer(AccessCategory accessCategory)
{
    if (accessCategory >= AC_BK && accessCategory < AC_NUMCATEGORIES)
        edca->getEdcaf(accessCategory)->startMuEdcaTimer();
}
} // namespace ieee80211
} // namespace inet
