//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixSnir.h"

#include <algorithm>
#include <cmath>

#include "inet/common/math/AlgebraicOperations.h"
#include "inet/common/math/PrimitiveFunctions.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixReceptionAnalogModel.h"

namespace inet {
namespace physicallayer {

namespace {

class MaterializedSpatialSnirFunction final : public FunctionBase<double, Domain<simsec, Hz>>
{
  protected:
    const std::shared_ptr<const MaterializedSpatialReception> materialized;
    const Interval<simsec, Hz> domain;

    static const MaterializedSpatialReception& requireMaterialized(
        const std::shared_ptr<const MaterializedSpatialReception>& materialized)
    {
        if (!materialized)
            throw cRuntimeError("Materialized spatial SNIR function requires a non-null payload");
        return *materialized;
    }

    static Interval<simsec, Hz> computeDomain(const MaterializedSpatialReception& materialized)
    {
        const auto& cells = materialized.getCells();
        simtime_t startTime = cells.front().getResource().getStartTime();
        simtime_t endTime = cells.front().getResource().getEndTime();
        Hz lowerFrequency = cells.front().getResource().getLowerFrequency();
        Hz upperFrequency = cells.front().getResource().getUpperFrequency();
        for (const auto& cell : cells) {
            startTime = std::min(startTime, cell.getResource().getStartTime());
            endTime = std::max(endTime, cell.getResource().getEndTime());
            lowerFrequency = std::min(lowerFrequency, cell.getResource().getLowerFrequency());
            upperFrequency = std::max(upperFrequency, cell.getResource().getUpperFrequency());
        }
        return Interval<simsec, Hz>(Point<simsec, Hz>(simsec(startTime), lowerFrequency),
            Point<simsec, Hz>(simsec(endTime), upperFrequency), 0b11, 0b00, 0b00);
    }

  public:
    explicit MaterializedSpatialSnirFunction(
        const std::shared_ptr<const MaterializedSpatialReception>& materialized) :
        materialized(materialized), domain(computeDomain(requireMaterialized(materialized)))
    {}

    virtual Interval<double> getRange() const override {
        return Interval<double>(0, materialized->getMaximumSinr(), 0b1, 0b1, 0b0);
    }
    virtual typename Domain<simsec, Hz>::I getDomain() const override { return domain; }
    virtual double getValue(const Point<simsec, Hz>& point) const override {
        const auto cell = materialized->findCell(std::get<0>(point).get(), std::get<1>(point));
        return cell == nullptr ? 0 : cell->getScalarSinr();
    }
    virtual void partition(const Interval<simsec, Hz>& interval,
        const std::function<void(const Interval<simsec, Hz>&,
            const IFunction<double, Domain<simsec, Hz>> *)> callback) const override
    {
        for (const auto& cell : materialized->getCells()) {
            const auto& resource = cell.getResource();
            const Interval<simsec, Hz> cellInterval(
                Point<simsec, Hz>(simsec(resource.getStartTime()), resource.getLowerFrequency()),
                Point<simsec, Hz>(simsec(resource.getEndTime()), resource.getUpperFrequency()),
                0b11, 0b00, 0b00);
            const auto intersection = interval.getIntersected(cellInterval);
            if (!intersection.isEmpty()) {
                ConstantFunction<double, Domain<simsec, Hz>> function(cell.getScalarSinr());
                callback(intersection, &function);
            }
        }
    }
    virtual bool isFinite(const Interval<simsec, Hz>&) const override { return true; }
    virtual bool isNonZero(const Interval<simsec, Hz>&) const override {
        return materialized->getMaximumSinr() != 0;
    }
    virtual void printStructure(std::ostream& stream, int level = 0) const override {
        stream << "(MaterializedSpatialSnir cells=" << materialized->getCells().size() << ")";
    }
};

} // namespace

ChannelMatrixSnir::ChannelMatrixSnir(const IReception *reception, const ChannelMatrixNoise *noise,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& compatibilityNoisePower) :
    DimensionalSnir(reception, noise)
{
    const auto desired = check_and_cast<const ChannelMatrixReceptionAnalogModel *>(reception->getAnalogModel());
    if (!compatibilityNoisePower)
        throw cRuntimeError("Channel-matrix SNIR requires non-null compatibility noise");
    snir = desired->getPower()->divide(compatibilityNoisePower);
}

ChannelMatrixSnir::ChannelMatrixSnir(const IReception *reception, const ChannelMatrixNoise *noise,
    const std::shared_ptr<const MaterializedSpatialReception>& materializedSpatialReception) :
    DimensionalSnir(reception, noise), materializedSpatialReception(materializedSpatialReception)
{
    if (!materializedSpatialReception)
        throw cRuntimeError("Channel-matrix SNIR requires non-null materialized spatial results");
    snir = makeShared<MaterializedSpatialSnirFunction>(materializedSpatialReception);
    matrixMinSnir = materializedSpatialReception->getMinimumSinr();
    matrixMaxSnir = materializedSpatialReception->getMaximumSinr();
    matrixMeanSnir = materializedSpatialReception->getMeanSinr();
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

double ChannelMatrixSnir::getMinimum(IRadioSignal::SignalPart part) const
{
    if (!materializedSpatialReception)
        return getMin();
    return materializedSpatialReception->getMinimumSinr(part);
}

double ChannelMatrixSnir::getMaximum(IRadioSignal::SignalPart part) const
{
    if (!materializedSpatialReception)
        return getMax();
    return materializedSpatialReception->getMaximumSinr(part);
}

double ChannelMatrixSnir::getMean(IRadioSignal::SignalPart part) const
{
    if (!materializedSpatialReception)
        return getMean();
    return materializedSpatialReception->getMeanSinr(part);
}

bool ChannelMatrixSnir::allRequiredOutputPowersMeet(
    IRadioSignal::SignalPart part, W sensitivity) const
{
    return materializedSpatialReception != nullptr &&
        materializedSpatialReception->allRequiredOutputPowersMeet(part, sensitivity);
}

} // namespace physicallayer
} // namespace inet
