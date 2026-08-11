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

void HcfExchangeCoordinator::beginTransmission(Packet *packet)
{
    requireState("beginTransmission", {State::IDLE, State::PREPARING,
            State::TRANSMITTING, State::AWAITING_RESPONSE,
            State::RETRYING_OR_RECOVERING});
    if (packet != nullptr)
        activePacket = packet;
    state = State::TRANSMITTING;
}

void HcfExchangeCoordinator::awaitResponse(const IFrameSequenceStep *responseStep)
{
    requireState("awaitResponse", {State::PREPARING, State::TRANSMITTING,
            State::AWAITING_RESPONSE});
    expectedResponseStep = responseStep;
    state = State::AWAITING_RESPONSE;
}

void HcfExchangeCoordinator::beginRetryOrRecovery(Packet *packet)
{
    requireState("beginRetryOrRecovery", {State::AWAITING_RESPONSE,
            State::TRANSMITTING, State::RETRYING_OR_RECOVERING});
    if (packet != nullptr) {
        validateActivePacket(packet);
        activePacket = packet;
    }
    state = State::RETRYING_OR_RECOVERING;
}

void HcfExchangeCoordinator::validateActivePacket(Packet *packet) const
{
    if (packet == nullptr)
        throw cRuntimeError("HCF exchange callback packet must not be null");
    if (activePacket != nullptr && activePacket != packet)
        throw cRuntimeError("HCF exchange callback packet does not match the active packet");
}

void HcfExchangeCoordinator::complete()
{
    requireState("complete", {State::PREPARING, State::TRANSMITTING,
            State::AWAITING_RESPONSE, State::RETRYING_OR_RECOVERING});
    state = State::COMPLETING;
}

void HcfExchangeCoordinator::abort()
{
    requireState("abort", {State::PREPARING, State::TRANSMITTING,
            State::AWAITING_RESPONSE, State::RETRYING_OR_RECOVERING,
            State::ABORTING});
    state = State::ABORTING;
}

void HcfExchangeCoordinator::reset()
{
    state = State::IDLE;
    activePacket = nullptr;
    expectedResponseStep = nullptr;
}

} // namespace ieee80211
} // namespace inet
