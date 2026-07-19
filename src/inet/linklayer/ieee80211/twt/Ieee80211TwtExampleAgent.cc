//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/twt/Ieee80211TwtExampleAgent.h"

namespace inet {
namespace ieee80211 {

Define_Module(Ieee80211TwtExampleAgent);

Ieee80211TwtExampleAgent::~Ieee80211TwtExampleAgent()
{
    cancelAndDelete(twtSetupRetryTimer);
}

void Ieee80211TwtExampleAgent::initialize(int stage)
{
    Ieee80211AgentSta::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        requestBroadcast = par("requestBroadcast");
        announced = par("announced");
        implicit = par("implicit");
        wakeInterval = par("wakeInterval");
        wakeDuration = par("wakeDuration");
        firstWakeOffset = par("firstWakeOffset");
        twtSetupRetryInterval = par("twtSetupRetryInterval");
        twtSetupMaxAttempts = par("twtSetupMaxAttempts");
        if (twtSetupMaxAttempts == 0 || twtSetupMaxAttempts < -1)
            throw cRuntimeError("Invalid TWT setup maximum attempt count");
        broadcastId = par("broadcastId");
        twtSetupRetryTimer = new cMessage("twtSetupRetry");
    }
}

void Ieee80211TwtExampleAgent::handleTimer(cMessage *msg)
{
    if (msg == twtSetupRetryTimer)
        sendTwtSetupRequest();
    else
        Ieee80211AgentSta::handleTimer(msg);
}

void Ieee80211TwtExampleAgent::sendTwtSetupRequest()
{
    Ieee80211Prim_TwtSetupRequest request;
    request.setPeerAddress(twtPeerAddress);
    request.setBroadcast(requestBroadcast);
    request.setBroadcastId(broadcastId);
    request.setSetupCommand(0); // Request: AP selects the individual schedule.
    request.setImplicit(implicit);
    request.setAnnounced(announced);
    request.setTargetWakeTime(simTime() + firstWakeOffset);
    request.setWakeInterval(wakeInterval);
    request.setWakeDuration(wakeDuration);
    request.setPersistence(8);
    requestTwtSetup(request);
    twtSetupAttempts++;
    if (canRetryTwtSetup())
        rescheduleAfter(twtSetupRetryInterval, twtSetupRetryTimer);
}

bool Ieee80211TwtExampleAgent::canRetryTwtSetup() const
{
    return !twtAgreementActive && twtSetupRetryInterval >= SIMTIME_ZERO &&
            (twtSetupMaxAttempts == -1 || twtSetupAttempts < twtSetupMaxAttempts);
}

void Ieee80211TwtExampleAgent::processAssociateConfirm(Ieee80211Prim_AssociateConfirm *resp)
{
    Ieee80211AgentSta::processAssociateConfirm(resp);
    if (resp->getResultCode() != PRC_SUCCESS)
        return;
    cancelEvent(twtSetupRetryTimer);
    twtAgreementActive = false;
    twtSetupAttempts = 0;
    twtPeerAddress = resp->getAddress();
    sendTwtSetupRequest();
}

void Ieee80211TwtExampleAgent::processTwtSetupConfirm(Ieee80211Prim_TwtSetupConfirm *resp)
{
    Ieee80211AgentSta::processTwtSetupConfirm(resp);
    twtAgreementActive = resp->getResultCode() == PRC_SUCCESS;
    if (twtAgreementActive)
        cancelEvent(twtSetupRetryTimer);
    else if (!twtSetupRetryTimer->isScheduled() && canRetryTwtSetup())
        rescheduleAfter(twtSetupRetryInterval, twtSetupRetryTimer);
}

void Ieee80211TwtExampleAgent::processTwtTeardownConfirm(Ieee80211Prim_TwtTeardownConfirm *resp)
{
    Ieee80211AgentSta::processTwtTeardownConfirm(resp);
    if (resp->getResultCode() == PRC_SUCCESS)
        twtAgreementActive = false;
}

void Ieee80211TwtExampleAgent::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    if (signalID == l2BeaconLostSignal && twtAgreementActive) {
        Enter_Method("%s", cComponent::getSignalName(signalID));
        EV_INFO << "Ignoring beacon-loss indication while a TWT agreement intentionally permits beacon sleep\n";
        return;
    }
    Ieee80211AgentSta::receiveSignal(source, signalID, obj, details);
}

} // namespace ieee80211
} // namespace inet
