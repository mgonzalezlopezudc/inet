//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211MmibErrorModel.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211MutualInformationMapping.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211MmibErrorModel);

double Ieee80211MmibErrorModel::computeMappedEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta) const
{
    return Ieee80211MutualInformationMapping::computeBitEffectiveSnrDb(carrierSnr, modulation, beta);
}

} // namespace physicallayer
} // namespace inet
