//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211TWTEXAMPLEAGENT_H
#define __INET_IEEE80211TWTEXAMPLEAGENT_H

#include "inet/linklayer/ieee80211/mgmt/Ieee80211AgentSta.h"

namespace inet {
namespace ieee80211 {

/** Scenario-only STA agent that requests TWT after association. */
class INET_API Ieee80211TwtExampleAgent : public Ieee80211AgentSta
{
  protected:
    bool requestBroadcast = false;
    bool announced = false;
    bool implicit = true;
    simtime_t wakeInterval;
    simtime_t wakeDuration;
    simtime_t firstWakeOffset;
    simtime_t twtSetupRetryInterval;
    int twtSetupMaxAttempts = 1;
    int twtSetupAttempts = 0;
    int broadcastId = 0;
    bool twtAgreementActive = false;
    MacAddress twtPeerAddress;
    cMessage *twtSetupRetryTimer = nullptr;

  protected:
    virtual ~Ieee80211TwtExampleAgent();
    virtual void initialize(int stage) override;
    virtual void handleTimer(cMessage *msg) override;
    virtual void sendTwtSetupRequest();
    virtual bool canRetryTwtSetup() const;
    virtual void processAssociateConfirm(Ieee80211Prim_AssociateConfirm *resp) override;
    virtual void processTwtSetupConfirm(Ieee80211Prim_TwtSetupConfirm *resp) override;
    virtual void processTwtTeardownConfirm(Ieee80211Prim_TwtTeardownConfirm *resp) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
};

} // namespace ieee80211
} // namespace inet

#endif
