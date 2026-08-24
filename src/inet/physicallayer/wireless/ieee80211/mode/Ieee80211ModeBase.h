//
// Copyright (C) 2015 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MODEBASE_H
#define __INET_IEEE80211MODEBASE_H

#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211ModeBase : public IIeee80211Mode
{
  private:
    std::string name;

  public:
    Ieee80211ModeBase(const char *name) : name(name) {}
    virtual const char *getName() const override { return name.c_str(); }
    virtual Ieee80211SignalPartDurations getSignalPartDurations(b dataLength) const override {
        const auto preambleDuration = getPreambleMode()->getDuration();
        const auto headerDuration = getHeaderMode()->getDuration();
        const auto dataDuration = getDuration(dataLength) - preambleDuration - headerDuration;
        if (dataDuration < SIMTIME_ZERO)
            throw cRuntimeError("Mode '%s' has a negative residual Data duration", getName());
        return Ieee80211SignalPartDurations(preambleDuration, headerDuration, dataDuration);
    }
};

} /* namespace physicallayer */
} /* namespace inet */

#endif
