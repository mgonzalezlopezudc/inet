//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211FECSUCCESSMODEL_H
#define __INET_IIEEE80211FECSUCCESSMODEL_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

class IIeee80211DataMode;
class Ieee80211DataEncodingPlan;

/** Replaceable deterministic packet-level success model for IEEE 802.11 FEC. */
class INET_API IIeee80211FecSuccessModel
{
  public:
    virtual ~IIeee80211FecSuccessModel() {}

    virtual double computeDataSuccessRate(const IIeee80211DataMode& mode,
            const Ieee80211DataEncodingPlan& plan, double snrDb) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
