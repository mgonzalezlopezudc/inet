//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixSnir.h"

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixReceptionAnalogModel.h"

namespace inet {
namespace physicallayer {

ChannelMatrixSnir::ChannelMatrixSnir(const IReception *reception, const ChannelMatrixNoise *noise,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& postCombinerNoisePower) :
    DimensionalSnir(reception, noise)
{
    const auto desired = check_and_cast<const ChannelMatrixReceptionAnalogModel *>(reception->getAnalogModel());
    if (!postCombinerNoisePower)
        throw cRuntimeError("Channel-matrix SNIR requires non-null post-combiner noise");
    snir = desired->getPower()->divide(postCombinerNoisePower);
}

Interval<simsec, Hz> ChannelMatrixSnir::getReceptionInterval() const
{
    const auto desired = check_and_cast<const ChannelMatrixReceptionAnalogModel *>(reception->getAnalogModel());
    Point<simsec, Hz> lower(simsec(reception->getStartTime()), desired->getCenterFrequency() - desired->getBandwidth() / 2);
    Point<simsec, Hz> upper(simsec(reception->getEndTime()), desired->getCenterFrequency() + desired->getBandwidth() / 2);
    return Interval<simsec, Hz>(lower, upper, 0b11, 0b00, 0b00);
}

double ChannelMatrixSnir::getMin() const
{
    if (std::isnan(matrixMinSnir))
        matrixMinSnir = snir->getMin(getReceptionInterval());
    return matrixMinSnir;
}

double ChannelMatrixSnir::getMax() const
{
    if (std::isnan(matrixMaxSnir))
        matrixMaxSnir = snir->getMax(getReceptionInterval());
    return matrixMaxSnir;
}

double ChannelMatrixSnir::getMean() const
{
    if (std::isnan(matrixMeanSnir))
        matrixMeanSnir = snir->getMean(getReceptionInterval());
    return matrixMeanSnir;
}

} // namespace physicallayer
} // namespace inet
