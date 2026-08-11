//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IRADIOMODEPOLICY_H
#define __INET_IRADIOMODEPOLICY_H

#include <cstdint>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"

namespace inet {
namespace ieee80211 {

class INET_API Ieee80211RadioModePolicySnapshot
{
  protected:
    physicallayer::IRadio::RadioMode baselineRadioMode = static_cast<physicallayer::IRadio::RadioMode>(-1);
    physicallayer::IRadio::RadioMode desiredRadioMode = static_cast<physicallayer::IRadio::RadioMode>(-1);
    physicallayer::IRadio::RadioMode observedRadioMode = static_cast<physicallayer::IRadio::RadioMode>(-1);
    physicallayer::IRadio::RadioMode externalRadioMode = static_cast<physicallayer::IRadio::RadioMode>(-1);
    bool lifecycleUp = false;
    bool transmissionActive = false;
    bool twtAwake = true;
    bool externalRadioModeSet = false;
    uint64_t generation = 0;

  public:
    Ieee80211RadioModePolicySnapshot() {}
    Ieee80211RadioModePolicySnapshot(physicallayer::IRadio::RadioMode baselineRadioMode,
            physicallayer::IRadio::RadioMode desiredRadioMode,
            physicallayer::IRadio::RadioMode observedRadioMode,
            physicallayer::IRadio::RadioMode externalRadioMode,
            bool lifecycleUp, bool transmissionActive, bool twtAwake,
            bool externalRadioModeSet, uint64_t generation) :
        baselineRadioMode(baselineRadioMode), desiredRadioMode(desiredRadioMode),
        observedRadioMode(observedRadioMode), externalRadioMode(externalRadioMode),
        lifecycleUp(lifecycleUp), transmissionActive(transmissionActive),
        twtAwake(twtAwake), externalRadioModeSet(externalRadioModeSet),
        generation(generation) {}

    physicallayer::IRadio::RadioMode getBaselineRadioMode() const { return baselineRadioMode; }
    physicallayer::IRadio::RadioMode getDesiredRadioMode() const { return desiredRadioMode; }
    physicallayer::IRadio::RadioMode getObservedRadioMode() const { return observedRadioMode; }
    bool hasExternalRadioMode() const { return externalRadioModeSet; }
    physicallayer::IRadio::RadioMode getExternalRadioMode() const { return externalRadioMode; }
    bool isLifecycleUp() const { return lifecycleUp; }
    bool isTransmissionActive() const { return transmissionActive; }
    bool isTwtAwake() const { return twtAwake; }
    uint64_t getGeneration() const { return generation; }
};

class INET_API IRadioModePolicy
{
  public:
    virtual ~IRadioModePolicy() {}

    virtual void initializeState(physicallayer::IRadio::RadioMode baselineRadioMode,
            physicallayer::IRadio::RadioMode observedRadioMode, bool lifecycleUp) = 0;
    virtual void setLifecycleUp(bool lifecycleUp) = 0;
    virtual void setTransmissionActive(bool transmissionActive) = 0;
    virtual void setTwtAwake(bool twtAwake) = 0;
    virtual void setExternalRadioMode(physicallayer::IRadio::RadioMode radioMode) = 0;
    virtual void setObservedRadioMode(physicallayer::IRadio::RadioMode radioMode) = 0;
    virtual Ieee80211RadioModePolicySnapshot getSnapshot() const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
