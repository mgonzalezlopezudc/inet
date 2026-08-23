//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MaterializedSpatialReception.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace inet {
namespace physicallayer {

namespace {

bool matchesPart(const MaterializedSpatialReception::Cell& cell, IRadioSignal::SignalPart part)
{
    return part == IRadioSignal::SIGNAL_PART_WHOLE || cell.getResource().getSignalPart() == part;
}

struct SinrSummary final
{
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = 0;
    long double weightedSum = 0;
    long double totalWeight = 0;
};

SinrSummary summarize(const std::vector<MaterializedSpatialReception::Cell>& cells,
    IRadioSignal::SignalPart part)
{
    SinrSummary result;
    for (const auto& cell : cells) {
        if (!matchesPart(cell, part))
            continue;
        const auto& resource = cell.getResource();
        const long double weight = (long double)(resource.getEndTime() - resource.getStartTime()).dbl() *
            (long double)(resource.getUpperFrequency() - resource.getLowerFrequency()).get();
        result.minimum = std::min(result.minimum, cell.getScalarSinr());
        result.maximum = std::max(result.maximum, cell.getScalarSinr());
        result.weightedSum += weight * cell.getScalarSinr();
        result.totalWeight += weight;
    }
    if (result.totalWeight <= 0)
        throw cRuntimeError("Materialized spatial reception has no resource cells for signal part %d", (int)part);
    return result;
}

} // namespace

MaterializedSpatialReception::Cell::Cell(const ChannelMatrixResourceGrid::Cell& resource,
    const ChannelMatrixDetectionResult& detectionResult) :
    resource(resource), detectionResult(detectionResult), scalarSinr([&] {
        if (detectionResult.getStatus() != ChannelMatrixDetectionStatus::SUCCESS)
            return 0.0;
        if (detectionResult.getSinrs().empty())
            throw cRuntimeError("Successful materialized spatial cell has no stream SINR");
        return *std::min_element(detectionResult.getSinrs().begin(), detectionResult.getSinrs().end());
    }())
{
    if (!std::isfinite(scalarSinr) || scalarSinr < 0)
        throw cRuntimeError("Materialized scalar SINR must be finite and nonnegative");
}

MaterializedSpatialReception::MaterializedSpatialReception(const std::vector<Cell>& cells) :
    cells(cells), minimumSinr(std::numeric_limits<double>::infinity()), maximumSinr(0), meanSinr(0)
{
    if (cells.empty())
        throw cRuntimeError("Materialized spatial reception requires at least one cell");
    long double weightedSum = 0;
    long double totalWeight = 0;
    for (const auto& cell : cells) {
        const auto& resource = cell.getResource();
        const long double weight = (long double)(resource.getEndTime() - resource.getStartTime()).dbl() *
            (long double)(resource.getUpperFrequency() - resource.getLowerFrequency()).get();
        if (!std::isfinite((double)weight) || weight <= 0)
            throw cRuntimeError("Materialized spatial cell has invalid integration weight");
        minimumSinr = std::min(minimumSinr, cell.getScalarSinr());
        maximumSinr = std::max(maximumSinr, cell.getScalarSinr());
        weightedSum += weight * cell.getScalarSinr();
        totalWeight += weight;
    }
    meanSinr = (double)(weightedSum / totalWeight);
    if (!std::isfinite(minimumSinr) || !std::isfinite(maximumSinr) || !std::isfinite(meanSinr))
        throw cRuntimeError("Materialized spatial SINR summary is not finite");
}

const MaterializedSpatialReception::Cell *MaterializedSpatialReception::findCell(
    simtime_t time, Hz frequency) const
{
    for (const auto& cell : cells)
        if (cell.getResource().contains(time, frequency))
            return &cell;
    return nullptr;
}

double MaterializedSpatialReception::getMinimumSinr(IRadioSignal::SignalPart part) const
{
    return part == IRadioSignal::SIGNAL_PART_WHOLE ? minimumSinr : summarize(cells, part).minimum;
}

double MaterializedSpatialReception::getMaximumSinr(IRadioSignal::SignalPart part) const
{
    return part == IRadioSignal::SIGNAL_PART_WHOLE ? maximumSinr : summarize(cells, part).maximum;
}

double MaterializedSpatialReception::getMeanSinr(IRadioSignal::SignalPart part) const
{
    if (part == IRadioSignal::SIGNAL_PART_WHOLE)
        return meanSinr;
    const auto summary = summarize(cells, part);
    return (double)(summary.weightedSum / summary.totalWeight);
}

bool MaterializedSpatialReception::allRequiredOutputPowersMeet(
    IRadioSignal::SignalPart part, W sensitivity) const
{
    if (std::isnan(sensitivity.get()))
        return true;
    if (!std::isfinite(sensitivity.get()) || sensitivity < W(0))
        throw cRuntimeError("Decoded sensitivity must be finite and nonnegative");

    bool found = false;
    simtime_t currentStart = SIMTIME_ZERO;
    simtime_t currentEnd = SIMTIME_ZERO;
    std::vector<long double> powers;
    auto finishTimeInterval = [&] {
        if (powers.empty())
            return false;
        for (const auto power : powers)
            if (!std::isfinite((double)power) || power < sensitivity.get())
                return false;
        return true;
    };

    for (const auto& cell : cells) {
        if (!matchesPart(cell, part))
            continue;
        found = true;
        const auto& resource = cell.getResource();
        if (powers.empty() || resource.getStartTime() != currentStart || resource.getEndTime() != currentEnd) {
            if (!powers.empty() && !finishTimeInterval())
                return false;
            currentStart = resource.getStartTime();
            currentEnd = resource.getEndTime();
            powers.clear();
        }
        const auto& detection = cell.getDetectionResult();
        if (detection.getStatus() != ChannelMatrixDetectionStatus::SUCCESS ||
            detection.getDesiredSignalPowerSpectralDensities().empty())
            return false;
        const auto& desired = detection.getDesiredSignalPowerSpectralDensities();
        if (powers.empty())
            powers.resize(desired.size(), 0);
        else if (powers.size() != desired.size())
            throw cRuntimeError("Materialized spatial stream count changes within one time interval");
        const long double bandwidth =
            (long double)(resource.getUpperFrequency() - resource.getLowerFrequency()).get();
        for (size_t stream = 0; stream < desired.size(); stream++) {
            const double psd = desired[stream].get<WpHz>();
            if (!std::isfinite(psd) || psd < 0)
                throw cRuntimeError("Materialized desired output PSD must be finite and nonnegative");
            powers[stream] += (long double)psd * bandwidth;
        }
    }
    return found && finishTimeInterval();
}

} // namespace physicallayer
} // namespace inet
