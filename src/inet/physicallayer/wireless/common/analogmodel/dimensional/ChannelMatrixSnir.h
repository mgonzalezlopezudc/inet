//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXSNIR_H
#define __INET_CHANNELMATRIXSNIR_H

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSnir.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MaterializedSpatialReception.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISpatialSnir.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixSnir : public DimensionalSnir, public virtual ISpatialSnir
{
  protected:
    Ptr<const IFunction<double, Domain<simsec, Hz>>> snir;
    std::shared_ptr<const MaterializedSpatialReception> materializedSpatialReception;
    mutable double matrixMinSnir = NaN;
    mutable double matrixMaxSnir = NaN;
    mutable double matrixMeanSnir = NaN;

  protected:
    Interval<simsec, Hz> getReceptionInterval() const;

  public:
    ChannelMatrixSnir(const IReception *reception, const ChannelMatrixNoise *noise,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& compatibilityNoisePower);
    ChannelMatrixSnir(const IReception *reception, const ChannelMatrixNoise *noise,
        const std::shared_ptr<const MaterializedSpatialReception>& materializedSpatialReception);

    virtual double getMin() const override;
    virtual double getMax() const override;
    virtual double getMean() const override;
    virtual const Ptr<const IFunction<double, Domain<simsec, Hz>>> getSnir() const override { return snir; }
    virtual const MaterializedSpatialReception *getMaterializedSpatialReception() const override {
        return materializedSpatialReception.get();
    }
    virtual double getMinimum(IRadioSignal::SignalPart part) const override;
    virtual double getMaximum(IRadioSignal::SignalPart part) const override;
    virtual double getMean(IRadioSignal::SignalPart part) const override;
    virtual bool allRequiredOutputPowersMeet(IRadioSignal::SignalPart part, W sensitivity) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
