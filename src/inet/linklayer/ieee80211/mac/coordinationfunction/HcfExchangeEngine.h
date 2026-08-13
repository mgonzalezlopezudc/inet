//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFEXCHANGEENGINE_H
#define __INET_HCFEXCHANGEENGINE_H

#include <functional>
#include <memory>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfResponseService.h"

namespace inet {
namespace ieee80211 {

class FrameSequenceContext;
class Hcf;
class IFrameSequence;
class IReceiveStep;

/**
 * Owns the active HCF frame exchange and adapts its generic frame-sequence
 * callbacks to call-scoped HCF actions.
 */
class INET_API HcfExchangeEngine : private IFrameSequenceHandler::ICallback
{
    friend class Hcf;

  public:
    struct Actions {
        std::function<bool()> isReceptionInProgress;
        std::function<void(Packet *, simtime_t)> transmitFrame;
        std::function<void(Packet *)> originatorProcessRtsProtectionFailed;
        std::function<void(Packet *)> originatorProcessTransmittedFrame;
        std::function<void(Packet *, Packet *)> originatorProcessReceivedFrame;
        std::function<void(Packet *)> originatorProcessFailedFrame;
        std::function<void(const FrameSequenceContext *)> frameSequenceStarted;
        std::function<void(const FrameSequenceContext *)> frameSequenceFinished;
        std::function<bool()> hasTransactionalOwner;
        std::function<void(HcfTransactionIdentity, HcfExchangeAbortReason)> exchangeTerminated;
        std::function<void()> resumeContention;
        std::function<void(Packet *)> discardResponse;
        std::function<void()> inactivityTimeout;
        std::function<void(cMessage *)> cancelTimer;
        std::function<void(simtime_t, cMessage *)> scheduleTimer;
        std::function<void(simtime_t, cMessage *)> rescheduleTimer;
    };

  private:
    class ActionScope {
      private:
        HcfExchangeEngine& engine;
        const Actions *previousActions;

      public:
        ActionScope(HcfExchangeEngine& engine, const Actions& actions);
        ~ActionScope();
    };

    std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler;
    HcfExchangeCoordinator exchangeCoordinator;
    HcfResponseService responseService;
    const Actions *activeActions = nullptr;
    uint64_t nextTransactionToken = 1;
    uint64_t nextTransactionGeneration = 1;
    HcfTransactionIdentity activeTransactionIdentity;
    uint64_t startRxTimerGeneration = 0;
    uint64_t deferredTimeoutGeneration = 0;
    HcfExchangeAbortReason terminalAbortReason = HcfExchangeAbortReason::NONE;

    const Actions& getActiveActions() const;
    static void checkActions(const Actions& actions);
    HcfResponseService::Actions makeResponseActions(const Actions& actions);
    void beginTransactionIfNeeded();
    void clearTransaction();
    void notifyTransactionTerminated(const Actions& actions, HcfExchangeAbortReason reason);
    const IFrameSequence *getFrameSequenceForLegacyAdapter() const;
    IFrameSequenceHandler::ICallback *getFrameSequenceCallbackForLegacyAdapter() { return this; }

    // IFrameSequenceHandler::ICallback
    virtual void transmitFrame(Packet *packet, simtime_t ifs) override;
    virtual void originatorProcessRtsProtectionFailed(Packet *packet) override;
    virtual void originatorProcessTransmittedFrame(Packet *packet) override;
    virtual void originatorProcessReceivedFrame(Packet *packet, Packet *lastTransmittedPacket) override;
    virtual void originatorProcessFailedFrame(Packet *packet) override;
    virtual void frameSequenceAborted() override;
    virtual void frameSequenceFinished() override;
    virtual void scheduleStartRxTimer(simtime_t timeout) override;

  public:
    explicit HcfExchangeEngine(std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler);

    void initializeTimers();
    void replaceFrameSequenceHandler(std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler);
    void cancelTimers(const Actions& actions);
    bool handleMessage(cMessage *message, const Actions& actions);

    void channelAccessRequested();
    void channelGranted();
    void beginPreparation();
    void preparationCompletedWithoutSequence(const Actions& actions);
    void startFrameSequence(IFrameSequence *frameSequence, FrameSequenceContext *context,
            const Actions& actions);
    void transmissionComplete(const Actions& actions);
    void processResponse(Packet *packet, const Actions& actions);
    HcfResponseService::ResponseClassification processResponseAccordingToPolicy(
            Packet *packet, bool addressedToUs, IReceiveStep *receiveStep,
            const Actions& actions);
    void processResponseAndCancelStartRxTimerIfCompleted(Packet *packet,
            IReceiveStep *receiveStep, const Actions& actions);
    void handleDeferredStartRxTimeout(const Actions& actions);
    bool handleCorruptedFrame(const Actions& actions);
    void scheduleInactivityTimer(simtime_t timeout, const Actions& actions);

    bool isSequenceRunning() const;
    bool canRequestChannelAccess() const
        { return exchangeCoordinator.getState() == HcfExchangeCoordinator::State::IDLE ||
                exchangeCoordinator.getState() == HcfExchangeCoordinator::State::AWAITING_CHANNEL_GRANT; }
    const FrameSequenceContext *getContext() const;
    const IFrameSequenceStep *getCurrentStep() const;
    Packet *getActivePacket() const { return exchangeCoordinator.getActivePacket(); }
    HcfTransactionIdentity getActiveTransactionIdentity() const { return activeTransactionIdentity; }
    bool isActiveTransaction(HcfTransactionIdentity identity) const;
    bool hasDeferredStartRxTimeout() const { return responseService.hasDeferredStartRxTimeout(); }
    void clearTimerStateForTest() { responseService.clearTimerState(); deferredTimeoutGeneration = 0; }
    bool isStartRxTimerScheduled() const;
    cMessage *getStartRxTimerForTest() const { return exchangeCoordinator.getStartRxTimer(); }
};

} // namespace ieee80211
} // namespace inet

#endif
