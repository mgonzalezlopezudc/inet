//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_AARFRATECONTROL_H
#define __INET_AARFRATECONTROL_H

#include <map>

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HtRateControl.h"
#include "inet/linklayer/ieee80211/mac/ratecontrol/RateControlBase.h"

namespace inet {
namespace ieee80211 {

/**
 * Implements the ARF and AARF rate control algorithms.
 */
class INET_API AarfRateControl : public RateControlBase, public IIeee80211HtRateControl
{
  protected:
    simtime_t timer = SIMTIME_ZERO;
    simtime_t interval = SIMTIME_ZERO;
    bool probing = false;
    int increaseThreshold = -1;
    int maxIncreaseThreshold = -1;
    int decreaseThreshold = -1;
    int maxNss = -1;
    double factor = -1;
    int numberOfConsSuccTransmissions = 0;

    struct HtPeerState {
        uint8_t nextRequestSequenceIdentifier = 0;
        bool outstandingRequest = false;
        uint8_t outstandingSequenceIdentifier = 7;
        Hz outstandingChannelWidth = Hz(0);
        int outstandingNss = 1;
        bool pendingFeedback = false;
        uint8_t pendingFeedbackSequenceIdentifier = 7;
        uint8_t pendingFeedbackValue = 127;
        simtime_t lastRequestTime = -1;
    };
    bool enableHtMcsFeedback = false;
    simtime_t htMcsRequestInterval = SimTime(100, SIMTIME_MS);
    HtCsiCache htCsiCache;
    std::map<MacAddress, HtPeerState> htPeers;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;

    virtual void multiplyIncreaseThreshold(double factor);
    virtual void resetIncreaseThreshdold();
    virtual void resetTimer();
    virtual void increaseRateIfTimerIsExpired();

    const physicallayer::IIeee80211Mode *increaseRateIfPossible(const physicallayer::IIeee80211Mode *currentMode);
    const physicallayer::IIeee80211Mode *decreaseRateIfPossible(const physicallayer::IIeee80211Mode *currentMode);
    HtPeerState& getHtPeerState(const MacAddress& peer);

  public:
    virtual const physicallayer::IIeee80211Mode *getRate() override;
    virtual void frameTransmitted(Packet *frame, int retryCount, bool isSuccessful, bool isGivenUp) override;
    virtual void frameReceived(Packet *frame) override;
    virtual void processReceivedHtMcsRequest(const MacAddress& peer, uint8_t msi,
            const physicallayer::IIeee80211Mode *receivedMode) override;
    virtual void processReceivedHtMcsFeedback(const MacAddress& peer, uint8_t mfsi,
            uint8_t mfb) override;
    virtual bool getPendingHtMcsControl(const MacAddress& peer,
            bool mcsRequestAllowed, bool mcsFeedbackAllowed,
            Ieee80211HtMcsControl& control) override;
    virtual HtCsiCache& getHtCsiCache() override { return htCsiCache; }
    virtual void invalidateHtPeer(const MacAddress& peer) override;
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
