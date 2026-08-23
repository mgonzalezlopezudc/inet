//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EFFECTIVESNIRERRORMODELBASE_H
#define __INET_IEEE80211EFFECTIVESNIRERRORMODELBASE_H

#include <vector>

#include "inet/physicallayer/wireless/common/base/packetlevel/ErrorModelBase.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211PerTable.h"

namespace inet {
namespace physicallayer {

class IIeee80211HtDataMode;

/**
 * Shared packet-level contract for effective-SNR error models.
 *
 * This owns the common HT20/HT40 BCC-SISO carrier and AWGN-PER lookup pipeline;
 * concrete policies supply only their effective-SNR mapping and artifact
 * initialization.  EESM and MI policies therefore share the runtime gates
 * without sharing their calibration artifacts.
 */
class INET_API Ieee80211EffectiveSnirErrorModelBase : public ErrorModelBase
{
  protected:
    Ieee80211PerTable perTable;

    virtual const char *getErrorModelName() const = 0;
    virtual const char *getSisoChainDescription() const { return "supported SISO chain"; }
    virtual double computeEffectiveSnrDb(const std::vector<double>& carrierSnr, const IIeee80211HtDataMode *dataMode) const = 0;

    virtual void initialize(int stage) override;
    virtual std::vector<double> collectDataSnir(const ISnir *snir, const IIeee80211HtDataMode *dataMode) const;
    virtual void validateSpectrum(const ISnir *snir, const IIeee80211HtDataMode *dataMode) const;
    virtual const IIeee80211HtDataMode *getHtDataMode(const ISnir *snir) const;
    virtual void validateHtDataMode(const IIeee80211HtDataMode *dataMode) const;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return stream << getErrorModelName(); }
    virtual double computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const override;
    virtual double computeBitErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const override;
    virtual double computeSymbolErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
