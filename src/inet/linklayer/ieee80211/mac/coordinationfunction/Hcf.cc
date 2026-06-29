//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"

#include <cstring>

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/twt/ITwtManager.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/recipient/RecipientAckProcedure.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

simsignal_t Hcf::edcaCollisionDetectedSignal = cComponent::registerSignal("edcaCollisionDetected");
simsignal_t Hcf::blockAckAgreementAddedSignal = cComponent::registerSignal("blockAckAgreementAdded");
simsignal_t Hcf::blockAckAgreementDeletedSignal = cComponent::registerSignal("blockAckAgreementDeleted");

Define_Module(Hcf);

static bool isHeMuContainerPacket(Packet *packet)
{
    if (packet == nullptr || !packet->hasAtFront<Ieee80211MacHeader>())
        return false;
    auto header = packet->peekAtFront<Ieee80211MacHeader>();
    auto payloadOffset = header->getChunkLength();
    return packet->getDataLength() > payloadOffset &&
           dynamicPtrCast<const Ieee80211HeMuRuPayloadHeader>(packet->peekDataAt(payloadOffset)) != nullptr;
}

void Hcf::initialize(int stage)
{
    ModeSetListener::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        mac = check_and_cast<Ieee80211Mac *>(getContainingNicModule(this)->getSubmodule("mac"));
        startRxTimer = new cMessage("startRxTimeout");
        inactivityTimer = new cMessage("blockAckInactivityTimer");
        edca = check_and_cast<Edca *>(getSubmodule("edca"));
        hcca = check_and_cast<Hcca *>(getSubmodule("hcca"));
        tx = check_and_cast<ITx *>(getModuleByPath(par("txModule")));
        rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
        dataAndMgmtRateControl = dynamic_cast<IRateControl *>(getSubmodule("rateControl"));
        originatorBlockAckAgreementPolicy = dynamic_cast<IOriginatorBlockAckAgreementPolicy *>(getSubmodule("originatorBlockAckAgreementPolicy"));
        recipientBlockAckAgreementPolicy = dynamic_cast<IRecipientBlockAckAgreementPolicy *>(getSubmodule("recipientBlockAckAgreementPolicy"));
        rateSelection = check_and_cast<IQosRateSelection *>(getSubmodule("rateSelection"));
        frameSequenceHandler = new FrameSequenceHandler();
        WATCH_EXPR("frameSequenceInfo", getFrameSequenceInfo());
        originatorDataService = check_and_cast<IOriginatorMacDataService *>(getSubmodule(("originatorMacDataService")));
        recipientDataService = check_and_cast<IRecipientQosMacDataService *>(getSubmodule("recipientMacDataService"));
        originatorAckPolicy = check_and_cast<IOriginatorQoSAckPolicy *>(getSubmodule("originatorAckPolicy"));
        recipientAckPolicy = check_and_cast<IRecipientQosAckPolicy *>(getSubmodule("recipientAckPolicy"));
        singleProtectionMechanism = check_and_cast<SingleProtectionMechanism *>(getSubmodule("singleProtectionMechanism"));
        rtsProcedure = new RtsProcedure();
        rtsPolicy = check_and_cast<IRtsPolicy *>(getSubmodule("rtsPolicy"));
        recipientAckProcedure = new RecipientAckProcedure();
        ctsProcedure = new CtsProcedure();
        ctsPolicy = check_and_cast<ICtsPolicy *>(getSubmodule("ctsPolicy"));
        if (originatorBlockAckAgreementPolicy && recipientBlockAckAgreementPolicy) {
            recipientBlockAckAgreementHandler = new RecipientBlockAckAgreementHandler();
            originatorBlockAckAgreementHandler = new OriginatorBlockAckAgreementHandler();
            originatorBlockAckProcedure = new OriginatorBlockAckProcedure();
            recipientBlockAckProcedure = new RecipientBlockAckProcedure();
        }
    }
}

std::string Hcf::getFrameSequenceInfo() const
{
    if (!frameSequenceHandler->isSequenceRunning())
        return "";
    auto history = frameSequenceHandler->getFrameSequence()->getHistory();
    if (history.length() > 32) {
        history.erase(history.begin(), history.end() - 32);
        history = "..." + history;
    }
    return "Fs: " + history;
}

void Hcf::forEachChild(cVisitor *v)
{
    SimpleModule::forEachChild(v);
    if (frameSequenceHandler != nullptr && frameSequenceHandler->getContext() != nullptr)
        v->visit(const_cast<FrameSequenceContext *>(frameSequenceHandler->getContext()));
}

void Hcf::handleMessage(cMessage *msg)
{
    if (msg == startRxTimer) {
        if (!isReceptionInProgress()) {
            frameSequenceHandler->handleStartRxTimeout();
        }
    }
    else if (msg == inactivityTimer) {
        if (originatorBlockAckAgreementHandler && recipientBlockAckAgreementHandler) {
            originatorBlockAckAgreementHandler->blockAckAgreementExpired(this, this);
            recipientBlockAckAgreementHandler->blockAckAgreementExpired(this, this);
        }
        else
            throw cRuntimeError("Unknown event");
    }
    else
        throw cRuntimeError("Unknown msg type");
}

void Hcf::refreshDisplay() const
{
    ModeSetListener::refreshDisplay();
    if (frameSequenceHandler->isSequenceRunning()) {
        auto history = frameSequenceHandler->getFrameSequence()->getHistory();
        getDisplayString().setTagArg("tt", 0, ("Fs: " + history).c_str());
    }
    else {
        getDisplayString().removeTag("tt");
    }
}

void Hcf::processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    Enter_Method("processUpperFrame(%s)", packet->getName());
    take(packet);
    EV_INFO << "Processing upper frame: " << packet->getName() << endl;
    // TODO
    // A QoS STA should send individually addressed Management frames that are addressed to a non-QoS STA
    // using the access category AC_BE and shall send all other management frames using the access category
    // AC_VO. A QoS STA that does not send individually addressed Management frames that are addressed to a
    // non-QoS STA using the access category AC_BE shall send them using the access category AC_VO.
    // Management frames are exempted from any and all restrictions on transmissions arising from admission
    // control procedures.
    AccessCategory ac = AccessCategory(-1);
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(header)) // TODO + non-QoS frames
        // IEEE Std 802.11-2024, 10.2.3.2: QoS STAs send most Management
        // frames using AC_VO; individually addressed Management frames to a
        // non-QoS STA may use AC_BE.
        ac = AccessCategory::AC_VO;
    else if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header))
        // IEEE Std 802.11-2024, 10.23.2.1: EDCA maps UP/TID to one of the
        // four AC transmit queues and runs one EDCAF per AC.
        ac = edca->classifyFrame(dataHeader);
    else
        throw cRuntimeError("Unknown message type");
    EV_INFO << "The upper frame has been classified as a " << printAccessCategory(ac) << " frame." << endl;
    
    // Determine if this frame should use per-STA queue (HE OFDMA scheduling).
    // Only AP+ax mode uses per-STA queues; all other cases use shared EDCAF queues.
    queueing::IPacketQueue *pendingQueue = nullptr;
    MacAddress destMac = header->getReceiverAddress();
    
    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
        // Check if unicast (not multicast or broadcast)
        if (!destMac.isMulticast() && !destMac.isBroadcast()) {
            pendingQueue = resolvePerStaQueue(destMac, ac);
            
            // Add original enqueue timestamp if not already present
            if (!packet->findTag<OrigEnqueueTimeTag>()) {
                auto timeTag = packet->addTagIfAbsent<OrigEnqueueTimeTag>();
                timeTag->setEnqueueTime(simTime());
            }
        }
    }
    
    // Fallback to shared queue if per-STA queue not available
    if (!pendingQueue) {
        pendingQueue = edca->getEdcaf(ac)->getPendingQueue();
    }
    
    // IEEE Std 802.11-2024, 10.23.2.2: queuing a frame for an empty AC can
    // invoke the EDCAF backoff procedure; Edcaf owns AIFS/CW/backoff timing.
    pendingQueue->enqueuePacket(packet);
    if (!pendingQueue->isEmpty()) {
        auto edcaf = edca->getChannelOwner();
        if (edcaf == nullptr || edcaf->getAccessCategory() != ac) {
            EV_DETAIL << "Requesting channel for access category " << printAccessCategory(ac) << endl;
            edca->requestChannelAccess(ac, this);
        }
    }
}

void Hcf::scheduleStartRxTimer(simtime_t timeout)
{
    Enter_Method("scheduleStartRxTimer");
    cancelEvent(startRxTimer);
    scheduleAfter(timeout, startRxTimer);
}

void Hcf::scheduleInactivityTimer(simtime_t timeout)
{
    Enter_Method("scheduleInactivityTimer");
    rescheduleAfter(timeout, inactivityTimer);
}

void Hcf::processLowerFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    Enter_Method("processLowerFrame(%s)", packet->getName());
    take(packet);
    EV_INFO << "Processing lower frame: " << packet->getName() << endl;
    auto edcaf = edca->getChannelOwner();
    if (edcaf && frameSequenceHandler->isSequenceRunning()) {
        // IEEE Std 802.11-2024, 10.23.2.2 plus 10.3.2.9/10.3.2.11:
        // EDCA treats the MPDU exchange as failed unless the timeout sees a
        // response from the expected recipient (or a control response without TA).
        // TODO always call processResponse?
        if ((!isForUs(header) && !startRxTimer->isScheduled()) || isForUs(header)) {
            auto receiveStep = dynamic_cast<IReceiveStep *>(frameSequenceHandler->getContext()->getLastStep());
            bool completesOnReception = receiveStep == nullptr || receiveStep->completesOnReception();
            frameSequenceHandler->processResponse(packet);
            // Only cancel RxTimer when the current running sequence has been handled by frameSequenceHandler->processResponse().
            // If the received frame is not for us, we are still waiting to receive our ACK. In that case, don't cancel the timer.
            // Otherwise, current frame sequence stucks in RX step and runs longer than intendeed, preventing sequence from
            // another access category (AC) to start running (RuntimeError("Channel access granted while a frame sequence is running")).
            if (completesOnReception)
                cancelEvent(startRxTimer);
        }
        else {
            EV_INFO << "This frame is not for us" << std::endl;
            PacketDropDetails details;
            details.setReason(NOT_ADDRESSED_TO_US);
            emit(packetDroppedSignal, packet, &details);
            delete packet;
        }
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented!");
    else if (isForUs(header))
        recipientProcessReceivedFrame(packet, header);
    else {
        EV_INFO << "This frame is not for us" << std::endl;
        PacketDropDetails details;
        details.setReason(NOT_ADDRESSED_TO_US);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
    }
}

void Hcf::channelGranted(IChannelAccess *channelAccess)
{
    Enter_Method("channelGranted");
    auto edcaf = check_and_cast<Edcaf *>(channelAccess);
    if (edcaf) {
        if (tx->isBusy()) {
            EV_WARN << "Channel access granted to the " << printAccessCategory(edcaf->getAccessCategory())
                    << " queue while tx is busy (e.g. pending sequential Ack). Releasing channel.\n";
            edcaf->releaseChannel(this);
            return;
        }
        AccessCategory ac = edcaf->getAccessCategory();
        EV_DETAIL << "Channel access granted to the " << printAccessCategory(ac) << " queue" << std::endl;
        // IEEE Std 802.11-2024, 10.23.2.3 and 10.23.2.4: an EDCAF whose
        // backoff reaches zero obtains an EDCA TXOP for its primary AC.
        edcaf->getTxopProcedure()->startTxop(ac);
        auto internallyCollidedEdcafs = edca->getInternallyCollidedEdcafs();
        if (internallyCollidedEdcafs.size() > 0) {
            EV_INFO << "Internal collision happened with the following queues:" << std::endl;
            handleInternalCollision(internallyCollidedEdcafs);
            emit(edcaCollisionDetectedSignal, (unsigned long)internallyCollidedEdcafs.size());
        }
        startFrameSequence(ac);
    }
    else
        throw cRuntimeError("Channel access granted but channel owner not found!");
}

FrameSequenceContext *Hcf::buildContext(AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    auto qosContext = new QoSContext(originatorAckPolicy, originatorBlockAckProcedure, originatorBlockAckAgreementHandler, edcaf->getTxopProcedure());
    return new FrameSequenceContext(mac->getAddress(), modeSet, edcaf->getInProgressFrames(), rtsProcedure, rtsPolicy, nullptr, qosContext);
}

void Hcf::startFrameSequence(AccessCategory ac)
{
    frameSequenceHandler->startFrameSequence(new HcfFs(), buildContext(ac), this);
    emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
}

void Hcf::resumeContention()
{
    for (int i = 0; i < 4; ++i) {
        AccessCategory ac = (AccessCategory)i;
        if (hasFrameToTransmit(ac)) {
            auto edcaf = edca->getEdcaf(ac);
            if (edcaf && !edcaf->isOwning()) {
                EV_DETAIL << "Resuming contention for access category " << printAccessCategory(ac) << std::endl;
                edca->requestChannelAccess(ac, this);
            }
        }
    }
}

void Hcf::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
{
    for (auto edcaf : internallyCollidedEdcafs) {
        AccessCategory ac = edcaf->getAccessCategory();
        auto dataRecoveryProcedure = edcaf->getRecoveryProcedure();
        Packet *internallyCollidedFrame = edcaf->getInProgressFrames()->getFrameToTransmit();
        auto internallyCollidedHeader = internallyCollidedFrame->peekAtFront<Ieee80211DataOrMgmtHeader>();
        EV_INFO << printAccessCategory(ac) << " (" << internallyCollidedFrame->getName() << ")" << endl;
        bool retryLimitReached = false;
        // IEEE Std 802.11-2024, 10.23.2.4: if two EDCAFs can initiate at the
        // same slot boundary, lower-priority ACs report internal collision and
        // invoke the backoff/retry update path from 10.23.2.2 item d).
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(internallyCollidedHeader)) { // TODO QoSDataFrame
            dataRecoveryProcedure->dataFrameTransmissionFailed(internallyCollidedFrame, dataHeader);
            retryLimitReached = dataRecoveryProcedure->isRetryLimitReached(internallyCollidedFrame, dataHeader);
        }
        else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(internallyCollidedHeader)) {
            ASSERT(ac == AccessCategory::AC_BE);
            edca->getMgmtAndNonQoSRecoveryProcedure()->dataOrMgmtFrameTransmissionFailed(internallyCollidedFrame, mgmtHeader, edca->getEdcaf(AccessCategory::AC_BE)->getStationRetryCounters());
            retryLimitReached = edca->getMgmtAndNonQoSRecoveryProcedure()->isRetryLimitReached(internallyCollidedFrame, mgmtHeader);
        }
        else // TODO + NonQoSDataFrame
            throw cRuntimeError("Unknown frame");
        if (retryLimitReached) {
            EV_DETAIL << "The frame has reached its retry limit. Dropping it" << std::endl;
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(internallyCollidedHeader))
                dataRecoveryProcedure->retryLimitReached(internallyCollidedFrame, dataHeader);
            else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(internallyCollidedHeader))
                edca->getMgmtAndNonQoSRecoveryProcedure()->retryLimitReached(internallyCollidedFrame, mgmtHeader);
            else ; // TODO + NonQoSDataFrame
            edcaf->getInProgressFrames()->dropFrame(internallyCollidedFrame);
            edcaf->getAckHandler()->dropFrame(internallyCollidedHeader);
            PacketDropDetails details;
            details.setReason(RETRY_LIMIT_REACHED);
            details.setLimit(-1); // TODO
            emit(packetDroppedSignal, internallyCollidedFrame, &details);
            emit(linkBrokenSignal, internallyCollidedFrame);
            if (hasFrameToTransmit(ac))
                edcaf->requestChannel(this);
        }
        else
            edcaf->requestChannel(this);
    }
}

/*
 * TODO  If a PHY-RXSTART.indication primitive does not occur during the ACKTimeout interval,
 * the STA concludes that the transmission of the MPDU has failed, and this STA shall invoke its
 * backoff procedure **upon expiration of the ACKTimeout interval**.
 */

void Hcf::frameSequenceFinished()
{
    Enter_Method("frameSequenceFinished");
    emit(IFrameSequenceHandler::frameSequenceFinishedSignal, frameSequenceHandler->getContext());
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        bool startContention = hasFrameToTransmit(); // TODO outstanding frame
        // IEEE Std 802.11-2024, 10.23.2.8 and 10.23.2.9: when the TXOP ends,
        // the holder releases medium control; further traffic must contend
        // unless it is sent as another permitted sequence inside the same TXOP.
        edcaf->releaseChannel(this);
        mac->sendDownPendingRadioConfigMsg(); // TODO review
        edcaf->getTxopProcedure()->endTxop();
        if (startContention)
            edcaf->requestChannel(this);
    }
    else if (hcca->isOwning()) {
        hcca->releaseChannel(this);
        mac->sendDownPendingRadioConfigMsg(); // TODO review
        throw cRuntimeError("Hcca is unimplemented!");
    }
    else
        throw cRuntimeError("Frame sequence finished but channel owner not found!");
}

void Hcf::recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (dynamicPtrCast<const Ieee80211MpduSubframeHeader>(packet->peekAtFront()) != nullptr) {
        // IEEE Std 802.11-2024, 9.7.1 and Table 9-659: an A-MPDU is a
        // sequence of MPDU delimiters and MPDUs; each nonfinal subframe is
        // padded to a 4-octet boundary.
        constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
                Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
        auto receiveInd = packet->findTag<Ieee80211MpduReceiveInd>();
        unsigned int resultIndex = 0;
        while (packet->getDataLength() > b(0) &&
                dynamicPtrCast<const Ieee80211MpduSubframeHeader>(packet->peekAtFront()) != nullptr) {
            auto delimiter = packet->popAtFront<Ieee80211MpduSubframeHeader>(
                    B(-1), parsingFlags);
            auto status = delimiter->isIncorrect() ? MPDU_DELIMITER_ERROR : MPDU_SUCCESS;
            if (receiveInd != nullptr && resultIndex < receiveInd->getResultsArraySize())
                status = receiveInd->getResults(resultIndex).status;
            auto mpduLength = B(delimiter->getLength());
            if (mpduLength > packet->getDataLength())
                status = MPDU_PAYLOAD_ERROR;
            else {
                auto mpdu = new Packet(packet->getName());
                mpdu->copyTags(*packet);
                mpdu->removeTagIfPresent<Ieee80211MpduReceiveInd>();
                mpdu->insertAtBack(packet->popAtFront(mpduLength, parsingFlags));
                auto mpduHeader = dynamicPtrCast<const Ieee80211MacHeader>(
                        mpdu->peekAtFront(b(-1), parsingFlags));
                if (status == MPDU_SUCCESS && mpduHeader != nullptr)
                    recipientProcessReceivedFrame(mpdu, mpduHeader);
                else
                    delete mpdu;
            }
            resultIndex++;
            int padding = (4 - (B(4) + mpduLength).get<B>() % 4) % 4;
            if (padding > 0 && packet->getDataLength() >= B(padding))
                packet->popAtFront(B(padding), parsingFlags);
        }
        delete packet;
        return;
    }

    EV_INFO << "Processing received frame " << packet->getName() << " as recipient.\n";
    emit(packetReceivedFromPeerSignal, packet);

    bool wasHeMu = false;
    int myAllocationIndex = -1;
    if (auto heMuRxTag = packet->findTag<Ieee80211HeMuRxTag>()) {
        wasHeMu = true;
        auto mib = mac->getMib();
        auto myStaId = mib == nullptr ? computeHeMuStaId(mac->getAddress()) :
                mib->bssStationData.associationId;
        for (unsigned int i = 0; i < heMuRxTag->getAllocationsArraySize(); ++i) {
            if (myStaId > 0 && heMuRxTag->getAllocations(i).staId == myStaId) {
                myAllocationIndex = i;
                break;
            }
        }
    }

    if (wasHeMu && myAllocationIndex != -1) {
        bool responseSent = false;
        bool isBlockAckPolicyData = false;
        auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header);
        if (dataOrMgmtHeader != nullptr) {
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(dataOrMgmtHeader)) {
                isBlockAckPolicyData = dataHeader->getType() == ST_DATA_WITH_QOS && dataHeader->getAckPolicy() == BLOCK_ACK;
                if (isBlockAckPolicyData && recipientBlockAckAgreementHandler) {
                    auto agreement = recipientBlockAckAgreementHandler->getAgreement(dataHeader->getTid(), dataHeader->getTransmitterAddress());
                    if (agreement)
                        recipientBlockAckAgreementHandler->qosFrameReceived(dataHeader, this);
                    if (agreement)
                        EV_INFO << "HeHcf: STA waits for an explicit BAR or MU-BAR Trigger before BlockAck: index = "
                                << myAllocationIndex << endl;
                }
            }

            if (!responseSent && !isBlockAckPolicyData && recipientAckPolicy->isAckNeeded(dataOrMgmtHeader)) {
                auto ack = makeShared<Ieee80211AckFrame>();
                ack->setReceiverAddress(dataOrMgmtHeader->getTransmitterAddress());

                auto dummyReq = makeShared<Ieee80211BasicBlockAckReq>();
                auto responseMode = rateSelection->computeResponseBlockAckFrameMode(packet, dummyReq);
                simtime_t blockAckDuration = responseMode->getDuration(LENGTH_BASIC_BLOCKACK);
                simtime_t ifs = (myAllocationIndex + 1) * modeSet->getSifsTime() + myAllocationIndex * blockAckDuration;

                auto ackMode = rateSelection->computeResponseAckFrameMode(packet, dataOrMgmtHeader);
                simtime_t ackDuration = ackMode->getDuration(LENGTH_ACK);

                simtime_t duration = dataOrMgmtHeader->getDurationField() - ifs - ackDuration;
                if (duration < 0) duration = 0;
                ack->setDurationField(duration);

                auto ackPacket = new Packet("WlanAck", ack);
                ackPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
                setFrameMode(ackPacket, ack, ackMode);

                EV_INFO << "HeHcf: STA sequential Ack scheduled: index = " << myAllocationIndex
                        << ", delay = " << ifs << ", duration = " << ackDuration << endl;

                tx->transmitFrame(ackPacket, ack, ifs, this);
                delete ackPacket;
            }
        }
    }
    else {
        if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header))
            // IEEE Std 802.11-2024, 10.3.2.11: the recipient performs the
            // immediate Ack/BlockAck procedure for received frames that require
            // it before higher MAC delivery decisions complete.
            recipientAckProcedure->processReceivedFrame(packet, dataOrMgmtHeader, check_and_cast<IRecipientAckPolicy *>(recipientAckPolicy), this);
    }

    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
        if (dataHeader->getType() == ST_DATA_WITH_QOS && recipientBlockAckAgreementHandler && !wasHeMu)
            // IEEE Std 802.11-2024, 10.25.6: HT-immediate block ack state is
            // updated for QoS Data frames covered by a block ack agreement.
            recipientBlockAckAgreementHandler->qosFrameReceived(dataHeader, this);
        sendUp(recipientDataService->dataFrameReceived(packet, dataHeader, recipientBlockAckAgreementHandler));
    }
    else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(header)) {
        sendUp(recipientDataService->managementFrameReceived(packet, mgmtHeader));
        recipientProcessReceivedManagementFrame(mgmtHeader);
    }
    else { // TODO else if (auto ctrlFrame = dynamic_cast<Ieee80211ControlFrame*>(frame))
        sendUp(recipientDataService->controlFrameReceived(packet, header, recipientBlockAckAgreementHandler));
        recipientProcessReceivedControlFrame(packet, header);
        delete packet;
    }
}

void Hcf::recipientProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (auto psPoll = dynamicPtrCast<const Ieee80211PsPollFrame>(header)) {
        if (auto twtManager = mac->getTwtManager())
            twtManager->notifyPeerAwake(psPoll->getTransmitterAddress());
        return;
    }
    if (auto rtsFrame = dynamicPtrCast<const Ieee80211RtsFrame>(header))
        // IEEE Std 802.11-2024, 10.3.2.9: an RTS addressed to this STA can
        // solicit a CTS after SIFS when NAV/CTS policy allows the response.
        ctsProcedure->processReceivedRts(packet, rtsFrame, ctsPolicy, this);
    else if (auto blockAckRequest = dynamicPtrCast<const Ieee80211BasicBlockAckReq>(header)) {
        // IEEE Std 802.11-2024, 10.25.3 and 10.25.6: a BlockAckReq solicits
        // an immediate BlockAck for frames in the negotiated block ack window.
        if (recipientBlockAckProcedure)
            recipientBlockAckProcedure->processReceivedBlockAckReq(packet, blockAckRequest, recipientAckPolicy, recipientBlockAckAgreementHandler, this);
    }
    else if (dynamicPtrCast<const Ieee80211AckFrame>(header))
        EV_WARN << "ACK frame received after timeout, ignoring it.\n"; // drop it, it is an ACK frame that is received after the ACKTimeout
    else
        throw cRuntimeError("Unknown control frame");
}

void Hcf::recipientProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header)
{
    if (recipientBlockAckAgreementHandler && originatorBlockAckAgreementHandler) {
        if (auto addbaRequest = dynamicPtrCast<const Ieee80211AddbaRequest>(header)) {
            // IEEE Std 802.11-2024, 10.25.2: ADDBA Request/Response establish
            // or modify the block ack agreement parameters for a TID.
            recipientBlockAckAgreementHandler->processReceivedAddbaRequest(addbaRequest, recipientBlockAckAgreementPolicy, this);
            auto agreement = recipientBlockAckAgreementHandler->getAgreement(addbaRequest->getTid(), addbaRequest->getTransmitterAddress());
            emit(blockAckAgreementAddedSignal, agreement);
        }
        else if (auto addbaResp = dynamicPtrCast<const Ieee80211AddbaResponse>(header)) {
            originatorBlockAckAgreementHandler->processReceivedAddbaResp(addbaResp, originatorBlockAckAgreementPolicy, this);
            auto agreement = originatorBlockAckAgreementHandler->getAgreement(addbaResp->getTransmitterAddress(), addbaResp->getTid());
            emit(blockAckAgreementAddedSignal, agreement);
            resumeContention();
        }
        else if (auto delba = dynamicPtrCast<const Ieee80211Delba>(header)) {
            if (delba->getInitiator()) {
                auto agreement = recipientBlockAckAgreementHandler->getAgreement(delba->getTid(), delba->getReceiverAddress());
                emit(blockAckAgreementDeletedSignal, agreement);
                recipientBlockAckAgreementHandler->processReceivedDelba(delba, recipientBlockAckAgreementPolicy);
            }
            else {
                auto agreement = originatorBlockAckAgreementHandler->getAgreement(delba->getReceiverAddress(), delba->getTid());
                emit(blockAckAgreementDeletedSignal, agreement);
                originatorBlockAckAgreementHandler->processReceivedDelba(delba, originatorBlockAckAgreementPolicy);
            }
        }
        else
            ; // Beacon, etc
    }
    else
        ; // Optional modules
}

void Hcf::transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    Enter_Method("transmissionComplete");
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        frameSequenceHandler->transmissionComplete();
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented!");
    else
        recipientProcessTransmittedControlResponseFrame(packet, header);
}

void Hcf::originatorProcessRtsProtectionFailed(Packet *packet)
{
    Enter_Method("originatorProcessRtsProtectionFailed");
    auto protectedHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        EV_INFO << "RTS frame transmission failed\n";
        bool retryLimitReached = false;
        // IEEE Std 802.11-2024, 10.3.2.9 and 10.23.2.2: a failed RTS/CTS
        // exchange invokes the EDCAF retry/backoff update for the protected
        // frame exchange.
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(protectedHeader)) {
            edcaf->getRecoveryProcedure()->rtsFrameTransmissionFailed(dataHeader);
            retryLimitReached = edcaf->getRecoveryProcedure()->isRtsFrameRetryLimitReached(packet, dataHeader);
        }
        else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(protectedHeader)) {
            edca->getMgmtAndNonQoSRecoveryProcedure()->rtsFrameTransmissionFailed(mgmtHeader, edcaf->getStationRetryCounters());
            retryLimitReached = edca->getMgmtAndNonQoSRecoveryProcedure()->isRtsFrameRetryLimitReached(packet, mgmtHeader);
        }
        else
            throw cRuntimeError("Unknown frame"); // TODO QoSDataFrame, NonQoSDataFrame
        if (retryLimitReached) {
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(protectedHeader))
                edcaf->getRecoveryProcedure()->retryLimitReached(packet, dataHeader);
            else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(protectedHeader))
                edca->getMgmtAndNonQoSRecoveryProcedure()->retryLimitReached(packet, mgmtHeader);
            else ; // TODO nonqos data
            edcaf->getInProgressFrames()->dropFrame(packet);
            edcaf->getAckHandler()->dropFrame(protectedHeader);
            EV_INFO << "Dropping RTS/CTS protected frame " << packet->getName() << ", because retry limit is reached.\n";
            PacketDropDetails details;
            details.setReason(RETRY_LIMIT_REACHED);
            details.setLimit(-1); // TODO
            emit(packetDroppedSignal, packet, &details);
            emit(linkBrokenSignal, packet);
        }
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

void Hcf::originatorProcessTransmittedFrame(Packet *packet)
{
    Enter_Method("originatorProcessTransmittedFrame");
    EV_INFO << "Processing transmitted frame " << packet->getName() << " as originator in frame sequence.\n";
    auto transmittedHeader = packet->peekAtFront<Ieee80211MacHeader>();
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        edcaf->emit(packetSentToPeerSignal, packet);
        AccessCategory ac = edcaf->getAccessCategory();
        if (transmittedHeader->getReceiverAddress().isMulticast()) {
            // IEEE Std 802.11-2024, 10.3.2.11: group addressed frames do not
            // solicit Ack/BlockAck and are considered acknowledged immediately.
            edcaf->getRecoveryProcedure()->multicastFrameTransmitted();
            if (auto transmittedDataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(transmittedHeader))
                edcaf->getInProgressFrames()->dropFrame(packet);
        }
        else if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(transmittedHeader))
            originatorProcessTransmittedDataFrame(packet, dataHeader, ac);
        else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(transmittedHeader))
            originatorProcessTransmittedManagementFrame(mgmtHeader, ac);
        else // TODO Ieee80211ControlFrame
            originatorProcessTransmittedControlFrame(transmittedHeader, ac);
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented");
    else
        throw cRuntimeError("Frame transmitted but channel owner not found");
}

void Hcf::originatorProcessTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    edcaf->getAckHandler()->processTransmittedDataOrMgmtFrame(dataHeader);
    if (originatorBlockAckAgreementHandler)
        originatorBlockAckAgreementHandler->processTransmittedDataFrame(packet, dataHeader, originatorBlockAckAgreementPolicy, this);
    if (dataHeader->getAckPolicy() == NO_ACK)
        edcaf->getInProgressFrames()->dropFrame(packet);
}

void Hcf::originatorProcessTransmittedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& mgmtHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    if (originatorAckPolicy->isAckNeeded(mgmtHeader))
        edcaf->getAckHandler()->processTransmittedDataOrMgmtFrame(mgmtHeader);
    if (auto addbaReq = dynamicPtrCast<const Ieee80211AddbaRequest>(mgmtHeader)) {
        if (originatorBlockAckAgreementHandler)
            originatorBlockAckAgreementHandler->processTransmittedAddbaReq(addbaReq);
    }
    else if (auto addbaResp = dynamicPtrCast<const Ieee80211AddbaResponse>(mgmtHeader))
        recipientBlockAckAgreementHandler->processTransmittedAddbaResp(addbaResp, this);
    else if (auto delba = dynamicPtrCast<const Ieee80211Delba>(mgmtHeader)) {
        if (delba->getInitiator())
            originatorBlockAckAgreementHandler->processTransmittedDelba(delba);
        else
            recipientBlockAckAgreementHandler->processTransmittedDelba(delba);
    }
    else ; // TODO other mgmt frames if needed
}

void Hcf::originatorProcessTransmittedControlFrame(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    if (auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(controlHeader))
        // IEEE Std 802.11-2024, 10.3.2.11 and 10.25.3: BlockAckReq frames
        // require immediate acknowledgment by a BlockAck response.
        edcaf->getAckHandler()->processTransmittedBlockAckReq(blockAckReq);
    else if (auto rtsFrame = dynamicPtrCast<const Ieee80211RtsFrame>(controlHeader))
        rtsProcedure->processTransmittedRts(rtsFrame);
    else
        throw cRuntimeError("Unknown control frame");
}

void Hcf::originatorProcessFailedFrame(Packet *failedPacket)
{
    Enter_Method("originatorProcessFailedFrame");
    auto failedHeader = failedPacket->peekAtFront<Ieee80211MacHeader>();
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        bool retryLimitReached = false;
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader)) {
            ASSERT(dataHeader->getAckPolicy() == NORMAL_ACK || dataHeader->getAckPolicy() == BLOCK_ACK);
            EV_INFO << "Data frame transmission failed\n";
            // IEEE Std 802.11-2024, 10.23.2.2 and 10.23.2.12: EDCA updates
            // retry/CW state for failed initial MPDU exchanges; QoS retry
            // counters are tracked per TID and sequence number.
            edcaf->getRecoveryProcedure()->dataFrameTransmissionFailed(failedPacket, dataHeader);
            retryLimitReached = edcaf->getRecoveryProcedure()->isRetryLimitReached(failedPacket, dataHeader);
            if (dataAndMgmtRateControl) {
                int retryCount = edcaf->getRecoveryProcedure()->getRetryCount(failedPacket, dataHeader);
                dataAndMgmtRateControl->frameTransmitted(failedPacket, retryCount, false, retryLimitReached);
            }
            edcaf->getAckHandler()->processFailedFrame(dataHeader);
        }
        else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(failedHeader)) { // TODO + NonQoS frames
            EV_INFO << "Management frame transmission failed\n";
            // IEEE Std 802.11-2024, 10.3.4.4: Management frames use the
            // non-QoS SRC/LRC retry-limit rules even when carried by EDCA.
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
        else
            throw cRuntimeError("Unknown frame"); // TODO qos, nonqos
        if (retryLimitReached) {
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader))
                edcaf->getRecoveryProcedure()->retryLimitReached(failedPacket, dataHeader);
            else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(failedHeader))
                edca->getMgmtAndNonQoSRecoveryProcedure()->retryLimitReached(failedPacket, mgmtHeader);
            edcaf->getInProgressFrames()->dropFrame(failedPacket);
            edcaf->getAckHandler()->dropFrame(dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(failedHeader));
            EV_INFO << "Dropping frame " << failedPacket->getName() << ", because retry limit is reached.\n";
            PacketDropDetails details;
            details.setReason(RETRY_LIMIT_REACHED);
            details.setLimit(-1); // TODO
            emit(packetDroppedSignal, failedPacket, &details);
            emit(linkBrokenSignal, failedPacket);
        }
        else {
            EV_INFO << "Retrying frame " << failedPacket->getName() << ".\n";
            auto h = failedPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
            h->setRetry(true);
            failedPacket->insertAtFront(h);
        }
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

void Hcf::originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket)
{
    if (receivedPacket == nullptr)
        return;
    auto receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
    if (isHeMuContainerPacket(lastTransmittedPacket) && !dynamicPtrCast<const Ieee80211BasicBlockAck>(receivedHeader))
        return;
    Enter_Method("originatorProcessReceivedFrame");
    EV_INFO << "Processing received frame " << receivedPacket->getName() << " as originator in frame sequence.\n";
    emit(packetReceivedFromPeerSignal, receivedPacket);
    auto lastTransmittedHeader = lastTransmittedPacket->peekAtFront<Ieee80211MacHeader>();
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(receivedHeader))
            originatorProcessReceivedDataFrame(dataHeader, lastTransmittedHeader, ac);
        else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(receivedHeader))
            originatorProcessReceivedManagementFrame(mgmtHeader, lastTransmittedHeader, ac);
        else
            originatorProcessReceivedControlFrame(receivedPacket, receivedHeader, lastTransmittedPacket, lastTransmittedHeader, ac);
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

void Hcf::originatorProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac)
{
    if (auto addbaResp = dynamicPtrCast<const Ieee80211AddbaResponse>(header)) {
        if (originatorBlockAckAgreementHandler) {
            originatorBlockAckAgreementHandler->processReceivedAddbaResp(addbaResp, originatorBlockAckAgreementPolicy, this);
            auto agreement = originatorBlockAckAgreementHandler->getAgreement(addbaResp->getTransmitterAddress(), addbaResp->getTid());
            emit(blockAckAgreementAddedSignal, agreement);
            resumeContention();
        }
    }
    else {
        throw cRuntimeError("Unknown management frame");
    }
}

void Hcf::originatorProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, Packet *lastTransmittedPacket, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    if (auto ackFrame = dynamicPtrCast<const Ieee80211AckFrame>(header)) {
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(lastTransmittedHeader)) {
            if (dataAndMgmtRateControl) {
                int retryCount;
                if (dataHeader->getRetry())
                    retryCount = edcaf->getRecoveryProcedure()->getRetryCount(lastTransmittedPacket, dataHeader);
                else
                    retryCount = 0;
                dataAndMgmtRateControl->frameTransmitted(lastTransmittedPacket, retryCount, true, false);
            }
            // IEEE Std 802.11-2024, 10.3.2.11 and 10.23.2.2: a valid Ack
            // addressed to the originator completes the MPDU exchange and
            // resets the EDCAF retry/CW state for this AC.
            edcaf->getRecoveryProcedure()->ackFrameReceived(lastTransmittedPacket, dataHeader);
        }
        else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(lastTransmittedHeader)) {
            if (dataAndMgmtRateControl) {
                int retryCount = edca->getMgmtAndNonQoSRecoveryProcedure()->getRetryCount(lastTransmittedPacket, mgmtHeader);
                dataAndMgmtRateControl->frameTransmitted(lastTransmittedPacket, retryCount, true, false);
            }
            // IEEE Std 802.11-2024, 10.3.4.4: successful Management frame
            // acknowledgment resets the applicable non-QoS retry counters.
            edca->getMgmtAndNonQoSRecoveryProcedure()->ackFrameReceived(lastTransmittedPacket, mgmtHeader, edcaf->getStationRetryCounters());
        }
        else
            throw cRuntimeError("Unknown frame"); // TODO qos, nonqos frame
        auto lastTransmittedDataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(lastTransmittedHeader);
        edcaf->getAckHandler()->processReceivedAck(ackFrame, lastTransmittedDataOrMgmtHeader);
        edcaf->getInProgressFrames()->dropFrame(lastTransmittedPacket);
        edcaf->getAckHandler()->dropFrame(lastTransmittedDataOrMgmtHeader);
    }
    else if (auto blockAck = dynamicPtrCast<const Ieee80211BasicBlockAck>(header)) {
        EV_INFO << "BasicBlockAck has arrived" << std::endl;
        // IEEE Std 802.11-2024, 10.25.3 and 10.25.6.8: a received BlockAck
        // advances originator state and drops acknowledged MPDUs from the
        // retransmission window.
        edcaf->getRecoveryProcedure()->blockAckFrameReceived();
        auto ackedSeqAndFragNums = edcaf->getAckHandler()->processReceivedBlockAck(blockAck);
        if (originatorBlockAckAgreementHandler)
            originatorBlockAckAgreementHandler->processReceivedBlockAck(blockAck, this);
        EV_TRACE << "It has acknowledged the following frames:" << std::endl;
        for (auto it : ackedSeqAndFragNums)
            EV_TRACE << "   sequenceNumber = " << it.second.second.getSequenceNumber() << ", fragmentNumber = " << (int)it.second.second.getFragmentNumber() << std::endl;
        edcaf->getInProgressFrames()->dropFrames(ackedSeqAndFragNums);
        edcaf->getAckHandler()->dropFrames(ackedSeqAndFragNums);
    }
    else if (dynamicPtrCast<const Ieee80211RtsFrame>(header))
        ; // void
    else if (dynamicPtrCast<const Ieee80211CtsFrame>(header))
        edcaf->getRecoveryProcedure()->ctsFrameReceived();
    else if (header->getType() == ST_DATA_WITH_QOS)
        ; // void
    else if (dynamicPtrCast<const Ieee80211BasicBlockAckReq>(header))
        ; // void
    else
        throw cRuntimeError("Unknown control frame");
}

void Hcf::originatorProcessReceivedDataFrame(const Ptr<const Ieee80211DataHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac)
{
    throw cRuntimeError("Unknown data frame");
}

bool Hcf::hasFrameToTransmit(AccessCategory ac)
{
    if (auto twtManager = mac->getTwtManager(); twtManager != nullptr && !twtManager->isStationAwake())
        return false;
    auto edcaf = edca->getEdcaf(ac);
    if (edcaf)
        return !edcaf->getPendingQueue()->isEmpty() || edcaf->getInProgressFrames()->hasInProgressFrames();
    else
        throw cRuntimeError("Hcca is unimplemented");
}

bool Hcf::hasFrameToTransmit()
{
    if (auto twtManager = mac->getTwtManager(); twtManager != nullptr && !twtManager->isStationAwake())
        return false;
    auto edcaf = edca->getChannelOwner();
    if (edcaf)
        return !edcaf->getPendingQueue()->isEmpty() || edcaf->getInProgressFrames()->hasInProgressFrames();
    else
        throw cRuntimeError("Hcca is unimplemented");
}

void Hcf::sendUp(const std::vector<Packet *>& completeFrames)
{
    for (auto frame : completeFrames)
        mac->sendUpFrame(frame);
}

void Hcf::transmitFrame(Packet *packet, simtime_t ifs)
{
    Enter_Method("transmitFrame");
    auto channelOwner = edca->getChannelOwner();
    if (channelOwner) {
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        auto txop = channelOwner->getTxopProcedure();
        if (auto dataFrame = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            OriginatorBlockAckAgreement *agreement = nullptr;
            if (originatorBlockAckAgreementHandler)
                agreement = originatorBlockAckAgreementHandler->getAgreement(dataFrame->getReceiverAddress(), dataFrame->getTid());
            auto ackPolicy = originatorAckPolicy->computeAckPolicy(packet, dataFrame, agreement);
            auto dataHeader = packet->removeAtFront<Ieee80211DataHeader>();
            // IEEE Std 802.11-2024, 10.3.2.11 and 10.25: QoS Data frames
            // select Normal Ack, No Ack or Block Ack policy based on the frame
            // and any active block ack agreement.
            dataHeader->setAckPolicy(ackPolicy);
            packet->insertAtFront(dataHeader);
        }
        auto mode = rateSelection->computeMode(packet, header, txop);
        setFrameMode(packet, header, mode);
        emit(IRateSelection::datarateSelectedSignal, mode->getDataMode()->getNetBitrate().get<bps>(), packet);
        EV_DEBUG << "Datarate for " << packet->getName() << " is set to " << mode->getDataMode()->getNetBitrate() << ".\n";
        if (txop->getProtectionMechanism() == TxopProcedure::ProtectionMechanism::SINGLE_PROTECTION) {
            if (!isHeMuContainerPacket(packet) &&
                    !dynamicPtrCast<const Ieee80211TriggerFrame>(header) &&
                    !dynamicPtrCast<const Ieee80211MultiStaBlockAck>(header)) {
                auto pendingPacket = channelOwner->getInProgressFrames()->getPendingFrameFor(packet);
                const auto& pendingHeader = pendingPacket == nullptr ? nullptr : pendingPacket->peekAtFront<Ieee80211DataOrMgmtHeader>();
                auto duration = singleProtectionMechanism->computeDurationField(packet, header, pendingPacket, pendingHeader, txop, recipientAckPolicy);
                auto header = packet->removeAtFront<Ieee80211MacHeader>();
                // IEEE Std 802.11-2024, 10.3.1, 10.3.2.6 and 10.23.2.8:
                // Duration protects the remaining exchange or TXOP portion.
                header->setDurationField(duration);
                EV_DEBUG << "Duration for " << packet->getName() << " is set to " << duration << " s.\n";
                packet->insertAtFront(header);
            }
        }
        else if (txop->getProtectionMechanism() == TxopProcedure::ProtectionMechanism::MULTIPLE_PROTECTION)
            throw cRuntimeError("Multiple protection is unsupported");
        else
            throw cRuntimeError("Undefined protection mechanism");
        tx->transmitFrame(packet, packet->peekAtFront<Ieee80211MacHeader>(), ifs, this);
    }
    else
        throw cRuntimeError("Hcca is unimplemented");
}

void Hcf::transmitControlResponseFrame(Packet *responsePacket, const Ptr<const Ieee80211MacHeader>& responseHeader, Packet *receivedPacket, const Ptr<const Ieee80211MacHeader>& receivedHeader)
{
    Enter_Method("transmitControlResponseFrame");
    responsePacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    const IIeee80211Mode *responseMode = nullptr;
    if (auto rtsFrame = dynamicPtrCast<const Ieee80211RtsFrame>(receivedHeader))
        responseMode = rateSelection->computeResponseCtsFrameMode(receivedPacket, rtsFrame);
    else if (auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(receivedHeader))
        responseMode = rateSelection->computeResponseBlockAckFrameMode(receivedPacket, blockAckReq);
    else if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(receivedHeader))
        responseMode = rateSelection->computeResponseAckFrameMode(receivedPacket, dataOrMgmtHeader);
    else
        throw cRuntimeError("Unknown received frame type");
    // IEEE Std 802.11-2024, 10.3.2.9, 10.3.2.11 and 10.25.3: CTS, Ack and
    // BlockAck immediate responses are transmitted after SIFS.
    setFrameMode(responsePacket, responseHeader, responseMode);
    emit(IRateSelection::datarateSelectedSignal, responseMode->getDataMode()->getNetBitrate().get<bps>(), responsePacket);
    EV_DEBUG << "Datarate for " << responsePacket->getName() << " is set to " << responseMode->getDataMode()->getNetBitrate() << ".\n";
    tx->transmitFrame(responsePacket, responseHeader, modeSet->getSifsTime(), this);
    delete responsePacket;
}

void Hcf::recipientProcessTransmittedControlResponseFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    emit(packetSentToPeerSignal, packet);
    if (auto ctsFrame = dynamicPtrCast<const Ieee80211CtsFrame>(header))
        ctsProcedure->processTransmittedCts(ctsFrame);
    else if (auto blockAck = dynamicPtrCast<const Ieee80211BlockAck>(header)) {
        if (recipientBlockAckProcedure)
            recipientBlockAckProcedure->processTransmittedBlockAck(blockAck);
    }
    else if (auto ackFrame = dynamicPtrCast<const Ieee80211AckFrame>(header))
        recipientAckProcedure->processTransmittedAck(ackFrame);
    else
        throw cRuntimeError("Unknown control response frame");
    resumeContention();
}

void Hcf::processMgmtFrame(Packet *mgmtPacket, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader)
{
    Enter_Method("processMgmtFrame");
    mgmtPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    processUpperFrame(mgmtPacket, mgmtHeader);
}

void Hcf::setFrameMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, const IIeee80211Mode *mode) const
{
    ASSERT(mode != nullptr);
    packet->addTagIfAbsent<Ieee80211ModeReq>()->setMode(mode);
}

bool Hcf::isReceptionInProgress()
{
    return rx->isReceptionInProgress();
}

bool Hcf::isForUs(const Ptr<const Ieee80211MacHeader>& header) const
{
    // IEEE Std 802.11-2024, 9.2.4.3.1 and Table 9-60: Address 1 is RA/DA for
    // nonmesh reception; suppress local loopback of multicast frames sent by us.
    return header->getReceiverAddress() == mac->getAddress() || (header->getReceiverAddress().isMulticast() && !isSentByUs(header));
}

bool Hcf::isSentByUs(const Ptr<const Ieee80211MacHeader>& header) const
{
    // FIXME
    // Check the roles of the Addr3 field when aggregation is applied
    // IEEE Std 802.11-2024, Table 9-60: Address 3 can carry SA/BSSID/DA
    // depending on To DS/From DS, so this shortcut is incomplete.
    if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header))
        return dataOrMgmtHeader->getAddress3() == mac->getAddress();
    else
        return false;
}

void Hcf::corruptedFrameReceived()
{
    Enter_Method("corruptedFrameReceived");
    if (frameSequenceHandler->isSequenceRunning() && !startRxTimer->isScheduled()) {
        frameSequenceHandler->handleStartRxTimeout();
    }
    else
        EV_DEBUG << "Ignoring received corrupt frame.\n";
}

Hcf::~Hcf()
{
    cancelAndDelete(startRxTimer);
    cancelAndDelete(inactivityTimer);
    delete recipientAckProcedure;
    delete ctsProcedure;
    delete rtsProcedure;
    delete originatorBlockAckAgreementHandler;
    delete recipientBlockAckAgreementHandler;
    delete originatorBlockAckProcedure;
    delete recipientBlockAckProcedure;
    delete frameSequenceHandler;
}

queueing::IPacketQueue *Hcf::getPerStaQueue(const MacAddress& staAddr, AccessCategory ac)
{
    return edca->getEdcaf(ac)->getPendingQueue();
}

StationQueueBank *Hcf::createStationQueueBank(const MacAddress& staAddr)
{
    EV_WARN << "Per-STA queue banks are not supported by this HCF\n";
    return nullptr;
}

void Hcf::destroyStationQueueBank(const MacAddress& staAddr)
{
    EV_WARN << "Per-STA queue banks are not supported by this HCF\n";
}

StationQueueBank *Hcf::getStationQueueBank(const MacAddress& staAddr) const
{
    return nullptr;
}

} // namespace ieee80211
} // namespace inet
