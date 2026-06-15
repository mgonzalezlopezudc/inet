//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/ratecontrol/RateControlBase.h"

#include "inet/common/Simsignals.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

simsignal_t RateControlBase::datarateChangedSignal = cComponent::registerSignal("datarateChanged");

void RateControlBase::initialize(int stage)
{
    ModeSetListener::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
        WATCH_EXPR("currentMode", currentMode ? currentMode->getName() : "none");
}

const IIeee80211Mode *RateControlBase::increaseRateIfPossible(const IIeee80211Mode *currentMode)
{
    const IIeee80211Mode *newMode = modeSet->getFasterMode(currentMode);
    return newMode == nullptr ? currentMode : newMode;
}

const IIeee80211Mode *RateControlBase::decreaseRateIfPossible(const IIeee80211Mode *currentMode)
{
    const IIeee80211Mode *newMode = modeSet->getSlowerMode(currentMode);
    return newMode == nullptr ? currentMode : newMode;
}

void RateControlBase::emitDatarateChangedSignal()
{
    bps rate = currentMode->getDataMode()->getNetBitrate();
    emit(datarateChangedSignal, rate.get());
}

void RateControlBase::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        modeSet = check_and_cast<Ieee80211ModeSet *>(obj);
        cModule *nic = getContainingNicModule(this);
        cModule *radio = nic ? nic->getSubmodule("radio") : nullptr;
        cModule *transmitter = radio ? radio->getSubmodule("transmitter") : nullptr;
        Hz bandBw = Hz(NaN);
        std::string bandName = "";
        if (transmitter && transmitter->hasPar("bandName")) {
            bandName = transmitter->par("bandName").stringValue();
        } else if (radio && radio->hasPar("bandName")) {
            bandName = radio->par("bandName").stringValue();
        }
        if (!bandName.empty()) {
            auto band = Ieee80211CompliantBands::getBand(bandName.c_str());
            if (band)
                bandBw = band->getSpacing();
        }
        if (std::isnan(bandBw.get()) && transmitter && transmitter->hasPar("bandwidth")) {
            bandBw = Hz(transmitter->par("bandwidth").doubleValue());
        }
        double initRate = par("initialRate");
        currentMode = initRate == -1 ? modeSet->getFastestMandatoryMode(bandBw) : modeSet->getMode(bps(initRate), bandBw);
        emitDatarateChangedSignal();
    }
}

} /* namespace ieee80211 */
} /* namespace inet */

