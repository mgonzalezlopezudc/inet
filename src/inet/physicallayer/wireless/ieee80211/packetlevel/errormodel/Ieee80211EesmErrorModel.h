//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EESMERRORMODEL_H
#define __INET_IEEE80211EESMERRORMODEL_H

#include <string>
#include <vector>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211EffectiveSnirErrorModelBase.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211Eesm.h"

namespace inet {
namespace physicallayer {

/**
 * Packet-level BCC SISO HT EESM error model.  It intentionally has no
 * analytical fallback: the supplied PER and beta files are deployment data.
 */
class INET_API Ieee80211EesmErrorModel : public Ieee80211EffectiveSnirErrorModelBase
{
  protected:
    Ieee80211Eesm eesm;
    std::string calibrationSet;

    virtual const char *getErrorModelName() const override { return "Ieee80211EesmErrorModel"; }
    virtual const char *getSisoChainDescription() const override { return "calibrated SISO chain"; }
    virtual void initialize(int stage) override;
    virtual double computeEffectiveSnrDb(const std::vector<double>& carrierSnr, const IIeee80211HtDataMode *dataMode) const override;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return stream << "Ieee80211EesmErrorModel"; }
};

} // namespace physicallayer
} // namespace inet

#endif
