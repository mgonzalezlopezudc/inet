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

// HE HCF transmit/receive callbacks.

namespace inet {
namespace ieee80211 {

void HeHcf::recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"));
    if (soundingCoordinator->processSoundingFrame(packet, header, mac, modeSet, csiManager, tx, this))
        return;

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

void HeHcf::originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket)
{
    Enter_Method("originatorProcessReceivedFrame");
    auto receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
    // IEEE 802.11-2024 26.4.2 Multi-STA BlockAck support:
    // The standard says the originator examines the Per AID TID Info field
    // matching its AID/TID and processes the Block Ack bitmap according to
    // 10.25.6.  INET's base HCF understands BasicBlockAck, so this bridge
    // converts the matching record into an equivalent BasicBlockAck bitmap.
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
                
                // 26.4.3 allows a 64-bit Multi-STA BA bitmap for buffer sizes
                // up to 64.  Map it to the BasicBlockAck fragment-0 bitmap so
                // the existing originator BA handler applies the same sequence
                // acknowledgments.
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
                // HE non-AP STAs include BSR in QoS Control/HT Control style
                // metadata so the AP can make 26.5.2.2 UL Trigger decisions.
                // INET models that information directly on the data header.
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
