//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/ratecontrol/VhtMinstrelRateControl.h"

namespace inet {
namespace ieee80211 {

Define_Module(VhtMinstrelRateControl);

void VhtMinstrelRateControl::initialize(int stage)
{
    MinstrelRateControlBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        registerMinstrelSignals("vhtRate");
}

bool VhtMinstrelRateControl::isRateCandidate(const physicallayer::IIeee80211Mode *mode) const
{
    return dynamic_cast<const physicallayer::Ieee80211VhtMode *>(mode) != nullptr;
}

int VhtMinstrelRateControl::getModeMcs(const physicallayer::IIeee80211Mode *mode) const
{
    return check_and_cast<const physicallayer::Ieee80211VhtMode *>(mode)->getDataMode()->getMcsIndex();
}

int VhtMinstrelRateControl::getModeNss(const physicallayer::IIeee80211Mode *mode) const
{
    return check_and_cast<const physicallayer::Ieee80211VhtMode *>(mode)->getDataMode()->getNumberOfSpatialStreams();
}

} // namespace ieee80211
} // namespace inet
