//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFEXCHANGECOORDINATOR_H
#define __INET_HCFEXCHANGECOORDINATOR_H

#include <functional>
#include <initializer_list>

#include "inet/common/INETDefs.h"

namespace inet {

class Packet;

namespace ieee80211 {

class IFrameSequenceStep;

/**
 * Explicit lifecycle state for an HCF frame exchange.
 *
 * FrameSequenceHandler remains the operational owner of the frame-sequence
 * implementation and EDCA remains the owner of queue/recovery state. This
 * object owns the exchange timer objects and the active packet/expected-step
 * identity used to coordinate those collaborators.
 */
class INET_API HcfExchangeCoordinator
{
  public:
    enum class State {
        IDLE,
        AWAITING_CHANNEL_GRANT,
        PREPARING,
        TRANSMITTING,
        AWAITING_RESPONSE,
        RETRYING_OR_RECOVERING,
        COMPLETING,
        ABORTING,
    };

  private:
    State state = State::IDLE;
    cMessage *startRxTimer = nullptr;
    cMessage *inactivityTimer = nullptr;
    Packet *activePacket = nullptr;
    const IFrameSequenceStep *expectedResponseStep = nullptr;

    void requireState(const char *operation, std::initializer_list<State> allowed) const;

  public:
    ~HcfExchangeCoordinator();

    State getState() const { return state; }

    void initializeTimers();
    cMessage *getStartRxTimer() const { return startRxTimer; }
    cMessage *getInactivityTimer() const { return inactivityTimer; }
    void cancelTimers(const std::function<void(cMessage *)>& cancelTimer);

    Packet *getActivePacket() const { return activePacket; }
    const IFrameSequenceStep *getExpectedResponseStep() const { return expectedResponseStep; }
    void validateActivePacket(Packet *packet) const;

    void channelAccessRequested();
    void channelGranted();
    void beginPreparation();
    void beginTransmission(Packet *packet = nullptr);
    void awaitResponse(const IFrameSequenceStep *responseStep = nullptr);
    void beginRetryOrRecovery(Packet *packet = nullptr);
    void complete();
    void abort();
    void reset();
};

} // namespace ieee80211
} // namespace inet

#endif
