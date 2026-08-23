//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211MIESMERRORMODEL_H
#define __INET_IEEE80211MIESMERRORMODEL_H

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211MiEffectiveSnirErrorModelBase.h"

namespace inet {
namespace physicallayer {

/** Shared symbol-MI policy used by the MIESM and RBIR public modules. */
class INET_API Ieee80211SymbolMiErrorModel : public Ieee80211MiEffectiveSnirErrorModelBase
{
  protected:
    virtual double computeMappedEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta) const override;
};

/** MIESM public policy name. */
class INET_API Ieee80211MiesmErrorModel : public Ieee80211SymbolMiErrorModel
{
  protected:
    virtual const char *getErrorModelName() const override { return "Ieee80211MiesmErrorModel"; }
};

/** RBIR public policy name; fixed-modulation SISO uses the same symbol-MI map. */
class INET_API Ieee80211RbirErrorModel : public Ieee80211SymbolMiErrorModel
{
  protected:
    virtual const char *getErrorModelName() const override { return "Ieee80211RbirErrorModel"; }
};

} // namespace physicallayer
} // namespace inet

#endif
