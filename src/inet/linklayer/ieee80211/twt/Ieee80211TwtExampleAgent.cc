//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/twt/Ieee80211TwtExampleAgent.h"

namespace inet {
namespace ieee80211 {

Define_Module(Ieee80211TwtExampleAgent);

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
        broadcastId = par("broadcastId");
    }
}

void Ieee80211TwtExampleAgent::processAssociateConfirm(Ieee80211Prim_AssociateConfirm *resp)
{
    Ieee80211AgentSta::processAssociateConfirm(resp);
    if (resp->getResultCode() != PRC_SUCCESS)
        return;
    Ieee80211Prim_TwtSetupRequest request;
    request.setPeerAddress(resp->getAddress());
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
}

void Ieee80211TwtExampleAgent::processTwtSetupConfirm(Ieee80211Prim_TwtSetupConfirm *resp)
{
    Ieee80211AgentSta::processTwtSetupConfirm(resp);
    twtAgreementActive = resp->getResultCode() == PRC_SUCCESS;
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
