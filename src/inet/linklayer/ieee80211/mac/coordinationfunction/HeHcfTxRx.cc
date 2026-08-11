//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

#include <algorithm>
#include <set>
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
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF transmit/receive callbacks.

namespace inet {
namespace ieee80211 {

static bool isHeNdpPacket(const Packet *packet)
{
    auto request = packet == nullptr ? nullptr :
            packet->findTag<physicallayer::Ieee80211HeTxVectorReq>();
    return packet != nullptr && packet->getDataLength() == B(0) &&
            request != nullptr && request->getPpduLayout() != nullptr &&
            request->getPpduLayout()->isNdp();
}

static bool isHeTbPacket(const Packet *packet)
{
    if (packet == nullptr)
        return false;
    auto request = packet->findTag<physicallayer::Ieee80211HeTxVectorReq>();
    if (request != nullptr && request->getTxVector() != nullptr &&
            request->getTxVector()->getCommon().getParameters().ppduFormat ==
                    physicallayer::HE_TRIGGER_BASED_UPLINK)
        return true;
    // Preserve the Trigger correlation as the canonical fallback for the two
    // headerless HE-TB representations: an A-MPDU starts with a delimiter and
    // an NDP feedback report is empty.
    return packet->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>() != nullptr &&
            (packet->getDataLength() == B(0) ||
             dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                     packet->peekAtFront()) != nullptr);
}

bool HeHcf::processHeaderlessNdpIndication(Packet *packet)
{
    auto indication = packet->findTag<physicallayer::Ieee80211NdpInd>();
    if (indication == nullptr || indication->getPhyFormat() !=
            physicallayer::IEEE80211_NDP_PHY_HE_SU)
        return false;
    auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(
            getSubmodule("soundingCoordinator"));
    return soundingCoordinator->processSoundingFrame(packet, nullptr, mac,
            modeSet, csiManager, getLinkPhyContext(), tx, this);
}

void HeHcf::recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"));
    if (soundingCoordinator->processSoundingFrame(packet, header, mac, modeSet, csiManager,
            getLinkPhyContext(), tx, this))
        return;

    if (isHeTbPacket(packet)) {
        EV_INFO << "Discarding HE-TB response outside active Trigger collection\n";
        PacketDropDetails details;
        details.setReason(NOT_ADDRESSED_TO_US);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }

    if (auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(header)) {
        // 9.3.1.22 Trigger frames are control frames that solicit HE TB
        // responses; do not pass them through the legacy HCF recipient path.
        processReceivedTriggerFrame(packet, trigger);
        return;
    }
    if (auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(header)) {
        // 26.4.2 defines per-AID/TID Multi-STA BA records.  Triggered UL
        // responses retain their own pending exchange state, so handle them
        // before the base BlockAck path.
        processReceivedMultiStaBlockAck(packet, multiStaBlockAck);
        return;
    }
    if (ulCoordinator->isEnabled()) {
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            if (dataHeader->getBufferStatusPresent()) {
                auto aid = getAssociationId(dataHeader->getTransmitterAddress());
                if (aid > 0) {
                    ulCoordinator->updateBufferStatus(aid, dataHeader->getTransmitterAddress(),
                            static_cast<AccessCategory>(dataHeader->getBufferStatusAc()),
                            dataHeader->getBufferStatusTid(), dataHeader->getBufferStatusQueueSize());
                }
            }
        }
    }
    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
        if (dataHeader->getOperatingModePresent() && mac->getMib()->localHeCapabilities.omControl) {
            Ieee80211HeOperatingMode mode;
            mode.channelWidth = dataHeader->getOperatingModeChannelWidth();
            mode.rxNss = dataHeader->getOperatingModeRxNss();
            mode.ulMuDisable = dataHeader->getOperatingModeUlMuDisable();
            updatePeerOperatingMode(dataHeader->getTransmitterAddress(), mode);
            EV_INFO << "Accepted HE OMI from " << dataHeader->getTransmitterAddress()
                    << ": width=" << (int)mode.channelWidth << " rxNss=" << (int)mode.rxNss
                    << " ulMuDisable=" << mode.ulMuDisable << endl;
        }
    }
    Hcf::recipientProcessReceivedFrame(packet, header);
}

void HeHcf::transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (isHeTbPacket(packet)) {
        // IEEE Std 802.11-2024, 26.2.7: start MUEDCATimer[AC] at HE-TB PPDU
        // completion only for successful QoS Data that needs no immediate
        // acknowledgment. Immediate-ack data is deferred to its correlated
        // Multi-STA BA success decision, and QoS Null must not activate it.
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
        if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS &&
                dataHeader->getAckPolicy() == NO_ACK) {
            AccessCategory ac = edca->mapTidToAc(dataHeader->getTid());
            if (ac >= 0 && ac < 4)
                edca->getEdcaf(ac)->startMuEdcaTimer();
        }
        return;
    }
    Hcf::transmissionComplete(packet, header);
}

void HeHcf::originatorProcessTransmittedFrame(Packet *packet)
{
    Enter_Method("originatorProcessTransmittedFrame");
    ASSERT(frameSequenceHandler != nullptr);
    if (isHeNdpPacket(packet)) {
        // IEEE 802.11-2024 26.7.3: the sounding NDP has a PHY preamble but no
        // PSDU, hence no MAC header to enter acknowledgement, BA, or
        // content-derived packet statistics.
        return;
    }
    if (isHeTbPacket(packet)) {
        // IEEE Std 802.11-2024 10.3.2.13.3 and 26.4.4.5: Normal Ack in
        // this HE-TB MPDU solicits the terminal Multi-STA Block Ack. The
        // triggeredUlExchanges ledger owns that response and retry state, so
        // the legacy single-user Ack state machine must not start in parallel.
        return;
    }
    if (dynamic_cast<const HeUlMuTxOpFs *>(frameSequenceHandler->getFrameSequence()) != nullptr) {
        auto edcaf = edca->getChannelOwner();
        if (edcaf != nullptr)
            edcaf->emit(packetSentToPeerSignal, packet);
        return;
    }
    auto heMuTxop = dynamic_cast<const HeDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence());
    if (heMuTxop != nullptr && heMuTxop->isContainerPacket(packet)) {
        // The HE MU PPDU is one PHY transmission but contains per-user MPDUs.
        // 26.4/10.25 BlockAck state is per recipient/TID, so each contained
        // MPDU must enter the normal originator in-progress and BA state.
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
    // IEEE 802.11-2024 9.3.1.22.4 MU-BAR responses:
    // When a STA transmits a BlockAck response as a SIFS reply to a MU-BAR
    // Trigger, the TX complete path invokes originatorProcessTransmittedControlFrame.
    // Base HCF only expects control frames that request a response (like RTS/BlockAckReq) to schedule
    // timeouts/recovery and throws "Unknown control frame" for a sent BlockAck. Since BlockAck is terminal
    // and does not expect SIFS feedback, we explicitly bypass it here.
    if (dynamicPtrCast<const Ieee80211BlockAck>(controlHeader) != nullptr) {
        return;
    }
    Hcf::originatorProcessTransmittedControlFrame(controlHeader, ac);
}

bool HeHcf::reportHeDlMuTxResult(Packet *packet, AccessCategory ac, bool success)
{
    auto heMuTxop = frameSequenceHandler == nullptr ? nullptr :
            dynamic_cast<const HeDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence());
    auto heRateControl = dynamic_cast<IIeee80211HeRateControl *>(dataAndMgmtRateControl);
    if (packet == nullptr || heMuTxop == nullptr || heRateControl == nullptr)
        return false;

    if (ac < 0 || ac >= 4)
        return false;
    auto edcaf = edca->getEdcaf(ac);
    for (const auto& allocation : heMuTxop->getActiveAllocations()) {
        if (std::find(allocation.packets.begin(), allocation.packets.end(), packet) ==
                allocation.packets.end())
            continue;
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekAtFront<Ieee80211MacHeader>());
        if (dataHeader == nullptr)
            return false;
        int retryCount = dataHeader->getRetry() ?
                edcaf->getRecoveryProcedure()->getRetryCount(packet, dataHeader) : 0;
        heRateControl->reportHeTxResult(allocation.staAddress, allocation.mcs,
                allocation.numberOfSpatialStreams, allocation.ru.toneSize,
                retryCount, success, success ? packet->getByteLength() : 0);
        return true;
    }
    return false;
}

void HeHcf::originatorProcessBlockAckResult(
        const Ptr<const Ieee80211BlockAck>& blockAck,
        const std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>>& ackedFrames,
        AccessCategory ac)
{
    auto heMuTxop = frameSequenceHandler == nullptr ? nullptr :
            dynamic_cast<const HeDlMuTxOpFs *>(frameSequenceHandler->getFrameSequence());
    if (heMuTxop == nullptr)
        return;

    for (const auto& allocation : heMuTxop->getActiveAllocations()) {
        if (allocation.staAddress != blockAck->getTransmitterAddress())
            continue;
        for (auto packet : allocation.packets) {
            auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    packet->peekAtFront<Ieee80211MacHeader>());
            if (dataHeader == nullptr || !dataHeader->getSequenceNumber().isValid())
                continue;
            auto key = std::make_pair(dataHeader->getReceiverAddress(),
                    std::make_pair(dataHeader->getTid(),
                            SequenceControlField(dataHeader->getSequenceNumber().get(),
                                    dataHeader->getFragmentNumber())));
            bool tidCovered = false;
            if (auto basic = dynamicPtrCast<const Ieee80211BasicBlockAck>(blockAck))
                tidCovered = dataHeader->getTid() == basic->getTidInfo();
            else if (auto compressed = dynamicPtrCast<const Ieee80211CompressedBlockAck>(blockAck))
                tidCovered = dataHeader->getTid() == compressed->getTidInfo();
            else if (auto multiTid = dynamicPtrCast<const Ieee80211MultiTidBlockAck>(blockAck))
                for (unsigned int i = 0; i < multiTid->getRecordsArraySize(); ++i)
                    tidCovered |= dataHeader->getTid() == multiTid->getRecords(i).tid;
            if (!tidCovered)
                continue;
            if (ackedFrames.count(key) != 0)
                reportHeDlMuTxResult(packet, ac, true);
            else
                reportHeDlMuTxResult(packet, ac, false);
        }
    }
}

void HeHcf::originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket)
{
    Enter_Method("originatorProcessReceivedFrame");
    auto receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
    if (auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(receivedHeader)) {
        // Triggered UL has its own packet ledger and exact Trigger correlation.
        // Preserve that owner before considering the UL-SU BlockAck path.
        if (receivedPacket->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>() != nullptr) {
            processReceivedMultiStaBlockAck(receivedPacket->dup(), multiStaBlockAck);
            return;
        }
        auto lastTransmittedHeader = lastTransmittedPacket == nullptr ? nullptr :
                dynamicPtrCast<const Ieee80211MacHeader>(
                        lastTransmittedPacket->peekAtFront());
        auto multiTidBlockAckReq =
                dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(
                        lastTransmittedHeader);
        bool validUlSuResponse = multiTidBlockAckReq != nullptr;
        uint16_t expectedAid = 0;
        auto mib = mac->getMib();
        if (validUlSuResponse &&
                mib->getStationType() == Ieee80211Mib::STATION) {
            auto associationId = mib->getLocalAssociationId();
            validUlSuResponse = associationId > 0 && associationId <= 2007;
            if (validUlSuResponse)
                expectedAid = associationId;
        }
        else if (validUlSuResponse &&
                mib->getStationType() != Ieee80211Mib::ACCESS_POINT)
            validUlSuResponse = false;
        if (validUlSuResponse) {
            validUlSuResponse =
                    multiStaBlockAck->getReceiverAddress() ==
                            multiTidBlockAckReq->getTransmitterAddress() &&
                    multiStaBlockAck->getTransmitterAddress() ==
                            multiTidBlockAckReq->getReceiverAddress() &&
                    multiStaBlockAck->getRecordsArraySize() ==
                            multiTidBlockAckReq->getRecordsArraySize();
        }
        std::set<std::pair<Tid, uint16_t>> requestedRecords;
        if (validUlSuResponse) {
            for (unsigned int i = 0;
                    i < multiTidBlockAckReq->getRecordsArraySize(); ++i) {
                const auto& record = multiTidBlockAckReq->getRecords(i);
                validUlSuResponse &= requestedRecords.emplace(record.tid,
                        record.startingSequenceNumber).second;
            }
        }
        std::set<std::pair<Tid, uint16_t>> responseRecords;
        if (validUlSuResponse) {
            for (unsigned int i = 0;
                    i < multiStaBlockAck->getRecordsArraySize(); ++i) {
                const auto& record = multiStaBlockAck->getRecords(i);
                auto key = std::make_pair(static_cast<Tid>(record.tid),
                        record.startingSequenceNumber);
                validUlSuResponse &= record.aid == expectedAid &&
                        requestedRecords.count(key) == 1 &&
                        responseRecords.insert(key).second;
            }
            validUlSuResponse &= responseRecords == requestedRecords;
        }
        // IEEE Std 802.11-2024, 10.25.5, 26.4.2 and 26.4.5:
        // only a matching per-AID/TID Multi-STA response to the preceding
        // HE Multi-TID BAR completes the ordinary originator exchange.
        if (validUlSuResponse) {
            Hcf::originatorProcessReceivedFrame(receivedPacket,
                    lastTransmittedPacket);
            return;
        }
        if (multiTidBlockAckReq != nullptr)
            EV_WARN << "Discarding invalid UL-SU Multi-STA Block Ack response\n";
        // FrameSequenceHandler owns the received frame on the originator path;
        // the transaction processor consumes its argument.
        processReceivedMultiStaBlockAck(receivedPacket->dup(), multiStaBlockAck);
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
        if (!exchangeCoordinator.beginRetryOrRecovery(failedPacket))
            return;
        aggregationService.discardTransmission(failedPacket);
        // 26.5.1 extends EDCA success/failure semantics for DL MU, but retry
        // state is still per MPDU/TID.  Requeue a failed subframe to the
        // destination's per-STA queue so the next DL MU scheduler run can choose
        // a standard-valid subset again.
        ASSERT(edca->getChannelOwner() != nullptr);
        auto failedHeader = failedPacket->peekAtFront<Ieee80211MacHeader>();
        auto edcaf = edca->getChannelOwner();
        if (edcaf) {
            bool retryLimitReached = false;
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader)) {
                edcaf->getRecoveryProcedure()->dataFrameTransmissionFailed(failedPacket, dataHeader);
                retryLimitReached = edcaf->getRecoveryProcedure()->isRetryLimitReached(failedPacket, dataHeader);
                bool heResultReported = reportHeDlMuTxResult(
                        failedPacket, edcaf->getAccessCategory(), false);
                if (dataAndMgmtRateControl && !heResultReported) {
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
                processFailedBlockAckReq(edcaf, blockAckReq, true);
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
    if (isHeNdpPacket(packet)) {
        exchangeCoordinator.beginTransmission(packet);
        // Frame-sequence transmission normally derives the Tx header by
        // peeking packet content. A sounding NDP is intentionally empty, so
        // retain a detached header only for the local Tx callback/address
        // contract, as is already done for triggered NDP feedback.
        auto ndpHeader = makeShared<Ieee80211DataHeader>();
        ndpHeader->setType(ST_QOS_NULL);
        ndpHeader->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        ndpHeader->setTransmitterAddress(mac->getAddress());
        ndpHeader->setAddress3(mac->getMib()->getBssid());
        ndpHeader->setDurationField(SIMTIME_ZERO);
        ndpHeader->setChunkLength(B(30));
        tx->transmitFrame(packet, ndpHeader, ifs, this);
        return;
    }
    if (mac->getMib()->getLocalAssociationId() > 0) {
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            if (dataHeader->getType() == ST_DATA_WITH_QOS) {
                // HE non-AP STAs include BSR in QoS Control/HT Control style
                // metadata so the AP can make 26.5.2.2 UL Trigger decisions.
                // INET models that information directly on the data header.
                auto tid = dataHeader->getTid();
                auto ac = mapTidToAccessCategory(tid);
                auto edcaf = edca->getEdcaf(ac);
                auto queueBytes = getBufferedTrafficServiceBytes(edcaf,
                        dataHeader->getReceiverAddress(), tid);
                auto writableHeader = packet->removeAtFront<Ieee80211DataHeader>();
                if (!writableHeader->getBufferStatusPresent() && !writableHeader->getOperatingModePresent())
                    writableHeader->setChunkLength(writableHeader->getChunkLength() + B(4));
                writableHeader->setOrder(true);
                if (par("sendOperatingModeIndication").boolValue() && mac->getMib()->localHeCapabilities.omControl) {
                    writableHeader->setOperatingModePresent(true);
                    writableHeader->setOperatingModeChannelWidth(par("operatingModeChannelWidth"));
                    writableHeader->setOperatingModeRxNss(par("operatingModeRxNss"));
                    writableHeader->setOperatingModeUlMuDisable(par("operatingModeUlMuDisable"));
                }
                else {
                    writableHeader->setBufferStatusPresent(true);
                    writableHeader->setBufferStatusTid(tid);
                    writableHeader->setBufferStatusAc(ac);
                    writableHeader->setBufferStatusQueueSize(queueBytes);
                }
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
