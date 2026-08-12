//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_DCF_H
#define __INET_DCF_H

#include <memory>

#include "inet/linklayer/ieee80211/mac/channelaccess/Dcaf.h"
#include "inet/linklayer/ieee80211/mac/common/ModeSetListener.h"
#include "inet/linklayer/ieee80211/mac/contract/ICoordinationFunction.h"
#include "inet/linklayer/ieee80211/mac/contract/ICtsPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/ICtsProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorMacDataService.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientAckPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientMacDataService.h"
#include "inet/linklayer/ieee80211/mac/contract/IRtsProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/Ieee80211MgmtExchangeResult.h"
#include "inet/linklayer/ieee80211/mac/contract/IRx.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/lifetime/DcfReceiveLifetimeHandler.h"
#include "inet/linklayer/ieee80211/mac/lifetime/DcfTransmitLifetimeHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/AckHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/NonQosRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/protectionmechanism/OriginatorProtectionMechanism.h"

namespace inet {
namespace ieee80211 {

class Ieee80211Mac;

/**
 * Implements IEEE 802.11 Distributed Coordination Function.
 */
class INET_API Dcf : public ICoordinationFunction, public IFrameSequenceHandler::ICallback, public IChannelAccess::ICallback, public ITx::ICallback, public IProcedureCallback, public ModeSetListener
{
  protected:
    Ieee80211Mac *mac = nullptr;
    IRateControl *dataAndMgmtRateControl = nullptr;

    cMessage *startRxTimer = nullptr;
    const IFrameSequenceStep *deferredStartRxTimeoutStep = nullptr;

    // Transmission and reception
    IRx *rx = nullptr;
    ITx *tx = nullptr;

    IRateSelection *rateSelection = nullptr;

    // Channel access method
    Dcaf *channelAccess = nullptr;

    // MAC Data Service
    IOriginatorMacDataService *originatorDataService = nullptr;
    IRecipientMacDataService *recipientDataService = nullptr;

    // MAC Procedures
    AckHandler *ackHandler = nullptr;
    IOriginatorAckPolicy *originatorAckPolicy = nullptr;
    std::unique_ptr<IRecipientAckProcedure> recipientAckProcedure;
    IRecipientAckPolicy *recipientAckPolicy = nullptr;
    std::unique_ptr<IRtsProcedure> rtsProcedure;
    IRtsPolicy *rtsPolicy = nullptr;
    std::unique_ptr<ICtsProcedure> ctsProcedure;
    ICtsPolicy *ctsPolicy = nullptr;
    NonQosRecoveryProcedure *recoveryProcedure = nullptr;

    // TODO Unimplemented
    ITransmitLifetimeHandler *transmitLifetimeHandler = nullptr;
    DcfReceiveLifetimeHandler *receiveLifetimeHandler = nullptr;

    // Protection mechanism
    OriginatorProtectionMechanism *originatorProtectionMechanism = nullptr;

    // Frame sequence handler
    std::unique_ptr<IFrameSequenceHandler> frameSequenceHandler;

    // Station counters
    std::unique_ptr<StationRetryCounters> stationRetryCounters;
    IIeee80211MgmtExchangeResultHandler *mgmtExchangeResultHandler = nullptr;

    void notifyMgmtExchangeResult(Packet *packet,
            Ieee80211MgmtExchangeResultKind kind);

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void forEachChild(cVisitor *v) override;
    virtual void handleMessage(cMessage *msg) override;
    void handleDeferredStartRxTimeout();

    virtual void sendUp(const std::vector<Packet *>& completeFrames);
    virtual bool hasFrameToTransmit();
    virtual bool isReceptionInProgress();
    virtual FrameSequenceContext *buildContext();

    virtual void recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);
    virtual void recipientProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);
    virtual void recipientProcessTransmittedControlResponseFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);

  protected:
    // IChannelAccess::ICallback
    virtual void channelGranted(IChannelAccess *channelAccess) override;

    // IFrameSequenceHandler::ICallback
    virtual void transmitFrame(Packet *packet, simtime_t ifs) override;
    virtual void originatorProcessRtsProtectionFailed(Packet *packet) override;
    virtual void originatorProcessTransmittedFrame(Packet *packet) override;
    virtual void originatorProcessReceivedFrame(Packet *packet, Packet *lastTransmittedPacket) override;
    virtual void originatorProcessFailedFrame(Packet *packet) override;
    virtual void frameSequenceFinished() override;
    virtual void scheduleStartRxTimer(simtime_t timeout) override;

    // ITx::ICallback
    virtual void transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;

    // IProcedureCallback
    virtual void transmitControlResponseFrame(Packet *responsePacket, const Ptr<const Ieee80211MacHeader>& responseHeader, Packet *receivedPacket, const Ptr<const Ieee80211MacHeader>& receivedHeader) override;
    virtual void processMgmtFrame(Packet *mgmtPacket, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader) override;

    virtual bool isSentByUs(const Ptr<const Ieee80211MacHeader>& header) const;
    virtual bool isForUs(const Ptr<const Ieee80211MacHeader>& header) const;

  public:
    virtual ~Dcf();

    void setMgmtExchangeResultHandler(IIeee80211MgmtExchangeResultHandler *handler) { mgmtExchangeResultHandler = handler; }

    // ICoordinationFunction
    virtual void processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header) override;
    virtual void processLowerFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void corruptedFrameReceived() override;
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
