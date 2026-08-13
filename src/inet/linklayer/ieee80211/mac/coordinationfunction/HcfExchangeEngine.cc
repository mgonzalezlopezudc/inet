//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.h"

#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"

namespace inet {
namespace ieee80211 {

HcfExchangeEngine::ActionScope::ActionScope(HcfExchangeEngine& engine,
        const Actions& actions) :
    engine(engine), previousActions(engine.activeActions)
{
    checkActions(actions);
    engine.activeActions = &actions;
}

HcfExchangeEngine::ActionScope::~ActionScope()
{
    engine.activeActions = previousActions;
}

HcfExchangeEngine::HcfExchangeEngine(
        std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler) :
    frameSequenceHandler(std::move(frameSequenceHandler))
{
    if (this->frameSequenceHandler == nullptr)
        throw cRuntimeError("HCF exchange engine requires a frame-sequence handler");
}

void HcfExchangeEngine::checkActions(const Actions& actions)
{
    if (!actions.isReceptionInProgress || !actions.transmitFrame ||
            !actions.originatorProcessRtsProtectionFailed ||
            !actions.originatorProcessTransmittedFrame ||
            !actions.originatorProcessReceivedFrame ||
            !actions.originatorProcessFailedFrame ||
            !actions.frameSequenceStarted || !actions.frameSequenceFinished ||
            !actions.exchangeTerminated ||
            !actions.resumeContention || !actions.discardResponse ||
            !actions.inactivityTimeout || !actions.cancelTimer ||
            !actions.scheduleTimer || !actions.rescheduleTimer)
        throw cRuntimeError("Incomplete HCF exchange engine actions");
}

const HcfExchangeEngine::Actions& HcfExchangeEngine::getActiveActions() const
{
    if (activeActions == nullptr)
        throw cRuntimeError("HCF frame-sequence callback outside an exchange engine call");
    return *activeActions;
}

void HcfExchangeEngine::initializeTimers()
{
    exchangeCoordinator.initializeTimers();
}

void HcfExchangeEngine::replaceFrameSequenceHandler(
        std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler)
{
    if (frameSequenceHandler == nullptr)
        throw cRuntimeError("HCF exchange engine requires a frame-sequence handler");
    if (isSequenceRunning())
        throw cRuntimeError("Cannot replace the HCF frame-sequence handler while an exchange is running");
    this->frameSequenceHandler = std::move(frameSequenceHandler);
}

void HcfExchangeEngine::beginTransactionIfNeeded()
{
    if (activeTransactionIdentity.isValid())
        return;
    activeTransactionIdentity = HcfTransactionIdentity(
            HcfTransactionToken(nextTransactionToken++),
            HcfTransactionGeneration(nextTransactionGeneration++));
}

void HcfExchangeEngine::clearTransaction()
{
    activeTransactionIdentity = HcfTransactionIdentity();
    startRxTimerGeneration = 0;
    deferredTimeoutGeneration = 0;
    terminalAbortReason = HcfExchangeAbortReason::NONE;
}

void HcfExchangeEngine::channelAccessRequested()
{
    exchangeCoordinator.channelAccessRequested();
}

void HcfExchangeEngine::channelGranted()
{
    exchangeCoordinator.channelGranted();
    beginTransactionIfNeeded();
}

void HcfExchangeEngine::beginPreparation()
{
    exchangeCoordinator.beginPreparation();
    beginTransactionIfNeeded();
}

void HcfExchangeEngine::preparationCompletedWithoutSequence(const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    exchangeCoordinator.preparationCompletedWithoutSequence();
    responseService.clearTimerState();
    actions.cancelTimer(exchangeCoordinator.getStartRxTimer());
    actions.cancelTimer(exchangeCoordinator.getInactivityTimer());
    actions.exchangeTerminated(activeTransactionIdentity,
            HcfExchangeAbortReason::NONE);
    clearTransaction();
}

void HcfExchangeEngine::startFrameSequence(IFrameSequence *frameSequence,
        FrameSequenceContext *context, const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    beginTransactionIfNeeded();
    frameSequenceHandler->startFrameSequence(frameSequence, context, this);
    actions.frameSequenceStarted(frameSequenceHandler->getContext());
}

void HcfExchangeEngine::transmissionComplete(const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    frameSequenceHandler->transmissionComplete();
}

HcfResponseService::Actions HcfExchangeEngine::makeResponseActions(
        const Actions& actions)
{
    HcfResponseService::Actions responseActions;
    responseActions.isSequenceRunning = [this] { return isSequenceRunning(); };
    responseActions.isReceptionInProgress = actions.isReceptionInProgress;
    responseActions.getCurrentStep = [this] { return getCurrentStep(); };
    responseActions.isStartRxTimerScheduled = [this] { return isStartRxTimerScheduled(); };
    responseActions.handleStartRxTimeout = [this] { frameSequenceHandler->handleStartRxTimeout(); };
    responseActions.processResponse = [this] (Packet *packet) { frameSequenceHandler->processResponse(packet); };
    responseActions.discardResponse = actions.discardResponse;
    responseActions.cancelStartRxTimer = [this, &actions] {
        actions.cancelTimer(exchangeCoordinator.getStartRxTimer());
    };
    return responseActions;
}

void HcfExchangeEngine::processResponse(Packet *packet, const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    responseService.processResponse(packet, makeResponseActions(actions));
}

HcfResponseService::ResponseClassification
HcfExchangeEngine::processResponseAccordingToPolicy(Packet *packet,
        bool addressedToUs, IReceiveStep *receiveStep, const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    return responseService.processResponseAccordingToPolicy(packet, addressedToUs,
            receiveStep, makeResponseActions(actions));
}

void HcfExchangeEngine::processResponseAndCancelStartRxTimerIfCompleted(
        Packet *packet, IReceiveStep *receiveStep, const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    responseService.processResponseAndCancelStartRxTimerIfCompleted(packet,
            receiveStep, makeResponseActions(actions));
}

void HcfExchangeEngine::handleDeferredStartRxTimeout(const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    if (responseService.hasDeferredStartRxTimeout() &&
            (!activeTransactionIdentity.isValid() ||
             deferredTimeoutGeneration != activeTransactionIdentity.getGeneration().getValue())) {
        responseService.clearTimerState();
        deferredTimeoutGeneration = 0;
        return;
    }
    responseService.handleDeferredStartRxTimeout(makeResponseActions(actions));
    if (!responseService.hasDeferredStartRxTimeout())
        deferredTimeoutGeneration = 0;
}

bool HcfExchangeEngine::handleCorruptedFrame(const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    if (responseService.hasDeferredStartRxTimeout() &&
            (!activeTransactionIdentity.isValid() ||
             deferredTimeoutGeneration != activeTransactionIdentity.getGeneration().getValue())) {
        responseService.clearTimerState();
        deferredTimeoutGeneration = 0;
        return false;
    }
    auto handled = responseService.handleCorruptedFrame(makeResponseActions(actions));
    if (!responseService.hasDeferredStartRxTimeout())
        deferredTimeoutGeneration = 0;
    return handled;
}

bool HcfExchangeEngine::handleMessage(cMessage *message, const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    if (message == exchangeCoordinator.getStartRxTimer()) {
        if (activeTransactionIdentity.isValid() && isSequenceRunning() &&
                startRxTimerGeneration == activeTransactionIdentity.getGeneration().getValue()) {
            responseService.handleStartRxTimeout(makeResponseActions(actions));
            if (responseService.hasDeferredStartRxTimeout())
                deferredTimeoutGeneration = startRxTimerGeneration;
        }
        return true;
    }
    else if (message == exchangeCoordinator.getInactivityTimer()) {
        actions.inactivityTimeout();
        return true;
    }
    return false;
}

void HcfExchangeEngine::scheduleInactivityTimer(simtime_t timeout,
        const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    auto timer = exchangeCoordinator.getInactivityTimer();
    auto deadline = simTime() + timeout;
    if (!timer->isScheduled() || deadline < timer->getArrivalTime())
        actions.rescheduleTimer(timeout, timer);
}

void HcfExchangeEngine::cancelTimers(const Actions& actions)
{
    ActionScope actionScope(*this, actions);
    exchangeCoordinator.cancelTimers([&actions] (cMessage *timer) {
        if (timer->isScheduled())
            actions.cancelTimer(timer);
    });
    responseService.clearTimerState();
    deferredTimeoutGeneration = 0;
}

bool HcfExchangeEngine::isSequenceRunning() const
{
    return frameSequenceHandler != nullptr && frameSequenceHandler->isSequenceRunning();
}

const FrameSequenceContext *HcfExchangeEngine::getContext() const
{
    return frameSequenceHandler == nullptr ? nullptr : frameSequenceHandler->getContext();
}

const IFrameSequenceStep *HcfExchangeEngine::getCurrentStep() const
{
    if (exchangeCoordinator.getExpectedResponseStep() != nullptr)
        return exchangeCoordinator.getExpectedResponseStep();
    return isSequenceRunning() ? frameSequenceHandler->getContext()->getLastStep() : nullptr;
}

const IFrameSequence *HcfExchangeEngine::getFrameSequenceForLegacyAdapter() const
{
    return frameSequenceHandler == nullptr ? nullptr : frameSequenceHandler->getFrameSequence();
}

bool HcfExchangeEngine::isActiveTransaction(HcfTransactionIdentity identity) const
{
    return identity.isValid() && activeTransactionIdentity.isValid() &&
            identity == activeTransactionIdentity;
}

bool HcfExchangeEngine::isStartRxTimerScheduled() const
{
    auto timer = exchangeCoordinator.getStartRxTimer();
    return timer != nullptr && timer->isScheduled();
}

void HcfExchangeEngine::transmitFrame(Packet *packet, simtime_t ifs)
{
    exchangeCoordinator.beginTransmission(packet);
    getActiveActions().transmitFrame(packet, ifs);
}

void HcfExchangeEngine::originatorProcessRtsProtectionFailed(Packet *packet)
{
    getActiveActions().originatorProcessRtsProtectionFailed(packet);
}

void HcfExchangeEngine::originatorProcessTransmittedFrame(Packet *packet)
{
    getActiveActions().originatorProcessTransmittedFrame(packet);
}

void HcfExchangeEngine::originatorProcessReceivedFrame(Packet *packet,
        Packet *lastTransmittedPacket)
{
    getActiveActions().originatorProcessReceivedFrame(packet, lastTransmittedPacket);
}

void HcfExchangeEngine::originatorProcessFailedFrame(Packet *packet)
{
    if (exchangeCoordinator.beginRetryOrRecovery(packet))
        getActiveActions().originatorProcessFailedFrame(packet);
}

void HcfExchangeEngine::frameSequenceAborted()
{
    if (exchangeCoordinator.abort())
        terminalAbortReason = HcfExchangeAbortReason::RESPONSE_TIMEOUT;
}

void HcfExchangeEngine::frameSequenceFinished()
{
    if (!exchangeCoordinator.complete())
        return;
    responseService.clearTimerState();
    deferredTimeoutGeneration = 0;
    auto context = frameSequenceHandler->getContext();
    getActiveActions().exchangeTerminated(activeTransactionIdentity,
            terminalAbortReason);
    getActiveActions().frameSequenceFinished(context);
    exchangeCoordinator.reset();
    clearTransaction();
    getActiveActions().resumeContention();
}

void HcfExchangeEngine::scheduleStartRxTimer(simtime_t timeout)
{
    auto responseStep = isSequenceRunning() ?
            frameSequenceHandler->getContext()->getLastStep() : nullptr;
    exchangeCoordinator.awaitResponse(responseStep);
    responseService.responseTimerScheduled();
    deferredTimeoutGeneration = 0;
    auto& actions = getActiveActions();
    auto timer = exchangeCoordinator.getStartRxTimer();
    actions.cancelTimer(timer);
    startRxTimerGeneration = activeTransactionIdentity.getGeneration().getValue();
    actions.scheduleTimer(timeout, timer);
}

} // namespace ieee80211
} // namespace inet
