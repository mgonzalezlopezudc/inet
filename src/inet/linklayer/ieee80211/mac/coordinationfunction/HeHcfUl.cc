//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

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
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF uplink MU support.

namespace inet {
namespace ieee80211 {

bool HeHcf::allAssociatedStationsSupportPreamblePuncturing() const
{
    return std::all_of(mac->getMib()->bssAccessPointData.stations.begin(),
            mac->getMib()->bssAccessPointData.stations.end(), [&] (const auto& station) {
                auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(station.first);
                return station.second != Ieee80211Mib::ASSOCIATED ||
                        (capabilities != nullptr && capabilities->valid &&
                         capabilities->intersection.preamblePuncturing);
            });
}

bool HeHcf::supportsPreamblePuncturing(const IIeee80211HeUlScheduler::RuAllocation& allocation) const
{
    if (allocation.randomAccess)
        return allAssociatedStationsSupportPreamblePuncturing();
    auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(allocation.staAddress);
    return capabilities != nullptr && capabilities->valid && capabilities->intersection.preamblePuncturing;
}

bool HeHcf::tryStartUlMuFrameSequence(AccessCategory ac)
{
    if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NO_TRIGGER ||
            !mac->isApInAxMode() || !ulCoordinator->isEnabled())
        return false;

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
        // IEEE 802.11-2024 Clause 26.5.2: a BSRP Trigger solicits Buffer Status
        // Reports from one or more STAs.  The standard does not mandate a
        // particular RU allocation; here we simply give every associated STA
        // its own RU and advertise the rest as random-access RUs.  This is an
        // approximation of the full scheduler freedom allowed by the standard.
        auto maxRus = physicallayer::getHeMaxRuCount(channelBandwidth);
        auto layout = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth, maxRus);
        int index = 0;
        auto ulScheduler = getSubmodule("ulScheduler");
        int maxMuStations = ulScheduler ? ulScheduler->par("maxMuStations").intValue() : maxRus;
        for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
            if (station.second != Ieee80211Mib::ASSOCIATED || index >= maxRus)
                continue;
            if (index >= maxMuStations)
                break;
            if (isTwtSleeping(mac, station.first)) {
                EV_DEBUG << "HE UL BSRP: skipping sleeping TWT STA " << station.first << "\n";
                continue;
            }
            IIeee80211HeUlScheduler::RuAllocation allocation;
            allocation.staAddress = station.first;
            allocation.associationId = mac->getMib()->getAssociationId(station.first);
            allocation.ru = layout[index++];
            allocation.targetRssiDbm = (int)std::round(sensitivityDbm + (double)par("ulTargetRssiMargin"));
            ulSchedule.allocations.push_back(allocation);
        }
        while (index < maxRus) {
            IIeee80211HeUlScheduler::RuAllocation allocation;
            allocation.randomAccess = true;
            allocation.associationId = 0;
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
    ulSchedule.allocations.erase(std::remove_if(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
            [this] (const auto& allocation) {
                return !allocation.randomAccess && isTwtSleeping(mac, allocation.staAddress);
            }), ulSchedule.allocations.end());
    auto puncturedSubchannels = parseHePreamblePuncturing(par("hePreamblePuncturing").stringValue(), channelBandwidth);
    if (!puncturedSubchannels.empty()) {
        for (size_t i = 0; i < puncturedSubchannels.size(); ++i)
            if (puncturedSubchannels[i])
                ulSchedule.puncturedSubchannelMask |= 1U << i;
        ulSchedule.allocations.erase(std::remove_if(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
                [&] (const auto& allocation) {
                    return overlapsHePuncturedSubchannel(allocation.ru, puncturedSubchannels, channelBandwidth) ||
                            !supportsPreamblePuncturing(allocation);
                }), ulSchedule.allocations.end());
    }
    ulSchedule.packetExtensionDurationUs = mac->getMib()->heOperation.defaultPeDurationUs;
    if (auto heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(modeSet->getMode(0))) {
        switch (heMode->getDataMode()->getGuardIntervalType()) {
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT:
                ulSchedule.guardInterval = physicallayer::HE_GI_0_8_US;
                break;
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM:
                ulSchedule.guardInterval = physicallayer::HE_GI_1_6_US;
                break;
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG:
                ulSchedule.guardInterval = physicallayer::HE_GI_3_2_US;
                break;
        }
    }
    bool ldpcSupportedByAll = mac->getMib()->localHeCapabilities.ldpc;
    for (const auto& allocation : ulSchedule.allocations) {
        if (allocation.randomAccess)
            continue;
        auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(allocation.staAddress);
        ldpcSupportedByAll = ldpcSupportedByAll && capabilities != nullptr && capabilities->valid &&
                capabilities->intersection.ldpc;
    }
    ulSchedule.coding = ldpcSupportedByAll ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
    auto triggerType = pendingUlTrigger;
    pendingUlTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    if (ulSchedule.allocations.empty()) {
        EV_WARN << "HE UL skipping Trigger because no usable RU allocations remain"
                << " after scheduling and puncturing checks\n";
        return false;
    }

    ASSERT(ulSchedule.commonDuration > SIMTIME_ZERO);
    EV_INFO << "HE UL starting"
             << (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? "BSRP" : "Basic")
             << " exchange with " << ulSchedule.allocations.size()
             << " RU allocations for " << ulSchedule.commonDuration << "\n";
    frameSequenceHandler->startFrameSequence(
            new HeUlMuTxOpFs(ulCoordinator, this, ulSchedule, triggerType,
                    modeSet, mac->getAddress()),
            buildContext(ac), this);
    emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
    return true;
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
void HeHcf::sendTriggeredBlockAckResponse(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger)
{
    auto myAid = mac->getMib()->bssStationData.associationId;
    const Ieee80211HeTriggerUserInfo *selected = nullptr;
    for (unsigned int i = 0; i < trigger->getUsersArraySize(); ++i)
        if (trigger->getUsers(i).aid == myAid) {
            selected = &trigger->getUsers(i);
            break;
        }
    auto agreement = selected == nullptr || recipientBlockAckAgreementHandler == nullptr ?
            nullptr : recipientBlockAckAgreementHandler->getAgreement(
                    selected->tid, trigger->getTransmitterAddress());
    if (agreement != nullptr) {
        auto blockAck = makeShared<Ieee80211BasicBlockAck>();
        auto startingSequenceNumber = agreement->getStartingSequenceNumber();
        for (int i = 0; i < 64; ++i) {
            auto& bitmap = blockAck->getBlockAckBitmapForUpdate(i);
            for (FragmentNumber fragment = 0; fragment < 16; ++fragment)
                bitmap.setBit(fragment, agreement->getBlockAckRecord()->getAckState(
                        startingSequenceNumber + i, fragment));
        }
        blockAck->setReceiverAddress(trigger->getTransmitterAddress());
        blockAck->setTransmitterAddress(mac->getAddress());
        blockAck->setCompressedBitmap(false);
        blockAck->setStartingSequenceNumber(startingSequenceNumber);
        blockAck->setTidInfo(selected->tid);
        blockAck->setDurationField(SIMTIME_ZERO);
        auto response = new Packet("HE-TB-BlockAck", blockAck);
        response->insertAtBack(makeShared<Ieee80211MacTrailer>());
        auto request = response->addTagIfAbsent<physicallayer::Ieee80211HeMuReq>();
        request->setPpduFormat(physicallayer::HE_TRIGGER_BASED_UPLINK);
        request->setTriggerId(trigger->getTriggerId());
        request->setRuIndex(selected->ruIndex);
        request->setRuToneSize(selected->ruToneSize);
        request->setRuToneOffset(selected->ruToneOffset);
        request->setStaId(myAid);
        request->setMcs(selected->mcs);
        request->setCommonDuration(trigger->getCommonDuration());
        tx->transmitFrame(response, blockAck, modeSet->getSifsTime(), this);
        delete response;
    }
    delete packet;
    return;
}

void HeHcf::retryPendingTriggeredUlExchanges()
{
    for (auto& entry : triggeredUlExchanges) {
        if (entry.second.randomAccess)
            ulCoordinator->reportRandomAccessResult(false);
        for (auto pkt : entry.second.packets) {
            auto writableHeader = pkt->removeAtFront<Ieee80211DataHeader>();
            writableHeader->setRetry(true);
            pkt->insertAtFront(writableHeader);
            entry.second.sourceQueue->pushPacket(pkt, nullptr);
        }
    }
    triggeredUlExchanges.clear();
}

Packet *HeHcf::buildTriggeredUlResponsePacket(Packet *sourcePacket, queueing::IPacketQueue *sourceQueue,
        AccessCategory selectedAc, uint8_t selectedTid, int64_t queueBytes, int availableSlots,
        const Ieee80211HeTriggerUserInfo *selected, const Ptr<const Ieee80211TriggerFrame>& trigger,
        TriggeredUlExchange& exchange)
{
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

    if (sourcePacket != nullptr) {
        exchange.packets.push_back(sourcePacket);
        exchange.sequenceNumbers.push_back(sourcePacket->peekAtFront<Ieee80211DataHeader>()->getSequenceNumber().get());

        // Basic Trigger UL aggregation is deliberately single-TID.  The
        // retained packets remain in their EDCA queue until the bitmap
        // arrives, so a partial Multi-STA BA can retry just the misses.
        int maximumMpduCount = std::min(64, availableSlots);
        for (int i = 0; availableSlots > 0 && (int)exchange.packets.size() < maximumMpduCount &&
                i < sourceQueue->getNumPackets(); ++i) {
            auto candidate = sourceQueue->getPacket(i);
            if (candidate == sourcePacket)
                continue;
            auto candidateHeader = dynamicPtrCast<const Ieee80211DataHeader>(candidate->peekAtFront<Ieee80211MacHeader>());
            if (candidateHeader == nullptr || candidateHeader->getType() != ST_DATA_WITH_QOS ||
                    candidateHeader->getTid() != selectedTid ||
                    candidateHeader->getReceiverAddress() != mac->getMib()->bssData.bssid)
                continue;
            B psduLength(0);
            for (auto packet : exchange.packets)
                psduLength += B(4 + packet->getByteLength());
            psduLength += B(4 + candidate->getByteLength());
            physicallayer::Ieee80211HeRu ru = exchange.ru;
            ru.dataSubcarriers = physicallayer::getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = physicallayer::getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * 78125.0);
            if (physicallayer::computeHeUserPhyParameters(psduLength, ru, selected->mcs).duration > trigger->getCommonDuration())
                break;
            auto writableCandidateHeader = candidate->removeAtFront<Ieee80211DataHeader>();
            if (!writableCandidateHeader->getRetry()) {
                auto qosDataService = check_and_cast<OriginatorQosMacDataService *>(originatorDataService);
                qosDataService->assignSequenceNumber(writableCandidateHeader);
            }
            writableCandidateHeader->setOrder(true);
            writableCandidateHeader->setAckPolicy(BLOCK_ACK);
            candidate->insertAtFront(writableCandidateHeader);
            exchange.packets.push_back(candidate);
            exchange.sequenceNumbers.push_back(writableCandidateHeader->getSequenceNumber().get());
        }
        int64_t reportedQueueBytes = queueBytes;
        for (auto pkt : exchange.packets)
            reportedQueueBytes = std::max<int64_t>(0, reportedQueueBytes - pkt->getByteLength());
        auto firstHeader = exchange.packets.front()->removeAtFront<Ieee80211DataHeader>();
        firstHeader->setBufferStatusQueueSize(reportedQueueBytes);
        exchange.packets.front()->insertAtFront(firstHeader);
        if (exchange.packets.size() > 1) {
            delete responsePacket;
            responsePacket = new Packet("HE-TB-A-MPDU");
            for (size_t i = 0; i < exchange.packets.size(); ++i) {
                auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
                delimiter->setLength(exchange.packets[i]->getByteLength());
                delimiter->setEof(i + 1 == exchange.packets.size());
                responsePacket->insertAtBack(delimiter);
                responsePacket->insertAtBack(exchange.packets[i]->peekAll());
                int padding = (4 - (B(4) + B(exchange.packets[i]->getByteLength())).get<B>() % 4) % 4;
                if (i + 1 != exchange.packets.size() && padding != 0)
                    responsePacket->insertAtBack(makeShared<ByteCountChunk>(B(padding)));
            }
        }
        else {
            delete responsePacket;
            responsePacket = exchange.packets.front()->dup();
        }
        for (auto pkt : exchange.packets) {
            exchange.sourceQueue->removePacket(pkt);
            take(pkt);
        }
    }
    return responsePacket;
}

void HeHcf::processReceivedTriggerFrame(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger)
{
    // IEEE 802.11-2024 Clause 26.5.2: processing of a received Trigger frame.
    // Trigger type 2 corresponds to a Basic Trigger (solicited HE TB PPDU).
    if (trigger->getTriggerType() == 2) {
        sendTriggeredBlockAckResponse(packet, trigger);
        return;
    }
    if (!ulCoordinator->isEnabled() || mac->isApInAxMode() ||
            mac->getMib()->bssStationData.associationId <= 0) {
        delete packet;
        return;
    }
    retryPendingTriggeredUlExchanges();
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
    else if (selected == nullptr && !randomAccessUsers.empty()) {
        queueing::IPacketQueue *pendingQueue = nullptr;
        Packet *pendingPacket = nullptr;
        AccessCategory pendingAc = AC_BE;
        int pendingTid = -1;
        for (int ac = AC_VO; ac >= AC_BK && pendingPacket == nullptr; ac--) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int i = 0; i < queue->getNumPackets(); i++) {
                auto candidate = queue->getPacket(i);
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                        candidate->peekAtFront<Ieee80211MacHeader>());
                if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS) {
                    pendingPacket = candidate;
                    pendingQueue = queue;
                    pendingAc = static_cast<AccessCategory>(ac);
                    pendingTid = dataHeader->getTid();
                    break;
                }
            }
        }
        if (pendingQueue != nullptr) {
            int raIndex = ulCoordinator->selectRandomAccessRu(randomAccessUsers.size());
            if (raIndex >= 0) {
                selected = randomAccessUsers[raIndex];
                randomAccess = true;
                selectedAc = pendingAc;
                sourceQueue = pendingQueue;
                if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) {
                    bsrpTid = pendingTid;
                    sourcePacket = nullptr;
                }
                else {
                    sourcePacket = pendingPacket;
                }
            }
        }
    }
    if (selected == nullptr) {
        EV_INFO << "Ignoring HE UL Trigger " << trigger->getTriggerId()
                 << ": this STA has no scheduled or selected random-access RU\n";
        delete packet;
        return;
    }

    ASSERT(selected->ruToneSize > 0);
    ASSERT(trigger->getCommonDuration() > SIMTIME_ZERO);

    uint8_t selectedTid = bsrpTid >= 0 ? bsrpTid : selected->tid;
    if (sourcePacket != nullptr) {
        auto sourceHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                sourcePacket->peekAtFront<Ieee80211MacHeader>());
        selectedTid = sourceHeader->getTid();
    }
    if (sourceQueue == nullptr)
        sourceQueue = edca->getEdcaf(selectedAc)->getPendingQueue();
    auto ulBaAgreement = originatorBlockAckAgreementHandler == nullptr ? nullptr :
            originatorBlockAckAgreementHandler->getAgreement(mac->getMib()->bssData.bssid, selectedTid);
    int occupiedSlots = edca->getEdcaf(selectedAc)->getAckHandler()->getOccupiedBlockAckSequenceNumbers(
            mac->getMib()->bssData.bssid, selectedTid).size();
    int availableSlots = ulBaAgreement == nullptr ? 0 :
            std::max(0, ulBaAgreement->getBufferSize() - occupiedSlots);
    if (sourcePacket != nullptr && (ulBaAgreement == nullptr || availableSlots == 0))
        sourcePacket = nullptr;
    int64_t queueBytes = 0;
    for (int i = 0; i < sourceQueue->getNumPackets(); i++) {
        auto queuedPacket = sourceQueue->getPacket(i);
        auto queuedHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                queuedPacket->peekAtFront<Ieee80211MacHeader>());
        if (queuedHeader != nullptr && queuedHeader->getTid() == selectedTid)
            queueBytes += queuedPacket->getByteLength();
    }
    TriggeredUlExchange exchange;
    exchange.tid = selectedTid;
    exchange.sourceQueue = sourceQueue;
    exchange.randomAccess = randomAccess;
    exchange.ru.index = selected->ruIndex;
    exchange.ru.toneSize = selected->ruToneSize;
    exchange.ru.toneOffset = selected->ruToneOffset;
    exchange.expectedResponseTime = simTime() + modeSet->getSifsTime();
    auto responsePacket = buildTriggeredUlResponsePacket(sourcePacket, sourceQueue, selectedAc,
            selectedTid, queueBytes, availableSlots, selected, trigger, exchange);
    if (!exchange.packets.empty())
        triggeredUlExchanges.emplace(trigger->getTriggerId(), std::move(exchange));

    auto radio = check_and_cast<physicallayer::IRadio *>(getContainingNicModule(this)->getSubmodule("radio"));
    auto transmitter = check_and_cast<const physicallayer::FlatTransmitterBase *>(radio->getTransmitter());
    W transmitPower = transmitter->getMaxPower();
    if (auto link = mac->getMib()->findStationLink(mac->getMib()->bssData.bssid)) {
        if (link->valid) {
            W requestedPower = mW(math::dBmW2mW(selected->targetRssiDbm + link->pathLossDb));
            transmitPower = std::min(requestedPower, transmitter->getMaxPower());
        }
    }
    // Tag the response as an HE TB PPDU (IEEE 802.11-2024 Clause 27.3.11.12).
    // The PHY layer uses these parameters to build the HE-SIG-A field and
    // place the PSDU on the assigned RU.
    auto request = responsePacket->addTagIfAbsent<physicallayer::Ieee80211HeMuReq>();
    request->setPpduFormat(physicallayer::HE_TRIGGER_BASED_UPLINK);
    request->setTriggerId(trigger->getTriggerId());
    request->setRuIndex(selected->ruIndex);
    request->setRuToneSize(selected->ruToneSize);
    request->setRuToneOffset(selected->ruToneOffset);
    request->setStaId(myAid);
    request->setMcs(selected->mcs);
    request->setGuardInterval(trigger->getGuardInterval());
    request->setCoding(trigger->getCoding());
    request->setPacketExtensionDurationUs(trigger->getPacketExtensionDurationUs());
    request->setPuncturedSubchannelMask(trigger->getPuncturedSubchannelMask());
    request->setCommonDuration(trigger->getCommonDuration());
    request->setTransmitPower(transmitPower);
    EV_INFO << "Sending HE-TB response: trigger=" << trigger->getTriggerId()
             << ", AID=" << myAid
             << ", " << (randomAccess ? "random-access" : "scheduled")
             << " RU=" << selected->ruIndex
             << ", packets=" << exchange.packets.size() << "\n";
    tx->transmitFrame(responsePacket, responsePacket->peekAtFront<Ieee80211MacHeader>(),
            modeSet->getSifsTime(), this);
    delete responsePacket;
    delete packet;
    return;
}

void HeHcf::processReceivedMultiStaBlockAck(Packet *packet, const Ptr<const Ieee80211MultiStaBlockAck>& multiStaBlockAck)
{
    auto myAid = mac->getMib()->bssStationData.associationId;
    bool success = false;
    for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); i++) {
        const auto& record = multiStaBlockAck->getRecords(i);
        if (record.aid == myAid) {
            success = record.responseReceived && (record.bitmap & 1);
            break;
        }
    }
    for (auto& entry : triggeredUlExchanges) {
        auto& exchange = entry.second;
        const Ieee80211MultiStaBlockAckRecord *record = nullptr;
        for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); ++i)
            if (multiStaBlockAck->getRecords(i).aid == myAid && multiStaBlockAck->getRecords(i).tid == exchange.tid) {
                record = &multiStaBlockAck->getRecords(i);
                break;
            }
        bool exchangeSuccess = false;
        for (size_t i = 0; i < exchange.packets.size(); ++i) {
            bool acknowledged = false;
            if (record != nullptr && record->responseReceived) {
                int offset = (exchange.sequenceNumbers[i] - record->startingSequenceNumber + 4096) % 4096;
                acknowledged = offset < 64 && (record->bitmap & (UINT64_C(1) << offset));
            }
            if (acknowledged) {
                delete exchange.packets[i];
                exchangeSuccess = true;
            }
            else {
                auto writableHeader = exchange.packets[i]->removeAtFront<Ieee80211DataHeader>();
                writableHeader->setRetry(true);
                exchange.packets[i]->insertAtFront(writableHeader);
                exchange.sourceQueue->pushPacket(exchange.packets[i], nullptr);
            }
        }
        if (exchange.randomAccess)
            ulCoordinator->reportRandomAccessResult(exchangeSuccess);
        success = success || exchangeSuccess;
    }
    triggeredUlExchanges.clear();
    delete packet;
    return;
}
} // namespace ieee80211
} // namespace inet
