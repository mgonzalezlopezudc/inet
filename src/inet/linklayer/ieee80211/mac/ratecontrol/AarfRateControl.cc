//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/ratecontrol/AarfRateControl.h"

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

} /* namespace ieee80211 */
} /* namespace inet */
