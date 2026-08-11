//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211RADIOMODEPOLICY_H
#define __INET_IEEE80211RADIOMODEPOLICY_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mac/contract/IRadioModePolicy.h"

namespace inet {
namespace ieee80211 {

class INET_API Ieee80211RadioModePolicy : public SimpleModule, public IRadioModePolicy
{
  protected:
    physicallayer::IRadio::RadioMode baselineRadioMode = static_cast<physicallayer::IRadio::RadioMode>(-1);
    physicallayer::IRadio::RadioMode desiredRadioMode = physicallayer::IRadio::RADIO_MODE_OFF;
    physicallayer::IRadio::RadioMode observedRadioMode = static_cast<physicallayer::IRadio::RadioMode>(-1);
    physicallayer::IRadio::RadioMode externalRadioMode = static_cast<physicallayer::IRadio::RadioMode>(-1);
    bool lifecycleUp = false;
    bool transmissionActive = false;
    bool twtAwake = true;
    bool externalRadioModeSet = false;
    uint64_t generation = 0;

  protected:
    virtual physicallayer::IRadio::RadioMode computeDesiredRadioMode() const;
    virtual void updateGenerationIfChanged(const Ieee80211RadioModePolicySnapshot& oldSnapshot);
    virtual void validateRadioMode(physicallayer::IRadio::RadioMode radioMode, bool allowUndefined) const;

  public:
    virtual void initializeState(physicallayer::IRadio::RadioMode baselineRadioMode,
            physicallayer::IRadio::RadioMode observedRadioMode, bool lifecycleUp) override;
    virtual void setLifecycleUp(bool lifecycleUp) override;
    virtual void setTransmissionActive(bool transmissionActive) override;
    virtual void setTwtAwake(bool twtAwake) override;
    virtual void setExternalRadioMode(physicallayer::IRadio::RadioMode radioMode) override;
    virtual void setObservedRadioMode(physicallayer::IRadio::RadioMode radioMode) override;
    virtual Ieee80211RadioModePolicySnapshot getSnapshot() const override;
};

} // namespace ieee80211
} // namespace inet

#endif
