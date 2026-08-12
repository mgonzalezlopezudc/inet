//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/coordinationfunction/Dcf.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211Addressing.h"

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211MgmtExchangeTag_m.h"
#include "inet/linklayer/ieee80211/mac/framesequence/DcfFs.h"
#include "inet/linklayer/ieee80211/mac/rateselection/RateSelection.h"
#include "inet/linklayer/ieee80211/mac/recipient/RecipientAckProcedure.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(Dcf);

void Dcf::notifyMgmtExchangeResult(Packet *packet,
        Ieee80211MgmtExchangeResultKind kind)
{
    if (mgmtExchangeResultHandler == nullptr || packet == nullptr)
        return;
    auto header = dynamicPtrCast<const Ieee80211MgmtHeader>(packet->peekAtFront<Ieee80211MacHeader>());
    auto tag = packet->findTag<Ieee80211MgmtExchangeTag>();
    if (header != nullptr && tag != nullptr)
        mgmtExchangeResultHandler->handleIeee80211MgmtExchangeResult(
                Ieee80211MgmtExchangeResult(tag->getTransactionId(), kind));
}

void Dcf::initialize(int stage)
{
    ModeSetListener::initialize(stage);
    if (stage == INITSTAGE_LINK_LAYER) {
        startRxTimer = new cMessage("startRxTimeout");
        mac = check_and_cast<Ieee80211Mac *>(getContainingNicModule(this)->getSubmodule("mac"));
        dataAndMgmtRateControl = dynamic_cast<IRateControl *>(getSubmodule(("rateControl")));
        tx = check_and_cast<ITx *>(getModuleByPath(par("txModule")));
        rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
        channelAccess = check_and_cast<Dcaf *>(getSubmodule("channelAccess"));
        originatorDataService = check_and_cast<IOriginatorMacDataService *>(getSubmodule(("originatorMacDataService")));
        recipientDataService = check_and_cast<IRecipientMacDataService *>(getSubmodule("recipientMacDataService"));
        recoveryProcedure = check_and_cast<NonQosRecoveryProcedure *>(getSubmodule("recoveryProcedure"));
        rateSelection = check_and_cast<IRateSelection *>(getSubmodule("rateSelection"));
        rtsProcedure = std::make_unique<RtsProcedure>();
        rtsPolicy = check_and_cast<IRtsPolicy *>(getSubmodule("rtsPolicy"));
        recipientAckProcedure = std::make_unique<RecipientAckProcedure>();
        recipientAckPolicy = check_and_cast<IRecipientAckPolicy *>(getSubmodule("recipientAckPolicy"));
        originatorAckPolicy = check_and_cast<IOriginatorAckPolicy *>(getSubmodule("originatorAckPolicy"));
        frameSequenceHandler = std::make_unique<FrameSequenceHandler>();
        ackHandler = check_and_cast<AckHandler *>(getSubmodule("ackHandler"));
        ctsProcedure = std::make_unique<CtsProcedure>();
        ctsPolicy = check_and_cast<ICtsPolicy *>(getSubmodule("ctsPolicy"));
        stationRetryCounters = std::make_unique<StationRetryCounters>();
        originatorProtectionMechanism = check_and_cast<OriginatorProtectionMechanism *>(getSubmodule("originatorProtectionMechanism"));
        WATCH_EXPR("frameSequenceInfo", frameSequenceHandler->isSequenceRunning() ? "Fs: " + frameSequenceHandler->getFrameSequence()->getHistory() : "");
    }
}

void Dcf::forEachChild(cVisitor *v)
{
    SimpleModule::forEachChild(v);
    if (frameSequenceHandler != nullptr && frameSequenceHandler->getContext() != nullptr)
        v->visit(const_cast<FrameSequenceContext *>(frameSequenceHandler->getContext()));
}

void Dcf::handleMessage(cMessage *msg)
{
    if (msg == startRxTimer) {
        if (!frameSequenceHandler->isSequenceRunning())
            return;
        else if (isReceptionInProgress())
            deferredStartRxTimeoutStep = frameSequenceHandler->getContext()->getLastStep();
        else
            frameSequenceHandler->handleStartRxTimeout();
    }
    else
        throw cRuntimeError("Unknown msg type");
}

void Dcf::handleDeferredStartRxTimeout()
{
    if (deferredStartRxTimeoutStep == nullptr || isReceptionInProgress())
        return;
    auto timeoutStep = deferredStartRxTimeoutStep;
    deferredStartRxTimeoutStep = nullptr;
    // IEEE Std 802.11-2024, 10.3.2.9 and 10.3.2.11: a reception that starts
    // before the response timeout is processed before deciding that the
    // expected response is missing. Do not apply the expired timeout to a
    // step that the received frame has already completed.
    if (frameSequenceHandler->isSequenceRunning() &&
            frameSequenceHandler->getContext()->getLastStep() == timeoutStep)
        frameSequenceHandler->handleStartRxTimeout();
}

void Dcf::channelGranted(IChannelAccess *channelAccess)
{
    Enter_Method("channelGranted");
    ASSERT(this->channelAccess == channelAccess);
    if (!frameSequenceHandler->isSequenceRunning()) {
        // IEEE Std 802.11-2024, 10.3.4.2 and 10.3.4.3: the DCF may transmit
        // after DIFS/EIFS and backoff reach zero; Dcaf owns that contention
        // procedure, and this callback starts the resulting frame exchange.
        frameSequenceHandler->startFrameSequence(new DcfFs(), buildContext(), this);
        emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
    }
}

void Dcf::processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    Enter_Method("processUpperFrame(%s)", packet->getName());
    take(packet);
    EV_INFO << "Processing upper frame: " << packet->getName() << endl;
    auto pendingQueue = channelAccess->getPendingQueue();
    // IEEE Std 802.11-2024, 10.3.1 and 10.3.4.2: queued DCF traffic enters
    // CSMA/CA contention; Dcaf applies DIFS/EIFS, random backoff and CW.
    pendingQueue->enqueuePacket(packet);
    if (!pendingQueue->isEmpty()) {
        EV_DETAIL << "Requesting channel" << endl;
        channelAccess->requestChannel(this);
    }
}

void Dcf::transmitControlResponseFrame(Packet *responsePacket, const Ptr<const Ieee80211MacHeader>& responseHeader, Packet *receivedPacket, const Ptr<const Ieee80211MacHeader>& receivedHeader)
{
    Enter_Method("transmitControlResponseFrame");
    responsePacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    const IIeee80211Mode *responseMode = nullptr;
    if (auto rtsFrame = dynamicPtrCast<const Ieee80211RtsFrame>(receivedHeader))
        responseMode = rateSelection->computeResponseCtsFrameMode(receivedPacket, rtsFrame);
    else if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(receivedHeader))
        responseMode = rateSelection->computeResponseAckFrameMode(receivedPacket, dataOrMgmtHeader);
    else
        throw cRuntimeError("Unknown received frame type");
    // IEEE Std 802.11-2024, 10.3.2.9 and 10.3.2.11: CTS/Ack responses are
    // control responses transmitted after SIFS, using the response-rate rules.
    RateSelection::setFrameMode(responsePacket, responseHeader, responseMode);
    emit(IRateSelection::datarateSelectedSignal, responseMode->getDataMode()->getNetBitrate().get<bps>(), responsePacket);
    EV_DEBUG << "Datarate for " << responsePacket->getName() << " is set to " << responseMode->getDataMode()->getNetBitrate() << ".\n";
    tx->transmitFrame(responsePacket, responseHeader, modeSet->getSifsTime(), this);
    delete responsePacket;
}

void Dcf::processMgmtFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader)
{
    throw cRuntimeError("Unknown management frame");
}

void Dcf::recipientProcessTransmittedControlResponseFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    emit(packetSentToPeerSignal, packet);
    if (auto ctsFrame = dynamicPtrCast<const Ieee80211CtsFrame>(header))
        ctsProcedure->processTransmittedCts(ctsFrame);
    else if (auto ackFrame = dynamicPtrCast<const Ieee80211AckFrame>(header))
        recipientAckProcedure->processTransmittedAck(ackFrame);
    else
        throw cRuntimeError("Unknown control response frame");
}

void Dcf::scheduleStartRxTimer(simtime_t timeout)
{
    Enter_Method("scheduleStartRxTimer");
    deferredStartRxTimeoutStep = nullptr;
    scheduleAfter(timeout, startRxTimer);
}

void Dcf::processLowerFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    Enter_Method("processLowerFrame(%s)", packet->getName());
    take(packet);
    EV_INFO << "Processing lower frame: " << packet->getName() << endl;
    if (frameSequenceHandler->isSequenceRunning()) {
        // IEEE Std 802.11-2024, 10.3.2.9 and 10.3.2.11: while waiting for
        // CTS/Ack, a matching response continues the exchange; an unexpected
        // frame or timeout is handled as a failed transmission by the sequence.
        // TODO always call processResponses
        if ((!isForUs(header) && !startRxTimer->isScheduled()) || isForUs(header)) {
            auto receiveStep = dynamic_cast<IReceiveStep *>(frameSequenceHandler->getContext()->getLastStep());
            bool completesOnReception = receiveStep == nullptr || receiveStep->completesOnReception();
            frameSequenceHandler->processResponse(packet);
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
        if (!isForUs(header))
            cancelEvent(startRxTimer);
    }
    else if (isForUs(header))
        recipientProcessReceivedFrame(packet, header);
    else {
        EV_INFO << "This frame is not for us" << std::endl;
        PacketDropDetails details;
        details.setReason(NOT_ADDRESSED_TO_US);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
    }
    handleDeferredStartRxTimeout();
}

void Dcf::transmitFrame(Packet *packet, simtime_t ifs)
{
    Enter_Method("transmitFrame");
    const auto& header = packet->peekAtFront<Ieee80211MacHeader>();
    auto modeReq = packet->findTag<Ieee80211ModeReq>();
    auto mode = modeReq == nullptr ? rateSelection->computeMode(packet, header) : modeReq->getMode();
    RateSelection::setFrameMode(packet, header, mode);
    emit(IRateSelection::datarateSelectedSignal, mode->getDataMode()->getNetBitrate().get<bps>(), packet);
    EV_DEBUG << "Datarate for " << packet->getName() << " is set to " << mode->getDataMode()->getNetBitrate() << ".\n";
    auto pendingPacket = channelAccess->getInProgressFrames()->getPendingFrameFor(packet);
    auto duration = originatorProtectionMechanism->computeDurationField(packet, header, pendingPacket, pendingPacket == nullptr ? nullptr : pendingPacket->peekAtFront<Ieee80211DataOrMgmtHeader>());
    const auto& updatedHeader = packet->removeAtFront<Ieee80211MacHeader>();
    // IEEE Std 802.11-2024, 10.3.1 and 10.3.2.6: RTS/CTS and individually
    // addressed frames announce the remaining protected exchange via Duration.
    updatedHeader->setDurationField(duration);
    EV_DEBUG << "Duration for " << packet->getName() << " is set to " << duration << " s.\n";
    packet->insertAtFront(updatedHeader);
    tx->transmitFrame(packet, packet->peekAtFront<Ieee80211MacHeader>(), ifs, this);
}

/*
 * TODO  If a PHY-RXSTART.indication primitive does not occur during the ACKTimeout interval,
 * the STA concludes that the transmission of the MPDU has failed, and this STA shall invoke its
 * backoff procedure **upon expiration of the ACKTimeout interval**.
 */

void Dcf::frameSequenceFinished()
{
    Enter_Method("frameSequenceFinished");
    deferredStartRxTimeoutStep = nullptr;
    emit(IFrameSequenceHandler::frameSequenceFinishedSignal, frameSequenceHandler->getContext());
    // IEEE Std 802.11-2024, 10.3.4.3: after a frame exchange the DCF releases
    // the current access and, if traffic remains, performs another contention.
    channelAccess->releaseChannel(this);
    if (hasFrameToTransmit())
        channelAccess->requestChannel(this);
    mac->sendDownPendingRadioConfigMsg(); // TODO review
}

bool Dcf::isReceptionInProgress()
{
    return rx->isReceptionInProgress();
}

void Dcf::recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    EV_INFO << "Processing received frame " << packet->getName() << " as recipient.\n";
    emit(packetReceivedFromPeerSignal, packet);
    // IEEE Std 802.11-2024, 10.3.2.11: individually addressed Data and
    // Management frames that require immediate acknowledgment are acknowledged
    // even if the payload is later discarded by higher MAC service logic.
    if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header))
        recipientAckProcedure->processReceivedFrame(packet, dataOrMgmtHeader, recipientAckPolicy, this);
    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header))
        sendUp(recipientDataService->dataFrameReceived(packet, dataHeader));
    else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(header))
        sendUp(recipientDataService->managementFrameReceived(packet, mgmtHeader));
    else { // TODO else if (auto ctrlFrame = dynamic_cast<Ieee80211ControlFrame*>(frame))
        sendUp(recipientDataService->controlFrameReceived(packet, header));
        recipientProcessReceivedControlFrame(packet, header);
        delete packet;
    }
}

void Dcf::sendUp(const std::vector<Packet *>& completeFrames)
{
    for (auto frame : completeFrames)
        mac->sendUpFrame(frame);
}

void Dcf::recipientProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    // IEEE Std 802.11-2024, 10.3.2.9: a non-VHT/non-S1G recipient of an RTS
    // addressed to it responds with CTS after SIFS when the CTS policy permits.
    if (auto rtsFrame = dynamicPtrCast<const Ieee80211RtsFrame>(header))
        ctsProcedure->processReceivedRts(packet, rtsFrame, ctsPolicy, this);
    else
        throw cRuntimeError("Unknown control frame");
}

FrameSequenceContext *Dcf::buildContext()
{
    auto nonQoSContext = new NonQoSContext(originatorAckPolicy);
    return new FrameSequenceContext(mac->getAddress(), modeSet, channelAccess->getInProgressFrames(), rtsProcedure.get(), rtsPolicy, nonQoSContext, nullptr);
}

void Dcf::transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    Enter_Method("transmissionComplete");
    if (frameSequenceHandler->isSequenceRunning()) {
        frameSequenceHandler->transmissionComplete();
    }
    else
        recipientProcessTransmittedControlResponseFrame(packet, header);
}

bool Dcf::hasFrameToTransmit()
{
    return !channelAccess->getPendingQueue()->isEmpty() || channelAccess->getInProgressFrames()->hasInProgressFrames();
}

void Dcf::originatorProcessRtsProtectionFailed(Packet *packet)
{
    Enter_Method("originatorProcessRtsProtectionFailed");
    EV_INFO << "RTS frame transmission failed\n";
    auto protectedHeader = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
    // IEEE Std 802.11-2024, 10.3.2.9 and 10.3.4.4: missing or invalid CTS
    // after CTSTimeout fails the RTS exchange, increments SRC/SSRC and may hit
    // dot11ShortRetryLimit for the protected MSDU/MMPDU.
    recoveryProcedure->rtsFrameTransmissionFailed(protectedHeader, stationRetryCounters.get());
    EV_INFO << "For the current frame exchange, we have CW = " << channelAccess->getCw() << " SRC = " << recoveryProcedure->getShortRetryCount(packet, protectedHeader) << " LRC = " << recoveryProcedure->getLongRetryCount(packet, protectedHeader) << " SSRC = " << stationRetryCounters->getStationShortRetryCount() << " and SLRC = " << stationRetryCounters->getStationLongRetryCount() << std::endl;
    if (recoveryProcedure->isRtsFrameRetryLimitReached(packet, protectedHeader)) {
        recoveryProcedure->retryLimitReached(packet, protectedHeader);
        channelAccess->getInProgressFrames()->dropFrame(packet);
        ackHandler->dropFrame(protectedHeader);
        EV_INFO << "Dropping RTS/CTS protected frame " << packet->getName() << ", because retry limit is reached.\n";
        PacketDropDetails details;
        details.setReason(RETRY_LIMIT_REACHED);
        details.setLimit(recoveryProcedure->getShortRetryLimit());
        emit(packetDroppedSignal, packet, &details);
        notifyMgmtExchangeResult(packet,
                Ieee80211MgmtExchangeResultKind::RETRY_LIMIT_REACHED);
        emit(linkBrokenSignal, packet);
    }
}

void Dcf::originatorProcessTransmittedFrame(Packet *packet)
{
    Enter_Method("originatorProcessTransmittedFrame");
    EV_INFO << "Processing transmitted frame " << packet->getName() << " as originator in frame sequence.\n";
    emit(packetSentToPeerSignal, packet);
    auto transmittedHeader = packet->peekAtFront<Ieee80211MacHeader>();
    if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(transmittedHeader)) {
        EV_INFO << "For the current frame exchange, we have CW = " << channelAccess->getCw() << " SRC = " << recoveryProcedure->getShortRetryCount(packet, dataOrMgmtHeader) << " LRC = " << recoveryProcedure->getLongRetryCount(packet, dataOrMgmtHeader) << " SSRC = " << stationRetryCounters->getStationShortRetryCount() << " and SLRC = " << stationRetryCounters->getStationLongRetryCount() << std::endl;
        if (originatorAckPolicy->isAckNeeded(dataOrMgmtHeader)) {
            // IEEE Std 802.11-2024, 10.3.2.11: an originator that sends an
            // MPDU requiring immediate Ack enters the AckTimeout wait state.
            ackHandler->processTransmittedDataOrMgmtFrame(dataOrMgmtHeader);
        }
        else if (dataOrMgmtHeader->getReceiverAddress().isMulticast()) {
            // IEEE Std 802.11-2024, 10.3.2.11: group addressed frames do not
            // solicit Ack and are implicitly considered acknowledged.
        recoveryProcedure->multicastFrameTransmitted(stationRetryCounters.get());
            channelAccess->getInProgressFrames()->dropFrame(packet);
        }
    }
    else if (auto rtsFrame = dynamicPtrCast<const Ieee80211RtsFrame>(transmittedHeader)) {
        auto protectedFrame = channelAccess->getInProgressFrames()->getFrameToTransmit(); // KLUDGE
        auto protectedHeader = protectedFrame->peekAtFront<Ieee80211DataOrMgmtHeader>();
        EV_INFO << "For the current frame exchange, we have CW = " << channelAccess->getCw() << " SRC = " << recoveryProcedure->getShortRetryCount(protectedFrame, protectedHeader) << " LRC = " << recoveryProcedure->getLongRetryCount(protectedFrame, protectedHeader) << " SSRC = " << stationRetryCounters->getStationShortRetryCount() << " and SLRC = " << stationRetryCounters->getStationLongRetryCount() << std::endl;
        rtsProcedure->processTransmittedRts(rtsFrame);
    }
}

void Dcf::originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket)
{
    Enter_Method("originatorProcessReceivedFrame");
    EV_INFO << "Processing received frame " << receivedPacket->getName() << " as originator in frame sequence.\n";
    emit(packetReceivedFromPeerSignal, receivedPacket);
    auto receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
    auto lastTransmittedHeader = lastTransmittedPacket->peekAtFront<Ieee80211MacHeader>();
    if (receivedHeader->getType() == ST_ACK) {
        auto lastTransmittedDataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(lastTransmittedHeader);
        if (dataAndMgmtRateControl) {
            int retryCount = lastTransmittedHeader->getRetry() ? recoveryProcedure->getRetryCount(lastTransmittedPacket, lastTransmittedDataOrMgmtHeader) : 0;
            dataAndMgmtRateControl->frameTransmitted(lastTransmittedPacket, retryCount, true, false);
        }
        // IEEE Std 802.11-2024, 10.3.2.11 and 10.3.4.4: a valid Ack addressed
        // to the originator completes the MPDU exchange and resets the relevant
        // retry counters/CW in the recovery procedure.
    recoveryProcedure->ackFrameReceived(lastTransmittedPacket, lastTransmittedDataOrMgmtHeader, stationRetryCounters.get());
        ackHandler->processReceivedAck(dynamicPtrCast<const Ieee80211AckFrame>(receivedHeader), lastTransmittedDataOrMgmtHeader);
        notifyMgmtExchangeResult(lastTransmittedPacket,
                Ieee80211MgmtExchangeResultKind::ACKNOWLEDGED);
        channelAccess->getInProgressFrames()->dropFrame(lastTransmittedPacket);
        ackHandler->dropFrame(lastTransmittedDataOrMgmtHeader);
    }
    else if (receivedHeader->getType() == ST_RTS)
        ; // void
    else if (receivedHeader->getType() == ST_CTS)
    recoveryProcedure->ctsFrameReceived(stationRetryCounters.get());
    else
        throw cRuntimeError("Unknown frame type");
}

void Dcf::originatorProcessFailedFrame(Packet *failedPacket)
{
    Enter_Method("originatorProcessFailedFrame");
    EV_INFO << "Data/Mgmt frame transmission failed\n";
    const auto& failedHeader = failedPacket->peekAtFront<Ieee80211DataOrMgmtHeader>();
    ASSERT(failedHeader->getType() != ST_DATA_WITH_QOS);
    ASSERT(ackHandler->getAckStatus(failedHeader) == AckHandler::Status::WAITING_FOR_ACK || ackHandler->getAckStatus(failedHeader) == AckHandler::Status::NO_ACK_REQUIRED);
    // IEEE Std 802.11-2024, 10.3.2.11 and 10.3.4.4: failure of the Ack
    // procedure increments SRC/LRC according to the MPDU length vs.
    // dot11RTSThreshold; retransmissions carry Retry=1 until a retry limit or
    // transmit lifetime causes discard.
    recoveryProcedure->dataOrMgmtFrameTransmissionFailed(failedPacket, failedHeader, stationRetryCounters.get());
    bool retryLimitReached = recoveryProcedure->isRetryLimitReached(failedPacket, failedHeader);
    if (dataAndMgmtRateControl) {
        int retryCount = recoveryProcedure->getRetryCount(failedPacket, failedHeader);
        dataAndMgmtRateControl->frameTransmitted(failedPacket, retryCount, false, retryLimitReached);
    }
    ackHandler->processFailedFrame(failedHeader);
    if (retryLimitReached) {
        recoveryProcedure->retryLimitReached(failedPacket, failedHeader);
        channelAccess->getInProgressFrames()->dropFrame(failedPacket);
        ackHandler->dropFrame(failedHeader);
        EV_INFO << "Dropping frame " << failedPacket->getName() << ", because retry limit is reached.\n";
        PacketDropDetails details;
        details.setReason(RETRY_LIMIT_REACHED);
        details.setLimit(-1); // TODO
        emit(packetDroppedSignal, failedPacket, &details);
        notifyMgmtExchangeResult(failedPacket,
                Ieee80211MgmtExchangeResultKind::RETRY_LIMIT_REACHED);
        emit(linkBrokenSignal, failedPacket);
    }
    else {
        EV_INFO << "Retrying frame " << failedPacket->getName() << ".\n";
        auto h = failedPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
        h->setRetry(true);
        failedPacket->insertAtFront(h);
    }
}

bool Dcf::isForUs(const Ptr<const Ieee80211MacHeader>& header) const
{
    auto roles = interpretIeee80211AddressRoles(header);
    return roles.receiverAddress == mac->getAddress() || (roles.receiverAddress.isMulticast() && !isSentByUs(header));
}

bool Dcf::isSentByUs(const Ptr<const Ieee80211MacHeader>& header) const
{
    auto roles = interpretIeee80211AddressRoles(header);
    return roles.hasTransmitterAddress && roles.transmitterAddress == mac->getAddress();
}

void Dcf::corruptedFrameReceived()
{
    Enter_Method("corruptedFrameReceived");
    if (deferredStartRxTimeoutStep != nullptr)
        handleDeferredStartRxTimeout();
    else if (frameSequenceHandler->isSequenceRunning() && !startRxTimer->isScheduled()) {
        frameSequenceHandler->handleStartRxTimeout();
    }
    else
        EV_DEBUG << "Ignoring received corrupt frame.\n";
}

Dcf::~Dcf()
{
    cancelAndDelete(startRxTimer);
}

} // namespace ieee80211
} // namespace inet
