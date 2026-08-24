//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211MIEFFECTIVESNIRERRORMODELBASE_H
#define __INET_IEEE80211MIEFFECTIVESNIRERRORMODELBASE_H

#include <string>
#include <vector>

#include <omnetpp/cvaluemap.h>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211EffectiveSnirErrorModelBase.h"

namespace inet {
namespace physicallayer {

class ApskModulationBase;
class IIeee80211HtDataMode;

/** Common initialization and authoritative modulation extraction for MI policies. */
class INET_API Ieee80211MiEffectiveSnirErrorModelBase : public Ieee80211EffectiveSnirErrorModelBase
{
  protected:
    double beta = NaN;

    virtual double computeMappedEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta) const = 0;
    virtual void initialize(int stage) override;
    virtual double computeEffectiveSnrDb(const std::vector<double>& carrierSnr, const IIeee80211HtDataMode *dataMode) const override final;

    const ApskModulationBase *getSubcarrierModulation(const IIeee80211HtDataMode *dataMode) const;

    static void validateAwgnManifest(const cValueMap *manifest, const std::string& acceptanceMode, const char *modelName);
};

} // namespace physicallayer
} // namespace inet

#endif
