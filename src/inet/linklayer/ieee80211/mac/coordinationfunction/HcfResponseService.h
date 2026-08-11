//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFRESPONSESERVICE_H
#define __INET_HCFRESPONSESERVICE_H

#include <functional>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceRxTimeoutState.h"

namespace inet {

class Packet;

namespace ieee80211 {

class IReceiveStep;
class IFrameSequenceStep;

/**
 * Coordinates response classification and receive-timeout bookkeeping for HCF.
 *
 * The service owns only the deferred-timeout token. The exchange coordinator
 * owns the OMNeT++ timer messages and active exchange identity, while
 * FrameSequenceHandler continues to own the frame-sequence implementation.
 */
class INET_API HcfResponseService
{
  public:
    enum class ResponseClassification {
        PROCESS,
        IGNORE,
    };

    struct Actions {
        std::function<bool()> isSequenceRunning;
        std::function<bool()> isReceptionInProgress;
        std::function<const IFrameSequenceStep *()> getCurrentStep;
        std::function<bool()> isStartRxTimerScheduled;
        std::function<void()> handleStartRxTimeout;
        std::function<void(Packet *)> processResponse;
        std::function<void()> cancelStartRxTimer;
    };

  protected:
    FrameSequenceRxTimeoutState timeoutState;

    static void checkActions(const Actions& actions);

  public:
    ResponseClassification classifyResponse(bool sequenceRunning,
            bool addressedToUs, bool responseTimerScheduled) const;
    void responseTimerScheduled();
    void handleStartRxTimeout(const Actions& actions);
    void processResponse(Packet *packet, const Actions& actions);
    void processResponseAndCancelStartRxTimerIfCompleted(Packet *packet,
            IReceiveStep *receiveStep, const Actions& actions);
    void processResponseAndCancelStartRxTimerIfReceiveStepCompletes(Packet *packet,
            IReceiveStep *receiveStep, const Actions& actions);
    void handleDeferredStartRxTimeout(const Actions& actions);
    bool handleCorruptedFrame(const Actions& actions);
    void clearTimerState();

    bool hasDeferredStartRxTimeout() const { return timeoutState.hasDeferredStep(); }
};

} // namespace ieee80211
} // namespace inet

#endif
