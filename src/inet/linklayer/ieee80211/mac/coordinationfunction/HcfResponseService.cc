//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfResponseService.h"

#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"

namespace inet {
namespace ieee80211 {

void HcfResponseService::checkActions(const Actions& actions)
{
    if (!actions.isSequenceRunning || !actions.isReceptionInProgress ||
            !actions.getCurrentStep || !actions.isStartRxTimerScheduled ||
            !actions.handleStartRxTimeout || !actions.processResponse ||
            !actions.discardResponse || !actions.cancelStartRxTimer)
        throw cRuntimeError("Incomplete HCF response service actions");
}

bool HcfResponseService::applyStartRxTimeout(const Actions& actions)
{
    if (timeoutHandled)
        return false;
    timeoutHandled = true;
    actions.handleStartRxTimeout();
    return true;
}

void HcfResponseService::cancelStartRxTimer(const Actions& actions)
{
    if (!actions.isStartRxTimerScheduled())
        return;
    timeoutHandled = true;
    timeoutState.clear();
    actions.cancelStartRxTimer();
}

HcfResponseService::ResponseClassification HcfResponseService::classifyResponse(
        bool sequenceRunning, bool addressedToUs, bool responseTimerScheduled) const
{
    if (!sequenceRunning || (!addressedToUs && responseTimerScheduled))
        return ResponseClassification::IGNORE;
    return ResponseClassification::PROCESS;
}

void HcfResponseService::responseTimerScheduled()
{
    timeoutState.clear();
    timeoutHandled = false;
}

void HcfResponseService::handleStartRxTimeout(const Actions& actions)
{
    checkActions(actions);
    if (!actions.isSequenceRunning() || timeoutHandled)
        return;
    if (actions.isReceptionInProgress())
        timeoutState.deferCurrentStep(actions.getCurrentStep());
    else
        applyStartRxTimeout(actions);
}

void HcfResponseService::processResponse(Packet *packet, const Actions& actions)
{
    checkActions(actions);
    actions.processResponse(packet);
}

HcfResponseService::ResponseClassification
HcfResponseService::processResponseAccordingToPolicy(Packet *packet,
        bool addressedToUs, IReceiveStep *receiveStep, const Actions& actions)
{
    checkActions(actions);
    auto classification = classifyResponse(actions.isSequenceRunning(),
            addressedToUs, actions.isStartRxTimerScheduled());
    if (classification == ResponseClassification::PROCESS)
        processResponseAndCancelStartRxTimerIfReceiveStepCompletes(
                packet, receiveStep, actions);
    else
        actions.discardResponse(packet);
    return classification;
}

void HcfResponseService::processResponseAndCancelStartRxTimerIfCompleted(
        Packet *packet, IReceiveStep *receiveStep, const Actions& actions)
{
    checkActions(actions);
    ASSERT(receiveStep != nullptr);
    bool completesOnReception = receiveStep->completesOnReception();
    actions.processResponse(packet);
    // IEEE Std 802.11-2024, 10.3.2.9 and 10.3.2.11: once the
    // expected response completes the exchange, its timeout no longer applies.
    if (completesOnReception &&
            (!actions.isSequenceRunning() || actions.getCurrentStep() != receiveStep))
        cancelStartRxTimer(actions);
}

void HcfResponseService::processResponseAndCancelStartRxTimerIfReceiveStepCompletes(
        Packet *packet, IReceiveStep *receiveStep, const Actions& actions)
{
    checkActions(actions);
    bool completesOnReception = receiveStep == nullptr || receiveStep->completesOnReception();
    actions.processResponse(packet);
    if (completesOnReception)
        cancelStartRxTimer(actions);
}

void HcfResponseService::handleDeferredStartRxTimeout(const Actions& actions)
{
    checkActions(actions);
    if (!timeoutState.hasDeferredStep() || actions.isReceptionInProgress())
        return;
    // IEEE Std 802.11-2024, 10.3.2.9 and 10.3.2.11: a reception that starts
    // before the response timeout is processed before deciding that the
    // expected response is missing. Do not apply the expired timeout to a
    // step that the received frame has already completed.
    auto currentStep = actions.isSequenceRunning() ? actions.getCurrentStep() : nullptr;
    if (timeoutState.takeIfCurrentStep(currentStep))
        applyStartRxTimeout(actions);
}

bool HcfResponseService::handleCorruptedFrame(const Actions& actions)
{
    checkActions(actions);
    if (timeoutState.hasDeferredStep())
        handleDeferredStartRxTimeout(actions);
    else if (actions.isSequenceRunning() && !actions.isStartRxTimerScheduled()) {
        applyStartRxTimeout(actions);
        return true;
    }
    else
        return false;
    return true;
}

void HcfResponseService::clearTimerState()
{
    timeoutState.clear();
    timeoutHandled = false;
}

} // namespace ieee80211
} // namespace inet
