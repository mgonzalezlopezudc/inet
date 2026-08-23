//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixPhysicalPowerMaterializer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "inet/common/math/AlgebraicOperations.h"
#include "inet/common/math/PrimitiveFunctions.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixAlgebra.h"

namespace inet {
namespace physicallayer {

using namespace inet::math;

namespace {

constexpr double TRACE_RELATIVE_TOLERANCE = 1e-9;
constexpr size_t MAX_EAGER_SAMPLE_VALUES = 20000000;

double validatePhysicalPower(WpHz power, const char *name);

size_t checkedIntervals(double requested, const char *name)
{
    if (!std::isfinite(requested) || requested < 0)
        throw cRuntimeError("Channel-matrix %s sampling density is invalid: %g", name, requested);
    if (requested > (double)std::numeric_limits<size_t>::max() - 1)
        throw cRuntimeError("Channel-matrix %s sampling density overflows", name);
    return std::max<size_t>(1, (size_t)std::ceil(requested));
}

size_t checkedProduct(size_t left, size_t right, const char *name)
{
    if (left != 0 && right > std::numeric_limits<size_t>::max() / left)
        throw cRuntimeError("Channel-matrix %s sample count overflows", name);
    const size_t result = left * right;
    if (result > MAX_EAGER_SAMPLE_VALUES)
        throw cRuntimeError("Channel-matrix %s sample count %zu exceeds eager materialization limit %zu",
            name, result, MAX_EAGER_SAMPLE_VALUES);
    return result;
}

void validateFinitePositive(Hz value, const char *name)
{
    if (!std::isfinite(value.get()) || value <= Hz(0))
        throw cRuntimeError("Channel-matrix %s must be finite and positive", name);
}

/**
 * Piecewise bilinear table whose values are all captured during construction.
 * Its partition implementation deliberately emits the primitive function
 * types understood by INET's dimensional integration helpers.
 */
class EagerPhysicalPowerFunction final : public FunctionBase<WpHz, Domain<simsec, Hz>>
{
  public:
    struct Grid final
    {
        simsec startTime;
        simsec endTime;
        Hz lowerFrequency;
        Hz upperFrequency;
        size_t timeSamples;
        size_t frequencySamples;
        std::vector<WpHz> values;
    };

  protected:
    const std::vector<Grid> grids;
    const simsec lowerTime;
    const simsec upperTime;
    const Hz lowerFrequency;
    const Hz upperFrequency;
    const WpHz maximumValue;

    static const Grid& firstGrid(const std::vector<Grid>& grids)
    {
        if (grids.empty())
            throw cRuntimeError("Eager physical power function requires at least one grid");
        return grids.front();
    }

    static size_t cellIndex(size_t timeIndex, size_t frequencyIndex, size_t frequencySamples)
    {
        if (timeIndex >= std::numeric_limits<size_t>::max() / frequencySamples)
            throw cRuntimeError("Eager physical power cell index overflows");
        return timeIndex * frequencySamples + frequencyIndex;
    }

    const Grid *findGrid(simsec time) const
    {
        for (size_t index = 0; index < grids.size(); index++) {
            const auto& grid = grids[index];
            if (time >= grid.startTime && (time < grid.endTime ||
                (index + 1 == grids.size() && time == grid.endTime)))
                return &grid;
        }
        return nullptr;
    }

    static WpHz interpolateCell(const Grid& grid, size_t timeIndex, size_t frequencyIndex,
        simsec time, Hz frequency)
    {
        const simsec time1 = grid.startTime + (grid.endTime - grid.startTime) *
            (double)timeIndex / (double)(grid.timeSamples - 1);
        const simsec time2 = grid.startTime + (grid.endTime - grid.startTime) *
            (double)(timeIndex + 1) / (double)(grid.timeSamples - 1);
        const Hz frequency1 = grid.lowerFrequency + (grid.upperFrequency - grid.lowerFrequency) *
            (double)frequencyIndex / (double)(grid.frequencySamples - 1);
        const Hz frequency2 = grid.lowerFrequency + (grid.upperFrequency - grid.lowerFrequency) *
            (double)(frequencyIndex + 1) / (double)(grid.frequencySamples - 1);
        const double alpha = std::clamp((time - time1).get<simsec>().dbl() /
            (time2 - time1).get<simsec>().dbl(), 0.0, 1.0);
        const double beta = std::clamp((frequency - frequency1).get() / (frequency2 - frequency1).get(), 0.0, 1.0);
        const double v00 = grid.values[cellIndex(timeIndex, frequencyIndex, grid.frequencySamples)].get<WpHz>();
        const double v01 = grid.values[cellIndex(timeIndex, frequencyIndex + 1, grid.frequencySamples)].get<WpHz>();
        const double v10 = grid.values[cellIndex(timeIndex + 1, frequencyIndex, grid.frequencySamples)].get<WpHz>();
        const double v11 = grid.values[cellIndex(timeIndex + 1, frequencyIndex + 1, grid.frequencySamples)].get<WpHz>();
        return WpHz((1 - alpha) * ((1 - beta) * v00 + beta * v01) +
            alpha * ((1 - beta) * v10 + beta * v11));
    }

    static WpHz cellValue(const Grid& grid, size_t timeIndex, size_t frequencyIndex,
        simsec time, Hz frequency)
    {
        return interpolateCell(grid, timeIndex, frequencyIndex, time, frequency);
    }

    void emitCell(const Grid& grid, size_t timeIndex, size_t frequencyIndex,
        const Interval<simsec, Hz>& interval,
        const std::function<void(const Interval<simsec, Hz>&, const IFunction<WpHz, Domain<simsec, Hz>> *)>& callback) const
    {
        if (interval.isEmpty())
            return;
        const auto fixed = interval.getFixed();
        const auto lower = interval.getLower();
        const auto upper = interval.getUpper();
        if (fixed == 0b11) {
            ConstantFunction<WpHz, Domain<simsec, Hz>> function(cellValue(grid, timeIndex, frequencyIndex,
                std::get<0>(lower), std::get<1>(lower)));
            callback(interval, &function);
        }
        else if (fixed & 0b10) {
            UnilinearFunction<WpHz, Domain<simsec, Hz>> function(lower, upper,
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(lower), std::get<1>(lower)),
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(upper), std::get<1>(upper)), 1);
            simplifyAndCall(interval, &function, callback);
        }
        else if (fixed & 0b01) {
            UnilinearFunction<WpHz, Domain<simsec, Hz>> function(lower, upper,
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(lower), std::get<1>(lower)),
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(upper), std::get<1>(upper)), 0);
            simplifyAndCall(interval, &function, callback);
        }
        else {
            BilinearFunction<WpHz, Domain<simsec, Hz>> function(
                lower, Point<simsec, Hz>(std::get<0>(lower), std::get<1>(upper)),
                Point<simsec, Hz>(std::get<0>(upper), std::get<1>(lower)), upper,
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(lower), std::get<1>(lower)),
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(lower), std::get<1>(upper)),
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(upper), std::get<1>(lower)),
                cellValue(grid, timeIndex, frequencyIndex, std::get<0>(upper), std::get<1>(upper)), 0, 1);
            simplifyAndCall(interval, &function, callback);
        }
    }

  public:
    explicit EagerPhysicalPowerFunction(const std::vector<Grid>& grids) :
        grids(grids), lowerTime(firstGrid(grids).startTime), upperTime(grids.back().endTime),
        lowerFrequency(firstGrid(grids).lowerFrequency), upperFrequency(firstGrid(grids).upperFrequency),
        maximumValue([&] {
            double value = 0;
            for (const auto& grid : grids)
                for (const auto& sample : grid.values)
                    value = std::max(value, sample.get<WpHz>());
            return WpHz(value);
        }())
    {
        if (grids.empty())
            throw cRuntimeError("Eager physical power function requires at least one grid");
        for (size_t index = 0; index < grids.size(); index++) {
            const auto& grid = grids[index];
            if (grid.startTime >= grid.endTime || grid.timeSamples < 2 || grid.frequencySamples < 2 ||
                grid.values.size() != cellIndex(grid.timeSamples - 1, grid.frequencySamples - 1,
                    grid.frequencySamples) + 1)
                throw cRuntimeError("Eager physical power grid dimensions are invalid");
            if (index > 0 && grids[index - 1].endTime != grid.startTime)
                throw cRuntimeError("Eager physical power grids are not adjacent");
            for (const auto& sample : grid.values)
                validatePhysicalPower(sample, "eager physical power sample");
        }
    }

    virtual Interval<WpHz> getRange() const override
    {
        return Interval<WpHz>(WpHz(0), maximumValue, 0b1, 0b1, 0b0);
    }

    virtual typename Domain<simsec, Hz>::I getDomain() const override
    {
        return Interval<simsec, Hz>(Point<simsec, Hz>(lowerTime, lowerFrequency),
            Point<simsec, Hz>(upperTime, upperFrequency), 0b11, 0b00, 0b00);
    }

    virtual WpHz getValue(const Point<simsec, Hz>& point) const override
    {
        const simsec time = std::get<0>(point);
        const Hz frequency = std::get<1>(point);
        const Grid *grid = findGrid(time);
        if (!grid || frequency < grid->lowerFrequency || frequency >= grid->upperFrequency)
            return WpHz(0);
        const double timeRatio = (time - grid->startTime).get<simsec>().dbl() /
            (grid->endTime - grid->startTime).get<simsec>().dbl();
        const double frequencyRatio = (frequency - grid->lowerFrequency).get() /
            (grid->upperFrequency - grid->lowerFrequency).get();
        const size_t timeIndex = std::min(grid->timeSamples - 2,
            (size_t)std::floor(std::max(0.0, timeRatio) * (grid->timeSamples - 1)));
        const size_t frequencyIndex = std::min(grid->frequencySamples - 2,
            (size_t)std::floor(std::max(0.0, frequencyRatio) * (grid->frequencySamples - 1)));
        return interpolateCell(*grid, timeIndex, frequencyIndex, time, frequency);
    }

    virtual void partition(const Interval<simsec, Hz>& interval,
        const std::function<void(const Interval<simsec, Hz>&, const IFunction<WpHz, Domain<simsec, Hz>> *)> callback) const override
    {
        for (const auto& grid : grids) {
            for (size_t timeIndex = 0; timeIndex + 1 < grid.timeSamples; timeIndex++) {
                const simsec time1 = grid.startTime + (grid.endTime - grid.startTime) *
                    (double)timeIndex / (double)(grid.timeSamples - 1);
                const simsec time2 = grid.startTime + (grid.endTime - grid.startTime) *
                    (double)(timeIndex + 1) / (double)(grid.timeSamples - 1);
                for (size_t frequencyIndex = 0; frequencyIndex + 1 < grid.frequencySamples; frequencyIndex++) {
                    const Hz frequency1 = grid.lowerFrequency + (grid.upperFrequency - grid.lowerFrequency) *
                        (double)frequencyIndex / (double)(grid.frequencySamples - 1);
                    const Hz frequency2 = grid.lowerFrequency + (grid.upperFrequency - grid.lowerFrequency) *
                        (double)(frequencyIndex + 1) / (double)(grid.frequencySamples - 1);
                    const auto cell = Interval<simsec, Hz>(Point<simsec, Hz>(time1, frequency1),
                        Point<simsec, Hz>(time2, frequency2), 0b11, 0b00, 0b00);
                    emitCell(grid, timeIndex, frequencyIndex, interval.getIntersected(cell), callback);
                }
            }
        }
    }

    virtual bool isFinite(const Interval<simsec, Hz>&) const override { return true; }
    virtual bool isNonZero(const Interval<simsec, Hz>&) const override { return maximumValue != WpHz(0); }
    virtual void printStructure(std::ostream& stream, int level = 0) const override
    {
        stream << "(EagerPhysicalPower grids=" << grids.size() << ")";
    }
};

double validatePhysicalPower(WpHz power, const char *name)
{
    const double value = power.get<WpHz>();
    if (!std::isfinite(value) || value < 0)
        throw cRuntimeError("Channel-matrix %s must be finite and nonnegative, got %g", name, value);
    return value;
}

} // namespace

double ChannelMatrixPhysicalPowerMaterializer::computePhysicalGain(
    const IChannelMatrixSnapshot& snapshot, const SpatialTransmissionPlan::Segment& segment,
    simtime_t absoluteTime, Hz basebandFrequency)
{
    const ComplexMatrix response = snapshot.getResponse(absoluteTime,
        snapshot.getReferenceFrequency() + basebandFrequency);
    ChannelMatrixAlgebra::validateDimensions(response, snapshot.getNumReceiveAntennas(),
        snapshot.getNumTransmitAntennas(), "Channel snapshot response");
    const ComplexMatrix transmitCovariance = segment.getTransmitCovariance(basebandFrequency);
    ChannelMatrixAlgebra::validateDimensions(transmitCovariance,
        snapshot.getNumTransmitAntennas(), snapshot.getNumTransmitAntennas(),
        "Spatial transmission covariance");
    const ComplexMatrix receivedCovariance = ChannelMatrixAlgebra::multiply(
        ChannelMatrixAlgebra::multiply(response, transmitCovariance),
        ChannelMatrixAlgebra::conjugateTranspose(response));
    ChannelMatrixAlgebra::validateHermitian(receivedCovariance, TRACE_RELATIVE_TOLERANCE,
        "Received physical covariance");

    double trace = 0;
    double scale = 1;
    for (int row = 0; row < receivedCovariance.getNumRows(); row++) {
        trace += receivedCovariance.get(row, row).real();
        scale = std::max(scale, std::abs(receivedCovariance.get(row, row)));
    }
    const double tolerance = TRACE_RELATIVE_TOLERANCE * scale;
    for (int row = 0; row < receivedCovariance.getNumRows(); row++) {
        if (std::abs(receivedCovariance.get(row, row).imag()) > tolerance)
            throw cRuntimeError("Received physical covariance has a non-real diagonal");
    }
    if (!std::isfinite(trace) || trace < -tolerance)
        throw cRuntimeError("Received physical covariance trace is invalid: %g", trace);
    return std::max(0.0, trace);
}

const Ptr<const ChannelMatrixPhysicalPowerMaterializer::PhysicalPowerFunction>
ChannelMatrixPhysicalPowerMaterializer::materialize(
    const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
    const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
    simtime_t receptionStartTime, simtime_t receptionEndTime,
    Hz centerFrequency, Hz bandwidth,
    const Ptr<const PhysicalPowerFunction>& deterministicLargeScalePower)
{
    if (!snapshot || !spatialTransmissionPlan || !deterministicLargeScalePower)
        throw cRuntimeError("Channel-matrix physical power materialization requires non-null inputs");
    if (receptionStartTime >= receptionEndTime)
        throw cRuntimeError("Channel-matrix reception interval must have positive duration");
    validateFinitePositive(centerFrequency, "center frequency");
    validateFinitePositive(bandwidth, "bandwidth");
    if (receptionStartTime < snapshot->getStartTime() || receptionEndTime > snapshot->getEndTime())
        throw cRuntimeError("Channel-matrix reception interval [%s, %s) is outside snapshot interval [%s, %s)",
            receptionStartTime.str().c_str(), receptionEndTime.str().c_str(),
            snapshot->getStartTime().str().c_str(), snapshot->getEndTime().str().c_str());
    if (spatialTransmissionPlan->getNumberOfTransmitAntennas() != snapshot->getNumTransmitAntennas())
        throw cRuntimeError("Spatial plan antenna count %d disagrees with channel snapshot transmit dimension %d",
            spatialTransmissionPlan->getNumberOfTransmitAntennas(), snapshot->getNumTransmitAntennas());

    const simtime_t duration = receptionEndTime - receptionStartTime;
    spatialTransmissionPlan->validateCompleteCoverage(duration);
    const double maximumTemporalFrequencyHz = snapshot->getActualMaximumTemporalFrequency().get();
    const double maximumDelaySeconds = snapshot->getMaximumExcessDelay().dbl();
    if (!std::isfinite(maximumTemporalFrequencyHz) || maximumTemporalFrequencyHz < 0 ||
        !std::isfinite(maximumDelaySeconds) || maximumDelaySeconds < 0)
        throw cRuntimeError("Channel-matrix snapshot sampling metadata is invalid");

    const double bandwidthHz = bandwidth.get();
    const size_t frequencyIntervals = maximumDelaySeconds == 0 ? 2 :
        std::max<size_t>(2, checkedIntervals(bandwidthHz * 20 * maximumDelaySeconds, "frequency"));
    if (frequencyIntervals > std::numeric_limits<size_t>::max() - 1)
        throw cRuntimeError("Channel-matrix frequency sample count overflows");
    const size_t frequencySamples = frequencyIntervals + 1;
    checkedProduct(1, frequencySamples, "frequency");

    const Hz lowerFrequency = centerFrequency - bandwidth / 2;
    const Hz frequencyStep = bandwidth / (double)frequencyIntervals;
    std::vector<EagerPhysicalPowerFunction::Grid> grids;
    grids.reserve(spatialTransmissionPlan->getSegments().size());
    size_t totalSamples = 0;
    for (size_t segmentIndex = 0; segmentIndex < spatialTransmissionPlan->getSegments().size(); segmentIndex++) {
        const auto& segment = spatialTransmissionPlan->getSegment((int)segmentIndex);
        const double segmentDurationSeconds = (segment.getEndOffset() - segment.getStartOffset()).dbl();
        const size_t timeIntervals = std::max<size_t>(1,
            checkedIntervals(segmentDurationSeconds * 20 * maximumTemporalFrequencyHz, "time"));
        if (timeIntervals > std::numeric_limits<size_t>::max() - 1)
            throw cRuntimeError("Channel-matrix time sample count overflows");
        const size_t timeSamples = timeIntervals + 1;
        const size_t segmentSamples = checkedProduct(timeSamples, frequencySamples, "two-dimensional");
        if (totalSamples > MAX_EAGER_SAMPLE_VALUES - segmentSamples)
            throw cRuntimeError("Channel-matrix total eager sample count exceeds limit %zu", MAX_EAGER_SAMPLE_VALUES);
        totalSamples += segmentSamples;

        const simtime_t segmentStartTime = receptionStartTime + segment.getStartOffset();
        const simtime_t segmentEndTime = receptionStartTime + segment.getEndOffset();
        const simtime_t timeStep = (segmentEndTime - segmentStartTime) / (double)timeIntervals;
        const Hz upperFrequency = centerFrequency + bandwidth / 2;
        std::vector<WpHz> values;
        values.reserve(segmentSamples);
        for (size_t timeIndex = 0; timeIndex < timeSamples; timeIndex++) {
            const simtime_t absoluteTime = timeIndex == timeIntervals ? segmentEndTime :
                segmentStartTime + timeStep * (double)timeIndex;
            const simtime_t offset = absoluteTime - receptionStartTime;
            const auto side = offset == segment.getEndOffset() ? SpatialTransmissionPlan::BoundarySide::LEFT_LIMIT :
                SpatialTransmissionPlan::BoundarySide::RIGHT_LIMIT;
            const auto& selectedSegment = spatialTransmissionPlan->getSegmentAt(offset, side);
            for (size_t frequencyIndex = 0; frequencyIndex < frequencySamples; frequencyIndex++) {
                const Hz frequency = frequencyIndex == frequencyIntervals ? upperFrequency :
                    lowerFrequency + frequencyStep * (double)frequencyIndex;
                const double gain = computePhysicalGain(*snapshot, selectedSegment, absoluteTime,
                    frequency - centerFrequency);
                const WpHz deterministicPower = deterministicLargeScalePower->getValue(
                    Point<simsec, Hz>(simsec(absoluteTime), frequency));
                const double deterministicValue = validatePhysicalPower(deterministicPower,
                    "deterministic large-scale power");
                const double value = deterministicValue * gain;
                if (!std::isfinite(value) || value < 0)
                    throw cRuntimeError("Materialized channel-matrix physical power is invalid");
                values.emplace_back(value);
            }
        }
        grids.push_back({simsec(segmentStartTime), simsec(segmentEndTime), lowerFrequency, upperFrequency,
            timeSamples, frequencySamples, std::move(values)});
    }
    if (totalSamples == 0)
        throw cRuntimeError("Channel-matrix materialization produced no samples");
    return makeShared<EagerPhysicalPowerFunction>(grids);
}

} // namespace physicallayer
} // namespace inet
