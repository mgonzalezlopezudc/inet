//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTMINSTRELRATECONTROL_H
#define __INET_VHTMINSTRELRATECONTROL_H

#include "inet/linklayer/ieee80211/mac/ratecontrol/MinstrelRateControlBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"

namespace inet {
namespace ieee80211 {

class INET_API VhtMinstrelRateControl : public MinstrelRateControlBase
{
  protected:
    virtual void initialize(int stage) override;
    virtual bool isRateCandidate(const physicallayer::IIeee80211Mode *mode) const override;
    virtual int getModeMcs(const physicallayer::IIeee80211Mode *mode) const override;
    virtual int getModeNss(const physicallayer::IIeee80211Mode *mode) const override;
};

} // namespace ieee80211
} // namespace inet

#endif
