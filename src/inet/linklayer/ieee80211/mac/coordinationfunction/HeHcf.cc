//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"

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

} // namespace

namespace inet {
namespace ieee80211 {

Define_Module(HeHcf);

void HeHcf::initialize(int stage)
{
    Hcf::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        dlScheduler = check_and_cast<IIeee80211HeDlScheduler *>(getSubmodule("dlScheduler"));
        delete frameSequenceHandler;
        frameSequenceHandler = new HeFrameSequenceHandler();
    }
}

std::vector<MacAddress> HeHcf::collectCandidateStations(queueing::IPacketQueue *queue) const
{
    std::vector<MacAddress> candidates;
    std::vector<MacAddress> seenDestinations;
    auto baHandler = getOriginatorBlockAckAgreementHandler();
    int n = queue->getNumPackets();
    for (int i = 0; i < n; ++i) {
        Packet *pkt = queue->getPacket(i);
        const auto& header = pkt->peekAtFront<Ieee80211MacHeader>();
        MacAddress dest = header->getReceiverAddress();
        if (dest.isMulticast() || dest.isBroadcast())
            continue;
        bool seen = false;
        for (const auto& c : seenDestinations) {
            if (c == dest) { seen = true; break; }
        }
        if (seen)
            continue;
        seenDestinations.push_back(dest);

        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            if (isMuEligibleDataHeader(dataHeader, baHandler))
                candidates.push_back(dest);
        }
    }
    return candidates;
}

void HeHcf::startFrameSequence(AccessCategory ac)
{
    // Check whether HE mode and multi-user conditions are met.
    bool isHeMode = (modeSet != nullptr && strcmp(modeSet->getName(), "ax") == 0);
    if (isHeMode) {
        auto edcaf = edca->getEdcaf(ac);
        auto pendingQueue = edcaf->getPendingQueue();
        auto inProgress = edcaf->getInProgressFrames();
        if (inProgress->getLength() > 0) {
            EV_INFO << "HeHcf: Pushing " << inProgress->getLength()
                    << " abandoned in-progress frames back to pendingQueue before starting MU sequence." << endl;
            std::vector<Packet *> framesToRequeue;
            for (int i = 0; i < inProgress->getLength(); ++i) {
                framesToRequeue.push_back(inProgress->getFrames(i));
            }
            for (auto frame : framesToRequeue) {
                inProgress->removeInProgressFrame(frame);
                pendingQueue->pushPacket(frame, nullptr);
            }
        }
        auto headDataHeader = getEligibleHoLDataHeader(pendingQueue);
        auto baHandler = getOriginatorBlockAckAgreementHandler();
        if (!isMuEligibleDataHeader(headDataHeader, baHandler)) {
            if (headDataHeader != nullptr) {
                EV_INFO << "HeHcf: earliest SU-transmittable packet "
                        << headDataHeader->getReceiverAddress() << " tid=" << headDataHeader->getTid()
                        << " is MU-ineligible, falling back to Hcf::startFrameSequence(ac)." << endl;
            }
            Hcf::startFrameSequence(ac);
            return;
        }
        auto candidates = collectCandidateStations(pendingQueue);
        if (candidates.size() >= 2) {
            EV_INFO << "HeHcf: MU-OFDMA opportunity detected for " << candidates.size()
                    << " STAs — starting HeDlMuTxOpFs." << endl;
            frameSequenceHandler->startFrameSequence(
                    new HeDlMuTxOpFs(dlScheduler, candidates, modeSet,
                                     pendingQueue, edcaf->getAckHandler(), this),
                    buildContext(ac), this);
            emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
            return;
        }
    }
    // Fallback: standard single-user frame sequence.
    Hcf::startFrameSequence(ac);
}

void HeHcf::originatorProcessTransmittedFrame(Packet *packet)
{
    Enter_Method("originatorProcessTransmittedFrame");
    auto muTag = packet->findTag<physicallayer::Ieee80211HeMuTag>();
    if (muTag != nullptr) {
        auto edcaf = edca->getChannelOwner();
        if (edcaf) {
            AccessCategory ac = edcaf->getAccessCategory();
            // Process each individual scheduled frame as transmitted
            for (const auto& alloc : muTag->getAllocations()) {
                Packet *staPacket = alloc.packet;
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
                
                // Re-enqueue into pendingQueue
                auto pendingQueue = edcaf->getPendingQueue();
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

