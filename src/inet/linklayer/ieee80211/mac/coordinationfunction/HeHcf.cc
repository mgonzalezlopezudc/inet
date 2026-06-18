//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

#include <algorithm>

#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

namespace {

inet::Ptr<const inet::ieee80211::Ieee80211DataHeader> getEligibleHoLDataHeader(inet::queueing::IPacketQueue *queue)
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

bool isMuEligibleDataHeader(const inet::Ptr<const inet::ieee80211::Ieee80211DataHeader>& dataHeader, inet::ieee80211::IOriginatorBlockAckAgreementHandler *baHandler)
{
    return dataHeader != nullptr &&
           dataHeader->getType() == inet::ieee80211::ST_DATA_WITH_QOS &&
           inet::ieee80211::hasActiveOriginatorBlockAckAgreement(baHandler, dataHeader->getReceiverAddress(), dataHeader->getTid());
}

inet::ieee80211::AccessCategory mapTidToAccessCategory(inet::ieee80211::Tid tid)
{
    switch (tid) {
        case 1:
        case 2: return inet::ieee80211::AC_BK;
        case 0:
        case 3: return inet::ieee80211::AC_BE;
        case 4:
        case 5: return inet::ieee80211::AC_VI;
        case 6:
        case 7: return inet::ieee80211::AC_VO;
        default: throw omnetpp::cRuntimeError("Invalid TID for HE UL scheduling: %d", tid);
    }
}

} // namespace

namespace inet {
namespace ieee80211 {

Define_Module(HeHcf);

HeHcf::~HeHcf()
{
    cancelAndDelete(ulTriggerTimer);
}

void HeHcf::initialize(int stage)
{
    Hcf::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        dlScheduler = check_and_cast<IIeee80211HeDlScheduler *>(getSubmodule("dlScheduler"));
        ulCoordinator = check_and_cast<HeUlCoordinator *>(getSubmodule("ulCoordinator"));
        ulTriggerTimer = new cMessage("heUlTriggerTimer");
        delete frameSequenceHandler;
        frameSequenceHandler = new HeFrameSequenceHandler();
    }
    else if (stage == INITSTAGE_LINK_LAYER && mac->isApInAxMode()) {
        queueBankManager = std::make_unique<StationQueueBankManager>(getSubmodule("queueBanks"));
        for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
            if (station.second == Ieee80211Mib::ASSOCIATED)
                queueBankManager->createQueueBank(station.first);
        }
        if (ulCoordinator->isEnabled())
            scheduleAfter(par("ulTriggerCheckInterval"), ulTriggerTimer);
    }
}

void HeHcf::handleMessage(cMessage *msg)
{
    if (msg != ulTriggerTimer) {
        Hcf::handleMessage(msg);
        return;
    }
    scheduleAfter(par("ulTriggerCheckInterval"), ulTriggerTimer);
    if (!mac->isApInAxMode() || !ulCoordinator->isEnabled() ||
            frameSequenceHandler->isSequenceRunning() || edca->getChannelOwner() != nullptr ||
            tx->isBusy() || ulTriggerAccessRequested)
        return;
    auto triggerType = ulCoordinator->selectTrigger(mac->getMib());
    if (triggerType == IIeee80211HeUlTriggerPolicy::NO_TRIGGER)
        return;
    pendingUlTrigger = triggerType;
    ulTriggerAccessRequested = true;
    auto ac = triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ?
            AC_BE : ulCoordinator->getPreferredAccessCategory();
    edca->requestChannelAccess(ac, this);
}

queueing::IPacketQueue *HeHcf::getPerStaQueue(const MacAddress& staAddr, AccessCategory ac)
{
    if (queueBankManager != nullptr) {
        auto staBank = queueBankManager->getQueueBank(staAddr);
        if (staBank != nullptr) {
            auto staQueue = staBank->getQueue((StationQueueBank::AccessCategory)ac);
            if (staQueue != nullptr) {
                EV_DEBUG << "Using per-STA queue for STA " << staAddr << " AC " << ac << "\n";
                return staQueue;
            }
            EV_WARN << "Could not get per-STA queue for STA " << staAddr << " AC " << ac << ", using shared queue\n";
        }
        else
            EV_DEBUG << "Queue bank not found for STA " << staAddr << ", using shared queue\n";
    }
    else
        EV_DEBUG << "Queue bank manager not available, using shared queue\n";
    return Hcf::getPerStaQueue(staAddr, ac);
}

StationQueueBank *HeHcf::createStationQueueBank(const MacAddress& staAddr)
{
    if (queueBankManager == nullptr) {
        EV_WARN << "Queue bank manager not initialized (not an 802.11ax AP?)\n";
        return nullptr;
    }
    return queueBankManager->createQueueBank(staAddr);
}

void HeHcf::destroyStationQueueBank(const MacAddress& staAddr)
{
    if (queueBankManager == nullptr) {
        EV_WARN << "Queue bank manager not initialized (not an 802.11ax AP?)\n";
        return;
    }
    queueBankManager->destroyQueueBank(staAddr);
}

StationQueueBank *HeHcf::getStationQueueBank(const MacAddress& staAddr) const
{
    return queueBankManager == nullptr ? nullptr : queueBankManager->getQueueBank(staAddr);
}

IIeee80211HeDlScheduler::ScheduleContext HeHcf::collectScheduleContext(AccessCategory ac) const
{
    IIeee80211HeDlScheduler::ScheduleContext context;
    auto radio = check_and_cast<physicallayer::IRadio *>(getContainingNicModule(this)->getSubmodule("radio"));
    auto transmitter = check_and_cast<const physicallayer::Ieee80211Transmitter *>(radio->getTransmitter());
    auto receiver = check_and_cast<const physicallayer::FlatReceiverBase *>(radio->getReceiver());
    auto channel = transmitter->getChannel();
    auto activeMode = transmitter->getMode();
    if (channel == nullptr || activeMode == nullptr)
        throw cRuntimeError("HE DL scheduling requires an active IEEE 802.11 channel and mode");
    context.channelNumber = channel->getChannelNumber();
    context.channelCenterFrequency = channel->getCenterFrequency();
    context.channelBandwidth = activeMode->getDataMode()->getBandwidth();
    context.totalTransmitPower = transmitter->getPower();
    context.receiverSensitivity = receiver->getSensitivity();
    context.noiseFigureDb = par("receiverNoiseFigure");
    context.maxAmpduMpduCount = par("maxAmpduMpduCount");
    if (auto heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(activeMode)) {
        switch (heMode->getDataMode()->getGuardIntervalType()) {
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT:
                context.guardInterval = physicallayer::HE_GI_0_8_US;
                break;
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM:
                context.guardInterval = physicallayer::HE_GI_1_6_US;
                break;
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG:
                context.guardInterval = physicallayer::HE_GI_3_2_US;
                break;
        }
    }
    auto edcaf = edca->getEdcaf(ac);
    auto txopProcedure = edcaf == nullptr ? nullptr : edcaf->getTxopProcedure();
    if (txopProcedure != nullptr && txopProcedure->getLimit() > SIMTIME_ZERO)
        context.txopLimit = std::max(SIMTIME_ZERO, txopProcedure->getLimit() - txopProcedure->getDuration());
    auto mib = mac->getMib();
    std::vector<MacAddress> seenDestinations;
    auto baHandler = getOriginatorBlockAckAgreementHandler();
    auto ackHandler = edcaf->getAckHandler();
    std::vector<queueing::IPacketQueue *> queues = {edcaf->getPendingQueue()};
    if (queueBankManager != nullptr) {
        for (const auto& entry : queueBankManager->getQueueBanks())
            queues.push_back(entry.second->getQueue((StationQueueBank::AccessCategory)ac));
    }

    for (auto queue : queues) {
        int n = queue->getNumPackets();
        for (int i = 0; i < n; ++i) {
            Packet *pkt = queue->getPacket(i);
            const auto& header = pkt->peekAtFront<Ieee80211MacHeader>();
            MacAddress dest = header->getReceiverAddress();
            if (dest.isMulticast() || dest.isBroadcast())
                continue;
            if (std::find(seenDestinations.begin(), seenDestinations.end(), dest) != seenDestinations.end())
                continue;

            auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
            if (!isMuEligibleDataHeader(dataHeader, baHandler) ||
                    !ackHandler->isEligibleToTransmit(dataHeader))
                continue;
            auto agreement = baHandler->getAgreement(dest, dataHeader->getTid());
            int occupiedSlots = ackHandler->getOccupiedBlockAckSequenceNumbers(
                    dest, dataHeader->getTid()).size();
            int availableSlots = agreement == nullptr ? 0 :
                    std::max(0, agreement->getBufferSize() - occupiedSlots);
            if (availableSlots == 0)
                continue;
            seenDestinations.push_back(dest);

            IIeee80211HeDlScheduler::CandidateInfo candidate;
            candidate.staAddress = dest;
            candidate.accessCategory = ac;
            candidate.holPacketBytes = pkt->getByteLength();
            auto enqueueTimeTag = pkt->findTag<OrigEnqueueTimeTag>();
            candidate.holEnqueueTime = enqueueTimeTag == nullptr ? pkt->getArrivalTime() : enqueueTimeTag->getEnqueueTime();
            candidate.holDelay = simTime() - candidate.holEnqueueTime;
            candidate.sourceQueue = queue;
            int eligiblePackets = 0;
            for (auto backlogQueue : queues) {
                for (int j = 0; j < backlogQueue->getNumPackets(); ++j) {
                    Packet *queuedPacket = backlogQueue->getPacket(j);
                    auto queuedHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                            queuedPacket->peekAtFront<Ieee80211MacHeader>());
                    if (queuedHeader != nullptr &&
                            queuedHeader->getType() == ST_DATA_WITH_QOS &&
                            queuedHeader->getReceiverAddress() == dest &&
                            queuedHeader->getTid() == dataHeader->getTid() &&
                            ackHandler->isEligibleToTransmit(queuedHeader) &&
                            hasActiveOriginatorBlockAckAgreement(baHandler, dest, queuedHeader->getTid()) &&
                            eligiblePackets < std::min(availableSlots, context.maxAmpduMpduCount)) {
                        B subframeLength = B(4) + B(queuedPacket->getByteLength());
                        candidate.backlogBytes += subframeLength.get<B>();
                        if (eligiblePackets > 0)
                            candidate.backlogBytes += (4 - subframeLength.get<B>() % 4) % 4;
                        eligiblePackets++;
                    }
                }
            }
            if (auto link = mib->findStationLink(dest)) {
                candidate.pathLossDb = link->pathLossDb;
                candidate.hasFreshPathLoss = link->valid &&
                        simTime() - link->lastUpdate <= SimTime(par("linkEstimateMaxAge"));
            }
            context.candidates.push_back(candidate);
        }
    }

    std::stable_sort(context.candidates.begin(), context.candidates.end(),
            [] (const auto& left, const auto& right) {
                return left.holEnqueueTime < right.holEnqueueTime;
            });
    if (!context.candidates.empty()) {
        context.candidates.front().anchor = true;
        context.anchorSta = context.candidates.front().staAddress;
    }
    return context;
}

queueing::IPacketQueue *HeHcf::findOldestPerStaQueue(AccessCategory ac) const
{
    if (queueBankManager == nullptr)
        return nullptr;

    queueing::IPacketQueue *oldestQueue = nullptr;
    simtime_t oldestEnqueueTime = SIMTIME_MAX;
    for (const auto& entry : queueBankManager->getQueueBanks()) {
        auto queue = entry.second->getQueue((StationQueueBank::AccessCategory)ac);
        if (queue->isEmpty())
            continue;
        auto packet = queue->getPacket(0);
        auto enqueueTimeTag = packet->findTag<OrigEnqueueTimeTag>();
        auto enqueueTime = enqueueTimeTag == nullptr ? packet->getArrivalTime() : enqueueTimeTag->getEnqueueTime();
        if (oldestQueue == nullptr || enqueueTime < oldestEnqueueTime) {
            oldestQueue = queue;
            oldestEnqueueTime = enqueueTime;
        }
    }
    return oldestQueue;
}

bool HeHcf::stagePerStaFrameForSingleUserTransmission(AccessCategory ac)
{
    auto sourceQueue = findOldestPerStaQueue(ac);
    if (sourceQueue == nullptr)
        return false;
    edca->getEdcaf(ac)->getPendingQueue()->enqueuePacket(sourceQueue->dequeuePacket());
    return true;
}

void HeHcf::startFrameSequence(AccessCategory ac)
{
    if (forceNextSingleUser[ac]) {
        forceNextSingleUser[ac] = false;
        Hcf::startFrameSequence(ac);
        return;
    }
    // Check whether HE mode and multi-user conditions are met.
    bool isHeMode = (modeSet != nullptr && strcmp(modeSet->getName(), "ax") == 0);
    if (isHeMode && pendingUlTrigger != IIeee80211HeUlTriggerPolicy::NO_TRIGGER &&
            mac->isApInAxMode() && ulCoordinator->isEnabled()) {
        ulTriggerAccessRequested = false;
        auto radio = check_and_cast<physicallayer::IRadio *>(getContainingNicModule(this)->getSubmodule("radio"));
        auto transmitter = check_and_cast<const physicallayer::NarrowbandTransmitterBase *>(radio->getTransmitter());
        auto receiver = check_and_cast<const physicallayer::FlatReceiverBase *>(radio->getReceiver());
        auto centerFrequency = transmitter->getCenterFrequency();
        Hz channelBandwidth = Hz(20e6);
        if (modeSet->getNumModes() > 0)
            if (auto heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(modeSet->getMode(0)))
                channelBandwidth = heMode->getDataMode()->getBandwidth();
        auto edcaf = edca->getEdcaf(ac);
        simtime_t txopLimit = SIMTIME_ZERO;
        if (edcaf->getTxopProcedure()->getLimit() > SIMTIME_ZERO)
            txopLimit = std::max(SIMTIME_ZERO,
                    edcaf->getTxopProcedure()->getLimit() - edcaf->getTxopProcedure()->getDuration());
        auto sensitivityDbm = math::mW2dBmW(receiver->getSensitivity().get<mW>());
        IIeee80211HeUlScheduler::Schedule ulSchedule;
        if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) {
            auto maxRus = physicallayer::getHeMaxRuCount(channelBandwidth);
            auto layout = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth, maxRus);
            int index = 0;
            for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
                if (station.second != Ieee80211Mib::ASSOCIATED || index >= maxRus)
                    continue;
                IIeee80211HeUlScheduler::RuAllocation allocation;
                allocation.staAddress = station.first;
                allocation.associationId = mac->getMib()->getAssociationId(station.first);
                allocation.ru = layout[index++];
                allocation.targetRssiDbm = (int)std::round(sensitivityDbm + (double)par("ulTargetRssiMargin"));
                ulSchedule.allocations.push_back(allocation);
            }
            ulSchedule.commonDuration = std::min(SimTime(par("maxHeTbPpduDuration")), txopLimit > SIMTIME_ZERO ?
                    txopLimit : SimTime(par("maxHeTbPpduDuration")));
        }
        else {
            int staleOrUnknown = 0;
            for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
                if (station.second != Ieee80211Mib::ASSOCIATED)
                    continue;
                auto aid = mac->getMib()->getAssociationId(station.first);
                auto status = ulCoordinator->getBufferStatus().find(aid);
                if (status == ulCoordinator->getBufferStatus().end() ||
                        simTime() - status->second.updateTime > ulCoordinator->getReportMaxAge())
                    staleOrUnknown++;
            }
            ulSchedule = ulCoordinator->createSchedule(mac->getMib(), centerFrequency, channelBandwidth,
                    txopLimit, sensitivityDbm, par("ulTargetRssiMargin"), staleOrUnknown, 0, 0);
        }
        auto triggerType = pendingUlTrigger;
        pendingUlTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
        if (!ulSchedule.allocations.empty()) {
            frameSequenceHandler->startFrameSequence(
                    new HeUlMuTxOpFs(ulCoordinator, this, ulSchedule, triggerType,
                            modeSet, mac->getAddress()),
                    buildContext(ac), this);
            emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
            return;
        }
    }
    if (isHeMode) {
        auto edcaf = edca->getEdcaf(ac);
        auto pendingQueue = edcaf->getPendingQueue();
        auto inProgress = edcaf->getInProgressFrames();
        if (inProgress->getLength() > 0) {
            EV_INFO << "HeHcf: completing " << inProgress->getLength()
                    << " recovery/outstanding frames before opening a new MU transmission." << endl;
            Hcf::startFrameSequence(ac);
            return;
        }
        auto headDataHeader = getEligibleHoLDataHeader(pendingQueue);
        auto baHandler = getOriginatorBlockAckAgreementHandler();
        if (!pendingQueue->isEmpty() && !isMuEligibleDataHeader(headDataHeader, baHandler)) {
            if (headDataHeader != nullptr) {
                EV_INFO << "HeHcf: earliest SU-transmittable packet "
                        << headDataHeader->getReceiverAddress() << " tid=" << headDataHeader->getTid()
                        << " is MU-ineligible, falling back to Hcf::startFrameSequence(ac)." << endl;
            }
            Hcf::startFrameSequence(ac);
            return;
        }
        auto scheduleContext = collectScheduleContext(ac);
        if (scheduleContext.candidates.size() >= 2) {
            auto previewAllocations = dlScheduler->schedule(scheduleContext);
            if (previewAllocations.size() < 2) {
                EV_INFO << "HeHcf: scheduler preview retained fewer than two MU users; falling back to SU." << endl;
                if (pendingQueue->isEmpty())
                    stagePerStaFrameForSingleUserTransmission(ac);
                Hcf::startFrameSequence(ac);
                return;
            }
            EV_INFO << "HeHcf: MU-OFDMA opportunity detected for " << scheduleContext.candidates.size()
                    << " STAs — starting HeDlMuTxOpFs." << endl;
            frameSequenceHandler->startFrameSequence(
                    new HeDlMuTxOpFs(dlScheduler, scheduleContext, modeSet,
                                     pendingQueue, edcaf->getAckHandler(), this,
                                     par("maxAmpduMpduCount"),
                                     par("maxHeMuPsduLength"),
                                     par("maxHeMuPpduDuration")),
                    buildContext(ac), this);
            emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
            return;
        }
        if (pendingQueue->isEmpty())
            stagePerStaFrameForSingleUserTransmission(ac);
    }
    // Fallback: standard single-user frame sequence.
    auto fallbackEdcaf = edca->getEdcaf(ac);
    if (fallbackEdcaf->getPendingQueue()->isEmpty() &&
            fallbackEdcaf->getInProgressFrames()->getLength() == 0)
        stagePerStaFrameForSingleUserTransmission(ac);
    if (fallbackEdcaf->getPendingQueue()->isEmpty() &&
            fallbackEdcaf->getInProgressFrames()->getLength() == 0) {
        EV_WARN << "HeHcf: channel granted without a pending SU, DL-MU, or UL trigger frame; releasing channel.\n";
        fallbackEdcaf->releaseChannel(this);
        fallbackEdcaf->getTxopProcedure()->endTxop();
        return;
    }
    Hcf::startFrameSequence(ac);
}

void HeHcf::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
{
    for (auto edcaf : internallyCollidedEdcafs) {
        if (edcaf->getPendingQueue()->isEmpty() && edcaf->getInProgressFrames()->getLength() == 0)
            stagePerStaFrameForSingleUserTransmission(edcaf->getAccessCategory());
    }
    Hcf::handleInternalCollision(internallyCollidedEdcafs);
}

bool HeHcf::hasFrameToTransmit(AccessCategory ac)
{
    if (Hcf::hasFrameToTransmit(ac))
        return true;
    if (queueBankManager == nullptr)
        return false;
    for (const auto& entry : queueBankManager->getQueueBanks()) {
        if (!entry.second->getQueue((StationQueueBank::AccessCategory)ac)->isEmpty())
            return true;
    }
    return false;
}

bool HeHcf::hasFrameToTransmit()
{
    auto edcaf = edca->getChannelOwner();
    return edcaf != nullptr && hasFrameToTransmit(edcaf->getAccessCategory());
}

uint16_t HeHcf::getAssociationId(const MacAddress& address) const
{
    auto aid = mac->getMib()->getAssociationId(address);
    return aid > 0 ? aid : 0;
}

void HeHcf::handleDlMuPlanningFailure(AccessCategory ac)
{
    forceNextSingleUser[ac] = stagePerStaFrameForSingleUserTransmission(ac);
}

void HeHcf::processTriggeredUlFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& header, uint16_t aid)
{
    emit(packetReceivedFromPeerSignal, packet);
    if (header->getBufferStatusPresent())
        ulCoordinator->updateBufferStatus(aid,
                static_cast<AccessCategory>(header->getBufferStatusAc()),
                header->getBufferStatusTid(), header->getBufferStatusQueueSize(), header->getRetry());
    if (header->getType() == ST_QOS_NULL) {
        delete packet;
        return;
    }
    if (recipientBlockAckAgreementHandler != nullptr) {
        auto agreement = recipientBlockAckAgreementHandler->getAgreement(header->getTid(), header->getTransmitterAddress());
        if (agreement != nullptr)
            recipientBlockAckAgreementHandler->qosFrameReceived(header, this);
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

void HeHcf::recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(header)) {
        if (!ulCoordinator->isEnabled() || mac->isApInAxMode() ||
                mac->getMib()->bssStationData.associationId <= 0 || triggeredUlOriginalPacket != nullptr) {
            delete packet;
            return;
        }
        auto myAid = mac->getMib()->bssStationData.associationId;
        const Ieee80211HeTriggerUserInfo *selected = nullptr;
        std::vector<const Ieee80211HeTriggerUserInfo *> randomAccessUsers;
        for (unsigned int i = 0; i < trigger->getUsersArraySize(); i++) {
            const auto& user = trigger->getUsers(i);
            if (user.randomAccess)
                randomAccessUsers.push_back(&user);
            else if (user.aid == myAid)
                selected = &user;
        }

        AccessCategory selectedAc = AC_BE;
        queueing::IPacketQueue *sourceQueue = nullptr;
        Packet *sourcePacket = nullptr;
        bool randomAccess = false;
        int bsrpTid = -1;
        if (selected != nullptr && trigger->getTriggerType() != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) {
            selectedAc = mapTidToAccessCategory(selected->tid);
            sourceQueue = edca->getEdcaf(selectedAc)->getPendingQueue();
            for (int i = 0; i < sourceQueue->getNumPackets(); i++) {
                auto candidate = sourceQueue->getPacket(i);
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                        candidate->peekAtFront<Ieee80211MacHeader>());
                if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS &&
                        dataHeader->getTid() == selected->tid) {
                    sourcePacket = candidate;
                    break;
                }
            }
        }
        else if (selected != nullptr && trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) {
            for (int ac = AC_VO; ac >= AC_BK && sourceQueue == nullptr; ac--) {
                auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
                for (int i = 0; i < queue->getNumPackets(); i++) {
                    auto candidate = queue->getPacket(i);
                    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                            candidate->peekAtFront<Ieee80211MacHeader>());
                    if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS) {
                        sourceQueue = queue;
                        selectedAc = static_cast<AccessCategory>(ac);
                        bsrpTid = dataHeader->getTid();
                        break;
                    }
                }
            }
        }
        else if (selected == nullptr && trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
                !randomAccessUsers.empty()) {
            for (int ac = AC_VO; ac >= AC_BK && sourcePacket == nullptr; ac--) {
                auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
                for (int i = 0; i < queue->getNumPackets(); i++) {
                    auto candidate = queue->getPacket(i);
                    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                            candidate->peekAtFront<Ieee80211MacHeader>());
                    if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS) {
                        sourcePacket = candidate;
                        sourceQueue = queue;
                        selectedAc = static_cast<AccessCategory>(ac);
                        break;
                    }
                }
            }
            int raIndex = sourcePacket == nullptr ? -1 :
                    ulCoordinator->selectRandomAccessRu(randomAccessUsers.size());
            if (raIndex >= 0) {
                selected = randomAccessUsers[raIndex];
                randomAccess = true;
            }
            else
                sourcePacket = nullptr;
        }
        if (selected == nullptr) {
            delete packet;
            return;
        }

        uint8_t selectedTid = bsrpTid >= 0 ? bsrpTid : selected->tid;
        if (sourcePacket != nullptr) {
            auto sourceHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    sourcePacket->peekAtFront<Ieee80211MacHeader>());
            selectedTid = sourceHeader->getTid();
        }
        if (sourceQueue == nullptr)
            sourceQueue = edca->getEdcaf(selectedAc)->getPendingQueue();
        int64_t queueBytes = 0;
        for (int i = 0; i < sourceQueue->getNumPackets(); i++) {
            auto queuedPacket = sourceQueue->getPacket(i);
            auto queuedHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    queuedPacket->peekAtFront<Ieee80211MacHeader>());
            if (queuedHeader != nullptr && queuedHeader->getTid() == selectedTid)
                queueBytes += queuedPacket->getByteLength();
        }
        if (sourcePacket != nullptr)
            queueBytes = std::max<int64_t>(0, queueBytes - sourcePacket->getByteLength());

        Packet *responsePacket = nullptr;
        if (sourcePacket != nullptr) {
            auto writableHeader = sourcePacket->removeAtFront<Ieee80211DataHeader>();
            if (!writableHeader->getRetry()) {
                auto qosDataService = check_and_cast<OriginatorQosMacDataService *>(originatorDataService);
                qosDataService->assignSequenceNumber(writableHeader);
            }
            if (!writableHeader->getBufferStatusPresent())
                writableHeader->setChunkLength(writableHeader->getChunkLength() + B(4));
            writableHeader->setOrder(true);
            writableHeader->setAckPolicy(BLOCK_ACK);
            writableHeader->setBufferStatusPresent(true);
            writableHeader->setBufferStatusTid(selectedTid);
            writableHeader->setBufferStatusAc(selectedAc);
            writableHeader->setBufferStatusQueueSize(queueBytes);
            sourcePacket->insertAtFront(writableHeader);
            responsePacket = sourcePacket->dup();
            triggeredUlOriginalPacket = sourcePacket;
            triggeredUlSourceQueue = sourceQueue;
        }
        else {
            auto nullHeader = makeShared<Ieee80211DataHeader>();
            nullHeader->setType(ST_QOS_NULL);
            nullHeader->setReceiverAddress(mac->getMib()->bssData.bssid);
            nullHeader->setTransmitterAddress(mac->getAddress());
            nullHeader->setAddress3(mac->getMib()->bssData.bssid);
            nullHeader->setToDS(true);
            nullHeader->setTid(selectedTid);
            nullHeader->setAckPolicy(BLOCK_ACK);
            nullHeader->setOrder(true);
            nullHeader->setBufferStatusPresent(true);
            nullHeader->setBufferStatusTid(selectedTid);
            nullHeader->setBufferStatusAc(selectedAc);
            nullHeader->setBufferStatusQueueSize(queueBytes);
            nullHeader->setChunkLength(B(30));
            responsePacket = new Packet("HE-TB-QoS-Null", nullHeader);
            responsePacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
        }

        auto radio = check_and_cast<physicallayer::IRadio *>(getContainingNicModule(this)->getSubmodule("radio"));
        auto transmitter = check_and_cast<const physicallayer::FlatTransmitterBase *>(radio->getTransmitter());
        W transmitPower = transmitter->getMaxPower();
        if (auto link = mac->getMib()->findStationLink(mac->getMib()->bssData.bssid)) {
            if (link->valid) {
                W requestedPower = mW(math::dBmW2mW(selected->targetRssiDbm + link->pathLossDb));
                transmitPower = std::min(requestedPower, transmitter->getMaxPower());
            }
        }
        auto request = responsePacket->addTagIfAbsent<physicallayer::Ieee80211HeMuReq>();
        request->setPpduFormat(physicallayer::HE_TRIGGER_BASED_UPLINK);
        request->setTriggerId(trigger->getTriggerId());
        request->setRuIndex(selected->ruIndex);
        request->setRuToneSize(selected->ruToneSize);
        request->setRuToneOffset(selected->ruToneOffset);
        request->setStaId(myAid);
        request->setMcs(selected->mcs);
        request->setCommonDuration(trigger->getCommonDuration());
        request->setTransmitPower(transmitPower);
        triggeredUlWasRandomAccess = randomAccess;
        tx->transmitFrame(responsePacket, responsePacket->peekAtFront<Ieee80211MacHeader>(),
                modeSet->getSifsTime(), this);
        delete responsePacket;
        delete packet;
        return;
    }
    if (auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(header)) {
        auto myAid = mac->getMib()->bssStationData.associationId;
        bool success = false;
        for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); i++) {
            const auto& record = multiStaBlockAck->getRecords(i);
            if (record.aid == myAid) {
                success = record.responseReceived && (record.bitmap & 1);
                break;
            }
        }
        if (triggeredUlOriginalPacket != nullptr) {
            if (success) {
                triggeredUlSourceQueue->removePacket(triggeredUlOriginalPacket);
                take(triggeredUlOriginalPacket);
                delete triggeredUlOriginalPacket;
            }
            else {
                auto writableHeader = triggeredUlOriginalPacket->removeAtFront<Ieee80211DataHeader>();
                writableHeader->setRetry(true);
                triggeredUlOriginalPacket->insertAtFront(writableHeader);
            }
        }
        if (triggeredUlWasRandomAccess)
            ulCoordinator->reportRandomAccessResult(success);
        triggeredUlOriginalPacket = nullptr;
        triggeredUlSourceQueue = nullptr;
        triggeredUlWasRandomAccess = false;
        delete packet;
        return;
    }
    Hcf::recipientProcessReceivedFrame(packet, header);
}

void HeHcf::transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (auto request = packet->findTag<physicallayer::Ieee80211HeMuReq>())
        if (request->getPpduFormat() == physicallayer::HE_TRIGGER_BASED_UPLINK)
            return;
    Hcf::transmissionComplete(packet, header);
}

void HeHcf::originatorProcessTransmittedFrame(Packet *packet)
{
    Enter_Method("originatorProcessTransmittedFrame");
    if (dynamic_cast<const HeUlMuTxOpFs *>(frameSequenceHandler->getFrameSequence()) != nullptr) {
        auto edcaf = edca->getChannelOwner();
        if (edcaf != nullptr)
            edcaf->emit(packetSentToPeerSignal, packet);
        return;
    }
    auto heMuTxop = dynamic_cast<const HeDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence());
    if (heMuTxop != nullptr && heMuTxop->isContainerPacket(packet)) {
        auto edcaf = edca->getChannelOwner();
        if (edcaf) {
            AccessCategory ac = edcaf->getAccessCategory();
            for (const auto& alloc : heMuTxop->getActiveAllocations()) {
                for (auto staPacket : alloc.packets) {
                    auto header = staPacket->peekAtFront<Ieee80211MacHeader>();
                    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
                        originatorProcessTransmittedDataFrame(staPacket, dataHeader, ac);
                        edcaf->getAckHandler()->transitionToWaitingForBlockAck(dataHeader);
                    }
                    else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(header)) {
                        originatorProcessTransmittedManagementFrame(mgmtHeader, ac);
                    }
                }
            }
        }
    }
    else {
        Hcf::originatorProcessTransmittedFrame(packet);
    }
}

void HeHcf::originatorProcessFailedFrame(Packet *failedPacket)
{
    Enter_Method("originatorProcessFailedFrame");
    EV_WARN << "HeHcf: originatorProcessFailedFrame for packet " << failedPacket->getName()
            << " type = " << (failedPacket->peekAtFront<Ieee80211MacHeader>() != nullptr ? (int)failedPacket->peekAtFront<Ieee80211MacHeader>()->getType() : -1) << endl;
    if (dynamic_cast<const HeDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence()) != nullptr) {
        auto failedHeader = failedPacket->peekAtFront<Ieee80211MacHeader>();
        auto edcaf = edca->getChannelOwner();
        if (edcaf) {
            bool retryLimitReached = false;
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader)) {
                edcaf->getRecoveryProcedure()->dataFrameTransmissionFailed(failedPacket, dataHeader);
                retryLimitReached = edcaf->getRecoveryProcedure()->isRetryLimitReached(failedPacket, dataHeader);
                if (dataAndMgmtRateControl) {
                    int retryCount = edcaf->getRecoveryProcedure()->getRetryCount(failedPacket, dataHeader);
                    dataAndMgmtRateControl->frameTransmitted(failedPacket, retryCount, false, retryLimitReached);
                }
                edcaf->getAckHandler()->processFailedFrame(dataHeader);
            }
            else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(failedHeader)) {
                edca->getMgmtAndNonQoSRecoveryProcedure()->dataOrMgmtFrameTransmissionFailed(failedPacket, mgmtHeader, edcaf->getStationRetryCounters());
                retryLimitReached = edca->getMgmtAndNonQoSRecoveryProcedure()->isRetryLimitReached(failedPacket, mgmtHeader);
                if (dataAndMgmtRateControl) {
                    int retryCount = edca->getMgmtAndNonQoSRecoveryProcedure()->getRetryCount(failedPacket, mgmtHeader);
                    dataAndMgmtRateControl->frameTransmitted(failedPacket, retryCount, false, retryLimitReached);
                }
                edcaf->getAckHandler()->processFailedFrame(mgmtHeader);
            }
            else if (auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(failedHeader)) {
                edcaf->getAckHandler()->processFailedBlockAckReq(blockAckReq);
                return;
            }

            if (retryLimitReached) {
                if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader))
                    edcaf->getRecoveryProcedure()->retryLimitReached(failedPacket, dataHeader);
                else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(failedHeader))
                    edca->getMgmtAndNonQoSRecoveryProcedure()->retryLimitReached(failedPacket, mgmtHeader);
                edcaf->getInProgressFrames()->dropFrame(failedPacket);
                edcaf->getAckHandler()->dropFrame(dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(failedHeader));
            }
            else {
                EV_INFO << "Retrying frame in MU-OFDMA: " << failedPacket->getName() << ", re-queuing.\n";
                auto h = failedPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
                h->setRetry(true);
                failedPacket->insertAtFront(h);
                
                // Remove from inProgressFrames
                edcaf->getInProgressFrames()->removeInProgressFrame(failedPacket);
                
                // Re-enqueue into the destination STA's queue bank when available.
                auto pendingQueue = resolvePerStaQueue(failedHeader->getReceiverAddress(), edcaf->getAccessCategory());
                pendingQueue->pushPacket(failedPacket, nullptr);
            }
        }
    }
    else {
        Hcf::originatorProcessFailedFrame(failedPacket);
    }
}

} // namespace ieee80211
} // namespace inet
