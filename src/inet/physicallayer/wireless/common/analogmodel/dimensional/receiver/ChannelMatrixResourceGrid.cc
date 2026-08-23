//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixResourceGrid.h"

#include <cmath>

namespace inet {
namespace physicallayer {

ChannelMatrixResourceGrid::Cell::Cell(simtime_t startTime, simtime_t endTime,
    Hz lowerFrequency, Hz upperFrequency, IRadioSignal::SignalPart signalPart,
    double powerSpectralDensityScale) :
    startTime(startTime), endTime(endTime), lowerFrequency(lowerFrequency),
    upperFrequency(upperFrequency), signalPart(signalPart),
    powerSpectralDensityScale(powerSpectralDensityScale)
{
    if (!std::isfinite(startTime.dbl()) || !std::isfinite(endTime.dbl()) || startTime >= endTime)
        throw cRuntimeError("Channel-matrix resource cell must have a finite positive time interval");
    if (!std::isfinite(lowerFrequency.get()) || !std::isfinite(upperFrequency.get()) ||
        lowerFrequency >= upperFrequency)
        throw cRuntimeError("Channel-matrix resource cell must have a finite positive frequency interval");
    if (signalPart < IRadioSignal::SIGNAL_PART_PREAMBLE || signalPart > IRadioSignal::SIGNAL_PART_DATA)
        throw cRuntimeError("Channel-matrix resource cell has invalid signal part %d", (int)signalPart);
    if (!std::isfinite(powerSpectralDensityScale) || powerSpectralDensityScale <= 0)
        throw cRuntimeError("Channel-matrix resource cell has invalid PSD scale %g",
            powerSpectralDensityScale);
}

bool ChannelMatrixResourceGrid::Cell::contains(simtime_t time, Hz frequency) const
{
    return time >= startTime && time < endTime &&
        frequency >= lowerFrequency && frequency < upperFrequency;
}

ChannelMatrixResourceGrid::ChannelMatrixResourceGrid(const std::vector<Cell>& cells,
    size_t maximumCellCount) : cells(cells)
{
    if (cells.empty())
        throw cRuntimeError("Channel-matrix resource grid requires at least one cell");
    if (maximumCellCount == 0 || cells.size() > maximumCellCount)
        throw cRuntimeError("Channel-matrix resource cell count %zu exceeds configured limit %zu",
            cells.size(), maximumCellCount);
    for (size_t index = 1; index < cells.size(); index++) {
        const auto& previous = cells[index - 1];
        const auto& current = cells[index];
        if (current.getStartTime() < previous.getStartTime() ||
            (current.getStartTime() == previous.getStartTime() &&
             current.getLowerFrequency() < previous.getLowerFrequency()))
            throw cRuntimeError("Channel-matrix resource cells must be time-major/frequency-major ordered");
        if (current.getStartTime() == previous.getStartTime() &&
            current.getLowerFrequency() < previous.getUpperFrequency())
            throw cRuntimeError("Channel-matrix resource cells overlap in frequency");
    }
}

const ChannelMatrixResourceGrid::Cell *ChannelMatrixResourceGrid::findCell(
    simtime_t time, Hz frequency) const
{
    for (const auto& cell : cells)
        if (cell.contains(time, frequency))
            return &cell;
    return nullptr;
}

} // namespace physicallayer
} // namespace inet
