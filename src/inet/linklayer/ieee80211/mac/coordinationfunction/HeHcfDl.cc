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
        if (ackHandler->isEligibleToTransmit(header))
            return true;
    }
    return false;
}

IIeee80211HeDlScheduler::ScheduleContext HeHcf::collectScheduleContext(AccessCategory ac) const
{
    IIeee80211HeDlScheduler::ScheduleContext context;
    auto nic = getContainingNicModule(this);
    ASSERT(nic != nullptr);
    auto radio = check_and_cast<physicallayer::IRadio *>(nic->getSubmodule("radio"));
    ASSERT(radio != nullptr);
    auto transmitter = check_and_cast<const physicallayer::Ieee80211Transmitter *>(radio->getTransmitter());
    ASSERT(transmitter != nullptr);
    auto receiver = check_and_cast<const physicallayer::FlatReceiverBase *>(radio->getReceiver());
    ASSERT(receiver != nullptr);
    auto channel = transmitter->getChannel();
    auto activeMode = transmitter->getMode();
    if (channel == nullptr || activeMode == nullptr)
        throw cRuntimeError("HE DL scheduling requires an active IEEE 802.11 channel and mode");
    context.channelNumber = channel->getChannelNumber();
    context.channelCenterFrequency = channel->getCenterFrequency();
    context.channelBandwidth = activeMode->getDataMode()->getBandwidth();
    EV_INFO << "HE DL schedule context: AC " << ac
             << ", channel " << context.channelNumber
             << ", centerFreq " << context.channelCenterFrequency
             << ", bandwidth " << context.channelBandwidth << "\n";
    context.totalTransmitPower = transmitter->getPower();
    context.receiverSensitivity = receiver->getSensitivity();
    context.noiseFigureDb = par("receiverNoiseFigure");
    context.maxAmpduMpduCount = par("maxAmpduMpduCount");
    context.packetExtensionDurationUs = mac->getMib()->heOperation.defaultPeDurationUs;
    context.puncturedSubchannels = resolveHePreamblePuncturing(this, context.channelBandwidth);
    for (size_t i = 0; i < context.puncturedSubchannels.size(); ++i)
        if (context.puncturedSubchannels[i])
            context.puncturedSubchannelMask |= 1U << i;
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
    ASSERT(mib != nullptr);
    std::vector<MacAddress> seenDestinations;
    auto baHandler = getOriginatorBlockAckAgreementHandler();
    ASSERT(baHandler != nullptr);
    auto ackHandler = edcaf->getAckHandler();
    ASSERT(ackHandler != nullptr);
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
            if (dest.isMulticast() || dest.isBroadcast()) {
                EV_INFO << "HE DL schedule context: skipping " << dest << " — broadcast/multicast\n";
                continue;
            }
            if (std::find(seenDestinations.begin(), seenDestinations.end(), dest) != seenDestinations.end())
                continue;
            if (isTwtSleeping(mac, dest)) {
                EV_DEBUG << "HE DL schedule context: skipping sleeping TWT STA " << dest << "\n";
                continue;
            }

            auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
            // Fresh queue entries have not been assigned a sequence number yet.
            // They are eligible to start a transmission without a BlockAck
            // scoreboard lookup; retried/in-progress entries retain a sequence
            // number and must remain inside the 10.25.6 BlockAck window.
            if (!isMuEligibleDataHeader(dataHeader, baHandler)) {
                // See isMuEligibleDataHeader() above: this is an implementation
                // precondition, not a normative requirement of Clause 26.5.
                EV_INFO << "HE DL schedule context: skipping packet " << i << " to " << dest
                         << " — not a QoS data frame or no active originator Block Ack agreement\n";
                continue;
            }
            if (dataHeader->getSequenceNumber().isValid() && !ackHandler->isEligibleToTransmit(dataHeader)) {
                EV_INFO << "HE DL schedule context: skipping packet " << i << " to " << dest
                         << " TID " << (int)dataHeader->getTid()
                         << " — sequence number not eligible for retransmission\n";
                continue;
            }
            auto negotiated = mib->findNegotiatedHeCapabilities(dest);
            if (negotiated != nullptr &&
                    (!negotiated->valid ||
                     !negotiated->intersection.dlOfdma ||
                     negotiated->intersection.supportedChannelWidths.count(context.channelBandwidth) == 0 ||
                     (par("dlMuAckMethod").stdstringValue() != "sequentialBar" &&
                      (!negotiated->intersection.muBarTriggerRx ||
                       !negotiated->intersection.heTbBlockAckTx)))) {
                const char *reason = !negotiated->valid ? "invalid negotiated capabilities" :
                        !negotiated->intersection.dlOfdma ? "DL OFDMA not supported" :
                        negotiated->intersection.supportedChannelWidths.count(context.channelBandwidth) == 0 ?
                                "channel bandwidth not supported" :
                        par("dlMuAckMethod").stdstringValue() != "sequentialBar" ?
                                "MU-BAR/HE-TB BlockAck not supported" : "unknown";
                EV_INFO << "HE DL schedule context: skipping packet " << i << " to " << dest
                         << " — negotiated HE capability mismatch: " << reason << "\n";
                continue;
            }
            if (!context.puncturedSubchannels.empty() && (negotiated == nullptr ||
                    !negotiated->intersection.preamblePuncturing)) {
                EV_INFO << "HE DL schedule context: skipping packet " << i << " to " << dest
                         << " — preamble puncturing not supported\n";
                continue;
            }
            auto agreement = baHandler->getAgreement(dest, dataHeader->getTid());
            int occupiedSlots = ackHandler->getOccupiedBlockAckSequenceNumbers(
                    dest, dataHeader->getTid()).size();
            int availableSlots = agreement == nullptr ? 0 :
                    std::max(0, agreement->getBufferSize() - occupiedSlots);
            // 10.25.6 bounds the outstanding MPDUs by the negotiated BlockAck
            // buffer/window.  The DL MU scheduler cannot select a user whose
            // next MPDU would fall outside that window.
            if (availableSlots == 0) {
                EV_INFO << "HE DL schedule context: skipping packet " << i << " to " << dest
                         << " TID " << (int)dataHeader->getTid()
                         << " — Block Ack window full (" << occupiedSlots << "/"
                         << (agreement == nullptr ? 0 : agreement->getBufferSize()) << ")\n";
                continue;
            }
            seenDestinations.push_back(dest);

            IIeee80211HeDlScheduler::CandidateInfo candidate;
            candidate.staAddress = dest;
            candidate.accessCategory = ac;
            // DL MU payloads are carried as A-MPDU subframes. Include the
            // delimiter in the HoL size so scheduler airtime estimates match
            // the packing planner's single-MPDU PSDU length.
            candidate.holPacketBytes = B(4).get<B>() + pkt->getByteLength();
            auto enqueueTimeTag = pkt->findTag<OrigEnqueueTimeTag>();
            candidate.holEnqueueTime = enqueueTimeTag == nullptr ? pkt->getArrivalTime() : enqueueTimeTag->getEnqueueTime();
            candidate.holDelay = simTime() - candidate.holEnqueueTime;
            candidate.sourceQueue = queue;
            candidate.negotiatedHeCapabilities = negotiated;
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
                            (!queuedHeader->getSequenceNumber().isValid() ||
                             ackHandler->isEligibleToTransmit(queuedHeader)) &&
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
    EV_INFO << "HE DL schedule context: collected " << context.candidates.size()
            << " DL MU candidates for AC " << ac
            << (context.candidates.empty() ? "" : ", anchor = " + context.anchorSta.str()) << "\n";
    context.coding = mac->getMib()->localHeCapabilities.ldpc &&
            std::all_of(context.candidates.begin(), context.candidates.end(), [] (const auto& candidate) {
                return candidate.negotiatedHeCapabilities != nullptr &&
                        candidate.negotiatedHeCapabilities->valid && candidate.negotiatedHeCapabilities->intersection.ldpc;
            }) ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
    context.csiManager = &csiManager;
    context.numApAntennas = radio->getAntenna()->getNumAntennas();
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
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (isTwtSleeping(mac, header->getReceiverAddress()))
            continue;
        auto enqueueTimeTag = packet->findTag<OrigEnqueueTimeTag>();
        auto enqueueTime = enqueueTimeTag == nullptr ? packet->getArrivalTime() : enqueueTimeTag->getEnqueueTime();
        if (oldestQueue == nullptr || enqueueTime < oldestEnqueueTime) {
            oldestQueue = queue;
            oldestEnqueueTime = enqueueTime;
        }
    }
    return oldestQueue;
}

bool HeHcf::stagePerStaFrameForBlockAckBootstrap(AccessCategory ac)
{
    if (queueBankManager == nullptr || originatorBlockAckAgreementHandler == nullptr ||
            originatorBlockAckAgreementPolicy == nullptr)
        return false;

    queueing::IPacketQueue *bootstrapQueue = nullptr;
    simtime_t oldestEnqueueTime = SIMTIME_MAX;
    for (const auto& entry : queueBankManager->getQueueBanks()) {
        auto queue = entry.second->getQueue((StationQueueBank::AccessCategory)ac);
        if (queue->isEmpty())
            continue;
        auto packet = queue->getPacket(0);
        auto header = dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront<Ieee80211MacHeader>());
        if (header == nullptr || header->getType() != ST_DATA_WITH_QOS ||
                header->getReceiverAddress().isMulticast() || header->getReceiverAddress().isBroadcast() ||
                isTwtSleeping(mac, header->getReceiverAddress()) ||
                !originatorBlockAckAgreementPolicy->isAddbaReqNeeded(packet, header))
            continue;

        auto agreement = originatorBlockAckAgreementHandler->getAgreement(
                header->getReceiverAddress(), header->getTid());
        if (agreement != nullptr && agreement->getIsAddbaResponseReceived())
            continue;
        if (agreement != nullptr && agreement->getIsAddbaRequestInProgress() &&
                (agreement->getExpirationTime() < SIMTIME_ZERO || simTime() < agreement->getExpirationTime()))
            continue;

        auto enqueueTimeTag = packet->findTag<OrigEnqueueTimeTag>();
        auto enqueueTime = enqueueTimeTag == nullptr ? packet->getArrivalTime() : enqueueTimeTag->getEnqueueTime();
        if (bootstrapQueue == nullptr || enqueueTime < oldestEnqueueTime) {
            bootstrapQueue = queue;
            oldestEnqueueTime = enqueueTime;
        }
    }

    if (bootstrapQueue == nullptr)
        return false;

    auto packet = bootstrapQueue->dequeuePacket();
    auto header = packet->peekAtFront<Ieee80211DataHeader>();
    edca->getEdcaf(ac)->getPendingQueue()->enqueuePacket(packet);
    EV_INFO << "Staging single-user Block Ack bootstrap frame for "
            << header->getReceiverAddress() << " tid=" << (int)header->getTid() << "\n";
    return true;
}

bool HeHcf::stagePerStaFrameForSingleUserTransmission(AccessCategory ac)
{
    auto sourceQueue = findOldestPerStaQueue(ac);
    if (sourceQueue == nullptr)
        return false;
    edca->getEdcaf(ac)->getPendingQueue()->enqueuePacket(sourceQueue->dequeuePacket());
    return true;
}

bool HeHcf::tryStartDlMuFrameSequence(AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    if (mac->isApInAxMode() && enableDlMuMimo && mac->getMib()->localHeCapabilities.dlMuMimoBeamformer) {
        // 26.5.1 allows DL MU-MIMO only when the AP has the required HE
        // beamformer capability and per-STA feedback; 26.7.3 defines the HE
        // TB sounding exchange used here to refresh CSI before scheduling.
        auto scheduleContext = collectScheduleContext(ac);
        auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"));
        if (soundingCoordinator->tryStartSoundingSequence(ac, scheduleContext, frameSequenceHandler, mac, modeSet, csiManager, buildContext(ac), this))
            return true;
    }

    auto pendingQueue = edcaf->getPendingQueue();
    auto inProgress = edcaf->getInProgressFrames();
    if (hasEligibleExistingFrame(inProgress, edcaf->getAckHandler())) {
        // 10.23.2.8 allows multiple frame exchange sequences in an EDCA TXOP,
        // but already outstanding frames keep their legacy recovery context.
        // Complete those before starting a new HE MU PPDU.
        EV_INFO << "Completing " << inProgress->getLength()
                << " recovery/outstanding frames before opening a new MU transmission." << endl;
        Hcf::startFrameSequence(ac);
        return true;
    }
    auto headDataHeader = getEligibleHoLDataHeader(pendingQueue);
    auto baHandler = getOriginatorBlockAckAgreementHandler();
    if (!pendingQueue->isEmpty() && !isMuEligibleDataHeader(headDataHeader, baHandler)) {
        if (headDataHeader != nullptr) {
            EV_INFO << "Earliest SU-transmittable packet "
                    << headDataHeader->getReceiverAddress() << " tid=" << headDataHeader->getTid()
                    << " is MU-ineligible, falling back to Hcf::startFrameSequence(ac)." << endl;
        }
        Hcf::startFrameSequence(ac);
        return true;
    }
    if (pendingQueue->isEmpty() && stagePerStaFrameForBlockAckBootstrap(ac)) {
        Hcf::startFrameSequence(ac);
        return true;
    }
    auto scheduleContext = collectScheduleContext(ac);
    if (scheduleContext.candidates.size() >= 2) {
        auto previewAllocations = dlScheduler->schedule(scheduleContext);
        if (previewAllocations.size() < 2) {
            EV_INFO << "HE DL scheduler preview retained fewer than two MU users; falling back to SU." << endl;
            if (pendingQueue->isEmpty())
                stagePerStaFrameForSingleUserTransmission(ac);
            Hcf::startFrameSequence(ac);
            return true;
        }
        EV_INFO << "HE DL MU opportunity detected for " << scheduleContext.candidates.size()
                << " STAs - starting HE DL MU TxOp FS." << endl;
        auto ackMethod = par("dlMuAckMethod").stdstringValue() == "sequentialBar" ?
                HeDlMuTxOpFs::AckMethod::EXPLICIT_SEQUENTIAL_BAR :
                HeDlMuTxOpFs::AckMethod::MU_BAR_TRIGGER;
        // 26.4.4.3 permits SU BlockAck responses to an HE MU PPDU; 9.3.1.22.4
        // plus 26.4.4.4/26.5.2 model the MU-BAR-triggered HE TB BlockAck path.
        EV_INFO << "Start HE DL MU TxOp FS: using "
                 << (ackMethod == HeDlMuTxOpFs::AckMethod::MU_BAR_TRIGGER ? "MU-BAR trigger" : "sequential BAR")
                 << " acknowledgment method\n";
        frameSequenceHandler->startFrameSequence(
                new HeDlMuTxOpFs(dlScheduler, scheduleContext, modeSet,
                                 pendingQueue, edcaf->getAckHandler(), this,
                                 par("maxAmpduMpduCount"),
                                 par("maxHeMuPsduLength"),
                                 par("maxHeMuPpduDuration"),
                                 ackMethod),
                buildContext(ac), this);
        emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
        return true;
    }
    if (pendingQueue->isEmpty())
        stagePerStaFrameForSingleUserTransmission(ac);
    else
        EV_INFO << "Only " << scheduleContext.candidates.size()
                 << " MU candidate(s), falling back to single-user\n";
    return false;
}

void HeHcf::handleDlMuPlanningFailure(AccessCategory ac)
{
    bool staged = stagePerStaFrameForSingleUserTransmission(ac);
    forceNextSingleUser[ac] = staged;
    EV_WARN << "DL MU planning failed for AC " << ac
            << ", forcing next TXOP single-user (staged = " << (staged ? "true" : "false") << ")\n";
}
} // namespace ieee80211
} // namespace inet
