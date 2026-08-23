//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXSNIR_H
#define __INET_CHANNELMATRIXSNIR_H

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSnir.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixSnir : public DimensionalSnir
{
  protected:
    Ptr<const IFunction<double, Domain<simsec, Hz>>> snir;
    mutable double matrixMinSnir = NaN;
    mutable double matrixMaxSnir = NaN;
    mutable double matrixMeanSnir = NaN;

  protected:
    Interval<simsec, Hz> getReceptionInterval() const;

  public:
    ChannelMatrixSnir(const IReception *reception, const ChannelMatrixNoise *noise,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& postCombinerNoisePower);

    virtual double getMin() const override;
    virtual double getMax() const override;
    virtual double getMean() const override;
    virtual const Ptr<const IFunction<double, Domain<simsec, Hz>>> getSnir() const override { return snir; }
};

} // namespace physicallayer
} // namespace inet

#endif
