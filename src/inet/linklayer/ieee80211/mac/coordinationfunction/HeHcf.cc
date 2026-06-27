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
//   - Clause 27.3.11.12: HE TB PPDU format.
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
    // INET-specific precondition: DL MU candidates must be QoS data frames with
    // an active Block Ack agreement so that A-MPDU aggregation and bitmap-based
    // acknowledgment can be used.  IEEE 802.11-2024 does not normatively require
    // a Block Ack agreement for DL MU, but without one this implementation
    // cannot perform the required A-MPDU aggregation (Clause 26.3.3).
    return dataHeader != nullptr &&
           dataHeader->getType() == inet::ieee80211::ST_DATA_WITH_QOS &&
           inet::ieee80211::hasActiveOriginatorBlockAckAgreement(baHandler, dataHeader->getReceiverAddress(), dataHeader->getTid());
}

bool hasEligibleExistingFrame(inet::ieee80211::InProgressFrames *inProgress, inet::ieee80211::IAckHandler *ackHandler)
{
    for (int i = 0; i < inProgress->getLength(); ++i) {
        auto header = inProgress->getFrames(i)->peekAtFront<inet::ieee80211::Ieee80211DataOrMgmtHeader>();
        if (ackHandler->isEligibleToTransmit(header))
            return true;
    }
    return false;
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

namespace {

template <typename T>
Ptr<const T> findPacketChunk(const Packet *packet)
{
    auto data = packet->peekData();
    if (auto chunk = dynamicPtrCast<const T>(data))
        return chunk;
    if (auto sequence = dynamicPtrCast<const SequenceChunk>(data)) {
        for (const auto& chunk : sequence->getChunks())
            if (auto result = dynamicPtrCast<const T>(chunk))
                return result;
    }
    return nullptr;
}

} // namespace

Define_Module(HeHcf);

HeHcf::~HeHcf()
{
    cancelAndDelete(ulTriggerTimer);
    for (auto& entry : triggeredUlExchanges) {
        for (auto pkt : entry.second.packets) {
            delete pkt;
        }
    }
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

        enableDlMuMimo = par("enableDlMuMimo").boolValue();
        csiValidityDuration = par("csiValidityDuration");
        defaultCsiLeakage = par("defaultCsiLeakage");
        csiLeakageOverrides = par("csiLeakageOverrides").stdstringValue();
        csiManager.configure(csiValidityDuration, defaultCsiLeakage, csiLeakageOverrides);

        WATCH(pendingUlTrigger);
        WATCH(ulTriggerAccessRequested);
        WATCH(forceNextSingleUser[0]);
        WATCH(forceNextSingleUser[1]);
        WATCH(forceNextSingleUser[2]);
        WATCH(forceNextSingleUser[3]);
        WATCH_MAP(triggeredUlExchanges);
        WATCH_EXPR("pendingUlTriggerName", getPendingUlTriggerName());
        WATCH_EXPR("stationQueueBanks", getStationQueueBankCount());
        WATCH_EXPR("triggeredUlExchangeCount", triggeredUlExchanges.size());
        WATCH_EXPR("heHcfSummary", getHeHcfSummary());
    }
    else if (stage == INITSTAGE_LINK_LAYER && mac->isApInAxMode()) {
        queueBankManager = std::make_unique<StationQueueBankManager>(getSubmodule("queueBanks"));
        for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
            if (station.second == Ieee80211Mib::ASSOCIATED)
                queueBankManager->createQueueBank(station.first);
        }
        WATCH_MAP(csiManager.csiTable);
        if (ulCoordinator->isEnabled())
            scheduleAfter(par("ulTriggerCheckInterval"), ulTriggerTimer);
    }
}

const char *HeHcf::getPendingUlTriggerName() const
{
    switch (pendingUlTrigger) {
        case IIeee80211HeUlTriggerPolicy::NO_TRIGGER: return "NO_TRIGGER";
        case IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER: return "BASIC_TRIGGER";
        case IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER: return "BSRP_TRIGGER";
        default: return "UNKNOWN";
    }
}

int HeHcf::getStationQueueBankCount() const
{
    return queueBankManager == nullptr ? 0 : queueBankManager->getQueueBanks().size();
}

std::string HeHcf::getHeHcfSummary() const
{
    std::stringstream stream;
    stream << "DL scheduler=" << (dlScheduler == nullptr ? "none" : "configured")
           << ", UL coordinator=" << (ulCoordinator != nullptr && ulCoordinator->isEnabled() ? "enabled" : "disabled")
           << ", pendingTrigger=" << getPendingUlTriggerName()
           << ", queueBanks=" << getStationQueueBankCount()
           << ", triggeredUL=" << triggeredUlExchanges.size()
           << ", dlMuMimo=" << (enableDlMuMimo ? "enabled" : "disabled")
           << ", csiEntries=" << csiManager.csiTable.size();
    return stream.str();
}

void HeHcf::finish()
{
    if (queueBankManager != nullptr) {
        for (const auto& pair : queueBankManager->getQueueBanks()) {
            pair.second->clear();
        }
    }
    cSimpleModule::finish();
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
    EV_INFO << "Requesting channel access for HE UL "
             << (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? "BSRP" : "Basic")
             << " Trigger\n";
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
    context.puncturedSubchannels = parseHePreamblePuncturing(par("hePreamblePuncturing").stringValue(),
            context.channelBandwidth);
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
            // Fresh queue entries have not been assigned a sequence number
            // yet. They are eligible to start a transmission without an ACK
            // status lookup; retried/in-progress entries retain a sequence
            // number and must still be checked by the ACK handler.
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
            candidate.holPacketBytes = pkt->getByteLength();
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
        auto supportsPuncturing = [&] (const IIeee80211HeUlScheduler::RuAllocation& allocation) {
            if (allocation.randomAccess)
                return std::all_of(mac->getMib()->bssAccessPointData.stations.begin(),
                        mac->getMib()->bssAccessPointData.stations.end(), [&] (const auto& station) {
                            auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(station.first);
                            return station.second != Ieee80211Mib::ASSOCIATED ||
                                    (capabilities != nullptr && capabilities->valid &&
                                     capabilities->intersection.preamblePuncturing);
                        });
            auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(allocation.staAddress);
            return capabilities != nullptr && capabilities->valid && capabilities->intersection.preamblePuncturing;
        };
        ulSchedule.allocations.erase(std::remove_if(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
                [&] (const auto& allocation) {
                    return overlapsHePuncturedSubchannel(allocation.ru, puncturedSubchannels, channelBandwidth) ||
                            !supportsPuncturing(allocation);
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

bool HeHcf::tryStartDlMuFrameSequence(AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    if (mac->isApInAxMode() && enableDlMuMimo && mac->getMib()->localHeCapabilities.dlMuMimoBeamformer) {
        auto scheduleContext = collectScheduleContext(ac);
        auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"));
        if (soundingCoordinator->tryStartSoundingSequence(ac, scheduleContext, frameSequenceHandler, mac, modeSet, csiManager, buildContext(ac), this))
            return true;
    }

    auto pendingQueue = edcaf->getPendingQueue();
    auto inProgress = edcaf->getInProgressFrames();
    if (hasEligibleExistingFrame(inProgress, edcaf->getAckHandler())) {
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

bool HeHcf::releaseChannelIfNoFallbackFrame(AccessCategory ac)
{
    auto fallbackEdcaf = edca->getEdcaf(ac);
    if (fallbackEdcaf->getPendingQueue()->isEmpty() &&
            fallbackEdcaf->getInProgressFrames()->getLength() == 0)
        stagePerStaFrameForSingleUserTransmission(ac);
    if (fallbackEdcaf->getInProgressFrames()->getFrameToTransmit() != nullptr)
        return false;

    EV_WARN << "Channel granted without an eligible SU, DL-MU, or UL trigger frame; releasing channel.\n";
    fallbackEdcaf->releaseChannel(this);
    fallbackEdcaf->getTxopProcedure()->endTxop();
    return true;
}

void HeHcf::startFrameSequence(AccessCategory ac)
{
    if (forceNextSingleUser[ac]) {
        EV_INFO << "Start FS: forced single-user TXOP for AC " << ac << "\n";
        forceNextSingleUser[ac] = false;
        Hcf::startFrameSequence(ac);
        return;
    }

    ASSERT(modeSet != nullptr);
    bool isHeMode = strcmp(modeSet->getName(), "ax") == 0;
    if (isHeMode) {
        if (tryStartUlMuFrameSequence(ac))
            return;
        if (tryStartDlMuFrameSequence(ac))
            return;
    }
    else
        EV_INFO << "Non-HE mode, falling back to SU\n";

    if (releaseChannelIfNoFallbackFrame(ac))
        return;
    Hcf::startFrameSequence(ac);
}

void HeHcf::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
{
    std::vector<Edcaf *> collidedEdcafsWithFrame;
    for (auto edcaf : internallyCollidedEdcafs) {
        if (edcaf->getPendingQueue()->isEmpty() && edcaf->getInProgressFrames()->getLength() == 0)
            stagePerStaFrameForSingleUserTransmission(edcaf->getAccessCategory());
        if (edcaf->getInProgressFrames()->getFrameToTransmit() != nullptr)
            collidedEdcafsWithFrame.push_back(edcaf);
    }
    if (!collidedEdcafsWithFrame.empty())
        Hcf::handleInternalCollision(collidedEdcafsWithFrame);
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
    bool staged = stagePerStaFrameForSingleUserTransmission(ac);
    forceNextSingleUser[ac] = staged;
    EV_WARN << "DL MU planning failed for AC " << ac
            << ", forcing next TXOP single-user (staged = " << (staged ? "true" : "false") << ")\n";
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
    auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"));
    if (soundingCoordinator->processSoundingFrame(packet, header, mac, modeSet, csiManager, tx, this))
        return;

    if (auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(header)) {
        // IEEE 802.11-2024 Clause 26.5.2: processing of a received Trigger frame.
        // Trigger type 2 corresponds to a Basic Trigger (solicited HE TB PPDU).
        if (trigger->getTriggerType() == 2) {
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
        if (!ulCoordinator->isEnabled() || mac->isApInAxMode() ||
                mac->getMib()->bssStationData.associationId <= 0) {
            delete packet;
            return;
        }
        if (!triggeredUlExchanges.empty()) {
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

        TriggeredUlExchange exchange;
        exchange.tid = selectedTid;
        exchange.sourceQueue = sourceQueue;
        exchange.randomAccess = randomAccess;
        exchange.ru.index = selected->ruIndex;
        exchange.ru.toneSize = selected->ruToneSize;
        exchange.ru.toneOffset = selected->ruToneOffset;
        exchange.expectedResponseTime = simTime() + modeSet->getSifsTime();
        if (sourcePacket != nullptr) {
            exchange.packets.push_back(sourcePacket);
            exchange.sequenceNumbers.push_back(sourcePacket->peekAtFront<Ieee80211DataHeader>()->getSequenceNumber().get());

            // Basic Trigger UL aggregation is deliberately single-TID.  The
            // retained packets remain in their EDCA queue until the bitmap
            // arrives, so a partial Multi-STA BA can retry just the misses.
            int maximumMpduCount = std::min(64, availableSlots);
            for (int i = 0; ulBaAgreement != nullptr && (int)exchange.packets.size() < maximumMpduCount &&
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
            triggeredUlExchanges.emplace(trigger->getTriggerId(), std::move(exchange));
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
    if (ulCoordinator->isEnabled()) {
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            if (dataHeader->getBufferStatusPresent()) {
                auto aid = getAssociationId(dataHeader->getTransmitterAddress());
                if (aid > 0) {
                    ulCoordinator->updateBufferStatus(aid,
                            static_cast<AccessCategory>(dataHeader->getBufferStatusAc()),
                            dataHeader->getBufferStatusTid(), dataHeader->getBufferStatusQueueSize(), dataHeader->getRetry());
                }
            }
        }
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
    ASSERT(frameSequenceHandler != nullptr);
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
            ASSERT(!heMuTxop->getActiveAllocations().empty());
            EV_DEBUG << "HE DL MU container transmitted with "
                     << heMuTxop->getActiveAllocations().size() << " active allocations\n";
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

void HeHcf::originatorProcessTransmittedControlFrame(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac)
{
    // IEEE 802.11ax MU-BAR responses:
    // When a STA transmits a BlockAck response (specifically basic/compressed BlockAck) as a SIFS
    // reply to a MU-BAR trigger frame, the TX complete path invokes originatorProcessTransmittedControlFrame.
    // Base HCF only expects control frames that request a response (like RTS/BlockAckReq) to schedule
    // timeouts/recovery and throws "Unknown control frame" for a sent BlockAck. Since BlockAck is terminal
    // and does not expect SIFS feedback, we explicitly bypass it here.
    if (dynamicPtrCast<const Ieee80211BlockAck>(controlHeader) != nullptr) {
        return;
    }
    Hcf::originatorProcessTransmittedControlFrame(controlHeader, ac);
}

void HeHcf::originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket)
{
    Enter_Method("originatorProcessReceivedFrame");
    auto receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
    // IEEE 802.11ax Multi-STA Block Ack support:
    // In HE simulations, the AP aggregates block ACKs for multiple STAs in a single Multi-STA Block Ack frame.
    // When received by a STA after sending SU data via EDCA, HCF fails to process this new frame format,
    // throwing "Unknown control frame".
    // We intercept the Multi-STA Block Ack and translate it into a dummy standard BasicBlockAck so that
    // the existing QosAckHandler and OriginatorBlockAckAgreementHandler process the sequence bitmap correctly.
    if (auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(receivedHeader)) {
        auto edcaf = edca->getChannelOwner();
        if (edcaf) {
            auto myAid = mac->getMib()->bssStationData.associationId;
            const Ieee80211MultiStaBlockAckRecord *myRecord = nullptr;
            for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); ++i) {
                if (multiStaBlockAck->getRecords(i).aid == myAid) {
                    myRecord = &multiStaBlockAck->getRecords(i);
                    break;
                }
            }
            if (myRecord && myRecord->responseReceived) {
                auto dummyBlockAck = makeShared<Ieee80211BasicBlockAck>();
                dummyBlockAck->setReceiverAddress(multiStaBlockAck->getReceiverAddress());
                dummyBlockAck->setTransmitterAddress(multiStaBlockAck->getTransmitterAddress());
                dummyBlockAck->setTidInfo(myRecord->tid);
                dummyBlockAck->setStartingSequenceNumber(SequenceNumberCyclic(myRecord->startingSequenceNumber));
                
                // Map the Multi-STA record's 64-bit bitmap to the BasicBlockAck bitmap (Fragment 0).
                for (int seqOffset = 0; seqOffset < 64; ++seqOffset) {
                    bool acked = ((myRecord->bitmap >> seqOffset) & 1ULL) == 1ULL;
                    auto& bitmap = dummyBlockAck->getBlockAckBitmapForUpdate(seqOffset);
                    bitmap.setBit(0, acked);
                }
                EV_INFO << "MultiStaBlockAck matching our AID " << myAid << " converted to dummy BasicBlockAck" << std::endl;
                originatorProcessReceivedControlFrame(receivedPacket, dummyBlockAck, lastTransmittedPacket, lastTransmittedPacket->peekAtFront<Ieee80211MacHeader>(), edcaf->getAccessCategory());
            }
        }
        return;
    }
    Hcf::originatorProcessReceivedFrame(receivedPacket, lastTransmittedPacket);
}

void HeHcf::originatorProcessFailedFrame(Packet *failedPacket)
{
    Enter_Method("originatorProcessFailedFrame");
    ASSERT(failedPacket != nullptr);
    EV_WARN << "HE MU: transmission failed for frame " << failedPacket->getName()
            << " type = " << (failedPacket->peekAtFront<Ieee80211MacHeader>() != nullptr ? (int)failedPacket->peekAtFront<Ieee80211MacHeader>()->getType() : -1) << endl;
    if (dynamic_cast<const HeDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence()) != nullptr) {
        // In an MU TXOP a failed sub-frame is re-queued to its per-STA queue bank
        // instead of being retried inside the shared EDCAF pending queue. This keeps
        // each destination's retries together and lets the next scheduler run select
        // the best subset again.
        ASSERT(edca->getChannelOwner() != nullptr);
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
                EV_INFO << "HE DL MU retrying frame: " << failedPacket->getName() << ", re-queuing.\n";
                auto h = failedPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
                ASSERT(h != nullptr);
                h->setRetry(true);
                failedPacket->insertAtFront(h);

                // Remove from inProgressFrames
                edcaf->getInProgressFrames()->removeInProgressFrame(failedPacket);

                // Re-enqueue into the destination STA's queue bank when available.
                auto pendingQueue = resolvePerStaQueue(failedHeader->getReceiverAddress(), edcaf->getAccessCategory());
                ASSERT(pendingQueue != nullptr);
                pendingQueue->pushPacket(failedPacket, nullptr);
            }
        }
    }
    else {
        Hcf::originatorProcessFailedFrame(failedPacket);
    }
}

void HeHcf::transmitFrame(Packet *packet, simtime_t ifs)
{
    Enter_Method("transmitFrame");
    if (mac->getMib()->bssStationData.associationId > 0) {
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            if (dataHeader->getType() == ST_DATA_WITH_QOS) {
                auto tid = dataHeader->getTid();
                auto ac = mapTidToAccessCategory(tid);
                auto pendingQueue = edca->getEdcaf(ac)->getPendingQueue();
                int64_t queueBytes = 0;
                for (int i = 0; i < pendingQueue->getNumPackets(); i++) {
                    auto queuedPacket = pendingQueue->getPacket(i);
                    auto queuedHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                            queuedPacket->peekAtFront<Ieee80211MacHeader>());
                    if (queuedHeader != nullptr && queuedHeader->getTid() == tid)
                        queueBytes += queuedPacket->getByteLength();
                }
                auto writableHeader = packet->removeAtFront<Ieee80211DataHeader>();
                if (!writableHeader->getBufferStatusPresent())
                    writableHeader->setChunkLength(writableHeader->getChunkLength() + B(4));
                writableHeader->setOrder(true);
                writableHeader->setBufferStatusPresent(true);
                writableHeader->setBufferStatusTid(tid);
                writableHeader->setBufferStatusAc(ac);
                writableHeader->setBufferStatusQueueSize(queueBytes);
                packet->insertAtFront(writableHeader);
            }
        }
    }
    Hcf::transmitFrame(packet, ifs);
}

void HeHcf::legacyPreambleReceived(Packet *packet)
{
    auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"));
    soundingCoordinator->processLegacyPreamble(packet);
}

} // namespace ieee80211
} // namespace inet
