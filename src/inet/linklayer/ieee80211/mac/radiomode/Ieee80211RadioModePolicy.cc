//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/radiomode/Ieee80211RadioModePolicy.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(Ieee80211RadioModePolicy);

void Ieee80211RadioModePolicy::validateRadioMode(IRadio::RadioMode radioMode, bool allowUndefined) const
{
    if (radioMode < IRadio::RADIO_MODE_OFF || radioMode > IRadio::RADIO_MODE_TRANSCEIVER) {
        if (allowUndefined && (radioMode == static_cast<IRadio::RadioMode>(-1) ||
                radioMode == IRadio::RADIO_MODE_SWITCHING))
            return;
        throw cRuntimeError("Invalid radio mode: %d", radioMode);
    }
}

IRadio::RadioMode Ieee80211RadioModePolicy::computeDesiredRadioMode() const
{
    if (!lifecycleUp)
        return IRadio::RADIO_MODE_OFF;
    if (transmissionActive)
        return IRadio::RADIO_MODE_TRANSMITTER;
    if (externalRadioModeSet)
        return externalRadioMode;
    if (!twtAwake)
        return IRadio::RADIO_MODE_SLEEP;
    return baselineRadioMode;
}

void Ieee80211RadioModePolicy::updateGenerationIfChanged(const Ieee80211RadioModePolicySnapshot& oldSnapshot)
{
    desiredRadioMode = computeDesiredRadioMode();
    auto newSnapshot = getSnapshot();
    if (oldSnapshot.getBaselineRadioMode() != newSnapshot.getBaselineRadioMode() ||
            oldSnapshot.getDesiredRadioMode() != newSnapshot.getDesiredRadioMode() ||
            oldSnapshot.getObservedRadioMode() != newSnapshot.getObservedRadioMode() ||
            oldSnapshot.hasExternalRadioMode() != newSnapshot.hasExternalRadioMode() ||
            oldSnapshot.getExternalRadioMode() != newSnapshot.getExternalRadioMode() ||
            oldSnapshot.isLifecycleUp() != newSnapshot.isLifecycleUp() ||
            oldSnapshot.isTransmissionActive() != newSnapshot.isTransmissionActive() ||
            oldSnapshot.isTwtAwake() != newSnapshot.isTwtAwake())
        generation++;
}

void Ieee80211RadioModePolicy::initializeState(IRadio::RadioMode baselineRadioMode,
        IRadio::RadioMode observedRadioMode, bool lifecycleUp)
{
    validateRadioMode(baselineRadioMode, false);
    validateRadioMode(observedRadioMode, true);
    auto oldSnapshot = getSnapshot();
    this->baselineRadioMode = baselineRadioMode;
    this->observedRadioMode = observedRadioMode;
    this->lifecycleUp = lifecycleUp;
    transmissionActive = false;
    twtAwake = true;
    externalRadioModeSet = false;
    externalRadioMode = static_cast<IRadio::RadioMode>(-1);
    updateGenerationIfChanged(oldSnapshot);
}

void Ieee80211RadioModePolicy::setLifecycleUp(bool lifecycleUp)
{
    auto oldSnapshot = getSnapshot();
    this->lifecycleUp = lifecycleUp;
    if (!lifecycleUp)
        transmissionActive = false;
    updateGenerationIfChanged(oldSnapshot);
}

void Ieee80211RadioModePolicy::setTransmissionActive(bool transmissionActive)
{
    if (transmissionActive && !lifecycleUp)
        throw cRuntimeError("Cannot request transmitter radio mode while the MAC is down");
    auto oldSnapshot = getSnapshot();
    this->transmissionActive = transmissionActive;
    updateGenerationIfChanged(oldSnapshot);
}

void Ieee80211RadioModePolicy::setTwtAwake(bool twtAwake)
{
    auto oldSnapshot = getSnapshot();
    this->twtAwake = twtAwake;
    updateGenerationIfChanged(oldSnapshot);
}

void Ieee80211RadioModePolicy::setExternalRadioMode(IRadio::RadioMode radioMode)
{
    validateRadioMode(radioMode, false);
    auto oldSnapshot = getSnapshot();
    externalRadioMode = radioMode;
    externalRadioModeSet = true;
    updateGenerationIfChanged(oldSnapshot);
}

void Ieee80211RadioModePolicy::setObservedRadioMode(IRadio::RadioMode radioMode)
{
    validateRadioMode(radioMode, true);
    auto oldSnapshot = getSnapshot();
    observedRadioMode = radioMode;
    updateGenerationIfChanged(oldSnapshot);
}

Ieee80211RadioModePolicySnapshot Ieee80211RadioModePolicy::getSnapshot() const
{
    return Ieee80211RadioModePolicySnapshot(baselineRadioMode, desiredRadioMode,
            observedRadioMode, externalRadioMode, lifecycleUp, transmissionActive,
            twtAwake, externalRadioModeSet, generation);
}

} // namespace ieee80211
} // namespace inet
