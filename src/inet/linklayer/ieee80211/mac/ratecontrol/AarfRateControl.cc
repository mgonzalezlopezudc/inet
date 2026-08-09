//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/ratecontrol/AarfRateControl.h"

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(AarfRateControl);

void AarfRateControl::initialize(int stage)
{
    RateControlBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        factor = par("increaseThresholdFactor");
        increaseThreshold = par("increaseThreshold");
        maxIncreaseThreshold = par("maxIncreaseThreshold");
        decreaseThreshold = par("decreaseThreshold");
        maxNss = par("maxNss");
        if (maxNss == 0 || maxNss < -1 || maxNss > 8)
            throw cRuntimeError("Invalid AARF maxNss");
        interval = par("interval");
        enableHtMcsFeedback = par("enableHtMcsFeedback");
        htMcsRequestInterval = par("htMcsRequestInterval");
        htCsiCache.configure(par("htCsiValidityDuration"));
        if (htMcsRequestInterval < SIMTIME_ZERO)
            throw cRuntimeError("HT MCS request interval must not be negative");
        WATCH(factor);
        WATCH(increaseThreshold);
        WATCH(maxIncreaseThreshold);
        WATCH(decreaseThreshold);
        WATCH(maxNss);
        WATCH(interval);
        WATCH(timer);
        WATCH(probing);
        WATCH(numberOfConsSuccTransmissions);
    }
    else if (stage == INITSTAGE_LINK_LAYER && currentMode != nullptr && maxNss != -1 &&
            currentMode->getDataMode()->getNumberOfSpatialStreams() > maxNss)
        currentMode = decreaseRateIfPossible(currentMode);
}

AarfRateControl::HtPeerState& AarfRateControl::getHtPeerState(const MacAddress& peer)
{
    return htPeers[peer];
}

const IIeee80211Mode *AarfRateControl::increaseRateIfPossible(const IIeee80211Mode *currentMode)
{
    auto bandwidth = currentMode->getDataMode()->getBandwidth();
    auto candidate = modeSet->getFasterMode(currentMode);
    while (candidate != nullptr && (candidate->getDataMode()->getBandwidth() != bandwidth ||
            (maxNss != -1 && candidate->getDataMode()->getNumberOfSpatialStreams() > maxNss)))
        candidate = modeSet->getFasterMode(candidate);
    return candidate == nullptr ? currentMode : candidate;
}

const IIeee80211Mode *AarfRateControl::decreaseRateIfPossible(const IIeee80211Mode *currentMode)
{
    auto bandwidth = currentMode->getDataMode()->getBandwidth();
    auto candidate = modeSet->getSlowerMode(currentMode);
    while (candidate != nullptr && (candidate->getDataMode()->getBandwidth() != bandwidth ||
            (maxNss != -1 && candidate->getDataMode()->getNumberOfSpatialStreams() > maxNss)))
        candidate = modeSet->getSlowerMode(candidate);
    return candidate == nullptr ? currentMode : candidate;
}

void AarfRateControl::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module doesn't handle self messages");
}

void AarfRateControl::frameTransmitted(Packet *frame, int retryCount, bool isSuccessful, bool isGivenUp)
{
    increaseRateIfTimerIsExpired();

    if (!isSuccessful && probing) { // probing packet failed
        numberOfConsSuccTransmissions = 0;
        currentMode = decreaseRateIfPossible(currentMode);
        emitDatarateChangedSignal();
        EV_DETAIL << "Decreased rate to " << *currentMode << endl;
        multiplyIncreaseThreshold(factor);
        resetTimer();
    }
    else if (!isSuccessful && retryCount >= decreaseThreshold - 1) { // decreaseThreshold consecutive failed transmissions
        numberOfConsSuccTransmissions = 0;
        currentMode = decreaseRateIfPossible(currentMode);
        emitDatarateChangedSignal();
        EV_DETAIL << "Decreased rate to " << *currentMode << endl;
        resetIncreaseThreshdold();
        resetTimer();
    }
    else if (isSuccessful && retryCount == 0)
        numberOfConsSuccTransmissions++;

    if (numberOfConsSuccTransmissions == increaseThreshold) {
        numberOfConsSuccTransmissions = 0;
        currentMode = increaseRateIfPossible(currentMode);
        emitDatarateChangedSignal();
        EV_DETAIL << "Increased rate to " << *currentMode << endl;
        resetTimer();
        probing = true;
    }
    else
        probing = false;

}

void AarfRateControl::multiplyIncreaseThreshold(double factor)
{
    if (increaseThreshold * factor <= maxIncreaseThreshold)
        increaseThreshold *= factor;
}

void AarfRateControl::resetIncreaseThreshdold()
{
    increaseThreshold = par("increaseThreshold");
}

void AarfRateControl::resetTimer()
{
    timer = simTime();
}

void AarfRateControl::increaseRateIfTimerIsExpired()
{
    if (simTime() - timer >= interval) {
        currentMode = increaseRateIfPossible(currentMode);
        emitDatarateChangedSignal();
        EV_DETAIL << "Increased rate to " << *currentMode << endl;
        resetTimer();
    }
}

void AarfRateControl::frameReceived(Packet *frame)
{
}

const IIeee80211Mode *AarfRateControl::getRate()
{
    Enter_Method("getRate");
    increaseRateIfTimerIsExpired();
    EV_INFO << "The current mode is " << currentMode << " the net bitrate is " << currentMode->getDataMode()->getNetBitrate() << std::endl;
    return currentMode;
}

void AarfRateControl::processReceivedHtMcsRequest(const MacAddress& peer, uint8_t msi,
        const IIeee80211Mode *receivedMode)
{
    if (!enableHtMcsFeedback || peer.isMulticast() || msi > 6)
        return;
    auto& state = getHtPeerState(peer);
    state.pendingFeedback = true;
    state.pendingFeedbackSequenceIdentifier = msi;
    state.pendingFeedbackValue = 127; // Explicitly report no fresh MFB.
    if (receivedMode == nullptr || modeSet->getPhyFamily(receivedMode) != Ieee80211PhyFamily::HT)
        return;
    auto measurement = htCsiCache.findFresh(peer,
            receivedMode->getDataMode()->getBandwidth(),
            receivedMode->getDataMode()->getNumberOfSpatialStreams());
    if (measurement != nullptr && measurement->recommendedMcs <= 76)
        state.pendingFeedbackValue = measurement->recommendedMcs;
}

void AarfRateControl::processReceivedHtMcsFeedback(const MacAddress& peer, uint8_t mfsi,
        uint8_t mfb)
{
    if (!enableHtMcsFeedback || peer.isMulticast() || mfsi > 6)
        return;
    auto& state = getHtPeerState(peer);
    if (!state.outstandingRequest || state.outstandingSequenceIdentifier != mfsi)
        return;
    state.outstandingRequest = false;
    if (mfb == 127 || mfb > 31 || state.outstandingChannelWidth <= Hz(0))
        return;
    const int nss = mfb / 8 + 1;
    const int mcs = mfb % 8;
    auto htDataMode = currentMode == nullptr ? nullptr :
            dynamic_cast<const Ieee80211HtDataMode *>(currentMode->getDataMode());
    const bool ldpc = htDataMode != nullptr && htDataMode->getCode() != nullptr &&
            htDataMode->getCode()->isLdpc();
    auto mode = modeSet->findHtMode(mcs, nss, state.outstandingChannelWidth, ldpc);
    if (mode != nullptr) {
        currentMode = mode;
        emitDatarateChangedSignal();
    }
}

bool AarfRateControl::getPendingHtMcsControl(const MacAddress& peer,
        bool mcsRequestAllowed, bool mcsFeedbackAllowed,
        Ieee80211HtMcsControl& control)
{
    control = {};
    control.mcsFeedbackSequenceIdentifier = 7;
    control.mcsFeedback = 127;
    if (!enableHtMcsFeedback || peer.isMulticast())
        return false;
    auto& state = getHtPeerState(peer);
    if (mcsFeedbackAllowed && state.pendingFeedback) {
        control.mcsFeedbackSequenceIdentifier = state.pendingFeedbackSequenceIdentifier;
        control.mcsFeedback = state.pendingFeedbackValue;
        state.pendingFeedback = false;
        return true;
    }
    if (!mcsRequestAllowed || currentMode == nullptr ||
            modeSet->getPhyFamily(currentMode) != Ieee80211PhyFamily::HT ||
            state.outstandingRequest ||
            (state.lastRequestTime >= SIMTIME_ZERO &&
             simTime() - state.lastRequestTime < htMcsRequestInterval))
        return false;
    control.mcsRequest = true;
    control.mcsRequestSequenceIdentifier = state.nextRequestSequenceIdentifier;
    state.nextRequestSequenceIdentifier = (state.nextRequestSequenceIdentifier + 1) % 7;
    state.outstandingRequest = true;
    state.outstandingSequenceIdentifier = control.mcsRequestSequenceIdentifier;
    state.outstandingChannelWidth = currentMode->getDataMode()->getBandwidth();
    state.outstandingNss = currentMode->getDataMode()->getNumberOfSpatialStreams();
    state.lastRequestTime = simTime();
    return true;
}

void AarfRateControl::invalidateHtPeer(const MacAddress& peer)
{
    htPeers.erase(peer);
    htCsiCache.invalidatePeer(peer);
}

} /* namespace ieee80211 */
} /* namespace inet */
