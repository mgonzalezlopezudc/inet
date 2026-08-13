//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeCoordinator.h"

#include <initializer_list>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"

namespace inet {
namespace ieee80211 {

HcfExchangeCoordinator::~HcfExchangeCoordinator()
{
    delete startRxTimer;
    delete inactivityTimer;
}

void HcfExchangeCoordinator::initializeTimers()
{
    if (startRxTimer == nullptr)
        startRxTimer = new cMessage("startRxTimeout");
    if (inactivityTimer == nullptr)
        inactivityTimer = new cMessage("blockAckInactivityTimer");
}

void HcfExchangeCoordinator::cancelTimers(
        const std::function<void(cMessage *)>& cancelTimer)
{
    if (startRxTimer != nullptr)
        cancelTimer(startRxTimer);
    if (inactivityTimer != nullptr)
        cancelTimer(inactivityTimer);
}

void HcfExchangeCoordinator::requireState(const char *operation, std::initializer_list<State> allowed) const
{
    for (auto candidate : allowed)
        if (state == candidate)
            return;
    throw cRuntimeError("Invalid HCF exchange transition '%s' from state %d",
            operation, static_cast<int>(state));
}

void HcfExchangeCoordinator::channelAccessRequested()
{
    requireState("channelAccessRequested", {State::IDLE, State::AWAITING_CHANNEL_GRANT});
    state = State::AWAITING_CHANNEL_GRANT;
}

void HcfExchangeCoordinator::channelGranted()
{
    requireState("channelGranted", {State::IDLE, State::AWAITING_CHANNEL_GRANT});
    state = State::PREPARING;
}

void HcfExchangeCoordinator::beginPreparation()
{
    requireState("beginPreparation", {State::IDLE, State::AWAITING_CHANNEL_GRANT, State::PREPARING});
    state = State::PREPARING;
}

void HcfExchangeCoordinator::preparationCompletedWithoutSequence()
{
    requireState("preparationCompletedWithoutSequence", {State::PREPARING});
    state = State::COMPLETING;
    reset();
}

void HcfExchangeCoordinator::beginTransmission(Packet *packet)
{
    requireState("beginTransmission", {State::IDLE, State::PREPARING,
            State::TRANSMITTING, State::AWAITING_RESPONSE,
            State::RETRYING_OR_RECOVERING});
    if (state == State::TRANSMITTING && packet == nullptr)
        throw cRuntimeError("Repeated HCF transmission callback must identify its packet");
    if (packet != nullptr)
        activePacket = packet;
    expectedResponseStep = nullptr;
    state = State::TRANSMITTING;
}

void HcfExchangeCoordinator::awaitResponse(const IFrameSequenceStep *responseStep)
{
    requireState("awaitResponse", {State::PREPARING, State::TRANSMITTING,
            State::AWAITING_RESPONSE});
    expectedResponseStep = responseStep;
    state = State::AWAITING_RESPONSE;
}

bool HcfExchangeCoordinator::beginRetryOrRecovery(Packet *packet)
{
    if (state == State::IDLE || state == State::COMPLETING ||
            state == State::ABORTING || state == State::RETRYING_OR_RECOVERING ||
            !isActivePacket(packet))
        return false;
    requireState("beginRetryOrRecovery", {State::AWAITING_RESPONSE,
            State::TRANSMITTING});
    state = State::RETRYING_OR_RECOVERING;
    return true;
}

bool HcfExchangeCoordinator::isActivePacket(const Packet *packet) const
{
    return packet != nullptr && activePacket == packet;
}

bool HcfExchangeCoordinator::isExpectedResponseStep(
        const IFrameSequenceStep *responseStep) const
{
    return responseStep != nullptr && expectedResponseStep == responseStep;
}

bool HcfExchangeCoordinator::complete()
{
    if (state == State::IDLE || state == State::COMPLETING)
        return false;
    requireState("complete", {State::PREPARING, State::TRANSMITTING,
            State::AWAITING_RESPONSE, State::RETRYING_OR_RECOVERING,
            State::ABORTING});
    state = State::COMPLETING;
    return true;
}

bool HcfExchangeCoordinator::abort()
{
    if (state == State::IDLE || state == State::COMPLETING ||
            state == State::ABORTING)
        return false;
    requireState("abort", {State::PREPARING, State::TRANSMITTING,
            State::AWAITING_RESPONSE, State::RETRYING_OR_RECOVERING});
    state = State::ABORTING;
    return true;
}

bool HcfExchangeCoordinator::reset()
{
    if (state == State::IDLE)
        return false;
    requireState("reset", {State::COMPLETING});
    state = State::IDLE;
    activePacket = nullptr;
    expectedResponseStep = nullptr;
    return true;
}

} // namespace ieee80211
} // namespace inet
