//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211MMIBERRORMODEL_H
#define __INET_IEEE80211MMIBERRORMODEL_H

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211MiEffectiveSnirErrorModelBase.h"

namespace inet {
namespace physicallayer {

/** Gray-labelled bit-channel MI effective-SNR policy for HT BCC SISO. */
class INET_API Ieee80211MmibErrorModel : public Ieee80211MiEffectiveSnirErrorModelBase
{
  protected:
    virtual const char *getErrorModelName() const override { return "Ieee80211MmibErrorModel"; }
    virtual double computeMappedEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
