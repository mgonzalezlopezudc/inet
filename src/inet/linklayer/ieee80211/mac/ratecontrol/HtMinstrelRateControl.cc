//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/ratecontrol/HtMinstrelRateControl.h"

namespace inet {
namespace ieee80211 {

Define_Module(HtMinstrelRateControl);

void HtMinstrelRateControl::initialize(int stage)
{
    MinstrelRateControlBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        registerMinstrelSignals("htRate");
}

bool HtMinstrelRateControl::isRateCandidate(const physicallayer::IIeee80211Mode *mode) const
{
    return dynamic_cast<const physicallayer::Ieee80211HtMode *>(mode) != nullptr;
}

int HtMinstrelRateControl::getModeMcs(const physicallayer::IIeee80211Mode *mode) const
{
    return check_and_cast<const physicallayer::Ieee80211HtMode *>(mode)->getDataMode()->getMcsIndex();
}

int HtMinstrelRateControl::getModeNss(const physicallayer::IIeee80211Mode *mode) const
{
    return check_and_cast<const physicallayer::Ieee80211HtMode *>(mode)->getDataMode()->getNumberOfSpatialStreams();
}

} // namespace ieee80211
} // namespace inet
