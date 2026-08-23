//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixReceptionMaterializer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixAlgebra.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixReceptionContext.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixResourceGrid.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MaterializedSpatialReception.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixReceptionProcessor.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixReceiver.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReception.h"

namespace inet {
namespace physicallayer {

namespace {

template<typename T>
void sortAndUnique(std::vector<T>& values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

IRadioSignal::SignalPart getSignalPart(const IReception& reception, simtime_t time)
{
    const simtime_t offset = time - reception.getStartTime();
    if (offset < reception.getPreambleDuration())
        return IRadioSignal::SIGNAL_PART_PREAMBLE;
    if (offset < reception.getPreambleDuration() + reception.getHeaderDuration())
        return IRadioSignal::SIGNAL_PART_HEADER;
    return IRadioSignal::SIGNAL_PART_DATA;
}

void addClipped(std::vector<simtime_t>& cuts, simtime_t value, simtime_t lower, simtime_t upper)
{
    if (value > lower && value < upper)
        cuts.push_back(value);
}

void addClipped(std::vector<Hz>& cuts, Hz value, Hz lower, Hz upper)
{
    if (value > lower && value < upper)
        cuts.push_back(value);
}

size_t checkedSubdivisionCount(double requested, const char *dimension)
{
    if (!std::isfinite(requested) || requested < 0 ||
        requested > (double)std::numeric_limits<size_t>::max())
        throw cRuntimeError("Channel-matrix %s refinement count is invalid", dimension);
    return std::max<size_t>(1, (size_t)std::ceil(requested));
}

void refineTimeCuts(std::vector<simtime_t>& cuts, double maximumTemporalFrequency,
    const SpatialTransmissionPlan& desiredPlan, simtime_t receptionStartTime)
{
    sortAndUnique(cuts);
    std::vector<simtime_t> refined;
    for (size_t index = 0; index + 1 < cuts.size(); index++) {
        const simtime_t lower = cuts[index];
        const simtime_t upper = cuts[index + 1];
        refined.push_back(lower);
        const auto& segment = desiredPlan.getSegmentAt(
            (lower + (upper - lower) / 2) - receptionStartTime);
        const size_t subdivisions = segment.hasSpaceTimeCode() &&
            segment.getSpaceTimeCodeSlotDuration() > SIMTIME_ZERO ? 1 :
            checkedSubdivisionCount((upper - lower).dbl() * 20 * maximumTemporalFrequency, "time");
        for (size_t subdivision = 1; subdivision < subdivisions; subdivision++)
            refined.push_back(lower + (upper - lower) * ((double)subdivision / subdivisions));
    }
    refined.push_back(cuts.back());
    cuts = std::move(refined);
}

void preserveDesiredSpaceTimeSlots(std::vector<simtime_t>& cuts,
    const SpatialTransmissionPlan& desiredPlan, simtime_t receptionStartTime)
{
    std::vector<simtime_t> filtered;
    filtered.reserve(cuts.size());
    for (const auto& cut : cuts) {
        bool keep = true;
        const simtime_t offset = cut - receptionStartTime;
        for (const auto& segment : desiredPlan.getSegments()) {
            if (!segment.hasSpaceTimeCode() ||
                segment.getSpaceTimeCodeSlotDuration() == SIMTIME_ZERO ||
                offset <= segment.getStartOffset() || offset >= segment.getEndOffset())
                continue;
            const simtime_t codedOffset = offset - segment.getStartOffset();
            keep = codedOffset.raw() % segment.getSpaceTimeCodeSlotDuration().raw() == 0;
            break;
        }
        if (keep)
            filtered.push_back(cut);
    }
    cuts = std::move(filtered);
}

void refineFrequencyCuts(std::vector<Hz>& cuts, double maximumExcessDelay)
{
    sortAndUnique(cuts);
    std::vector<Hz> refined;
    for (size_t index = 0; index + 1 < cuts.size(); index++) {
        const Hz lower = cuts[index];
        const Hz upper = cuts[index + 1];
        refined.push_back(lower);
        const size_t subdivisions = checkedSubdivisionCount(
            (upper - lower).get() * 20 * maximumExcessDelay, "frequency");
        for (size_t subdivision = 1; subdivision < subdivisions; subdivision++)
            refined.push_back(lower + (upper - lower) * ((double)subdivision / subdivisions));
    }
    refined.push_back(cuts.back());
    cuts = std::move(refined);
}

size_t checkedCellCount(size_t timeIntervals, size_t frequencyIntervals, size_t maximumCellCount)
{
    if (timeIntervals == 0 || frequencyIntervals == 0)
        throw cRuntimeError("Channel-matrix resource grid has an empty dimension");
    if (timeIntervals > std::numeric_limits<size_t>::max() / frequencyIntervals)
        throw cRuntimeError("Channel-matrix resource cell count overflows");
    const size_t count = timeIntervals * frequencyIntervals;
    if (maximumCellCount == 0 || count > maximumCellCount)
        throw cRuntimeError("Channel-matrix resource cell count %zu exceeds configured limit %zu",
            count, maximumCellCount);
    return count;
}

ChannelMatrixResourceGrid buildContinuumGrid(const IReception& reception,
    const ChannelMatrixReceptionAnalogModel& desired, const ChannelMatrixNoise& noise,
    size_t maximumCellCount)
{
    const simtime_t startTime = reception.getStartTime();
    const simtime_t endTime = reception.getEndTime();
    const Hz lowerFrequency = desired.getCenterFrequency() - desired.getBandwidth() / 2;
    const Hz upperFrequency = desired.getCenterFrequency() + desired.getBandwidth() / 2;
    std::vector<simtime_t> timeCuts = {startTime, endTime};
    addClipped(timeCuts, startTime + reception.getPreambleDuration(), startTime, endTime);
    addClipped(timeCuts, startTime + reception.getPreambleDuration() + reception.getHeaderDuration(),
        startTime, endTime);
    for (const auto& segment : desired.getSpatialTransmissionPlan()->getSegments()) {
        addClipped(timeCuts, startTime + segment.getStartOffset(), startTime, endTime);
        addClipped(timeCuts, startTime + segment.getEndOffset(), startTime, endTime);
        if (segment.hasSpaceTimeCode() && segment.getSpaceTimeCodeSlotDuration() > SIMTIME_ZERO)
            for (simtime_t offset = segment.getStartOffset() + segment.getSpaceTimeCodeSlotDuration();
                 offset < segment.getEndOffset(); offset += segment.getSpaceTimeCodeSlotDuration())
                addClipped(timeCuts, startTime + offset, startTime, endTime);
    }
    std::vector<Hz> frequencyCuts = {lowerFrequency, upperFrequency};
    for (const auto& interferer : noise.getInterferers()) {
        addClipped(timeCuts, interferer.startTime, startTime, endTime);
        addClipped(timeCuts, interferer.endTime, startTime, endTime);
        for (const auto& segment : interferer.spatialTransmissionPlan->getSegments()) {
            addClipped(timeCuts, interferer.startTime + segment.getStartOffset(), startTime, endTime);
            addClipped(timeCuts, interferer.startTime + segment.getEndOffset(), startTime, endTime);
            if (segment.hasSpaceTimeCode() && segment.getSpaceTimeCodeSlotDuration() > SIMTIME_ZERO)
                for (simtime_t offset = segment.getStartOffset() + segment.getSpaceTimeCodeSlotDuration();
                     offset < segment.getEndOffset(); offset += segment.getSpaceTimeCodeSlotDuration())
                    addClipped(timeCuts, interferer.startTime + offset, startTime, endTime);
        }
        addClipped(frequencyCuts, interferer.centerFrequency - interferer.bandwidth / 2,
            lowerFrequency, upperFrequency);
        addClipped(frequencyCuts, interferer.centerFrequency + interferer.bandwidth / 2,
            lowerFrequency, upperFrequency);
    }
    double maximumTemporalFrequency = desired.getSnapshot()->getActualMaximumTemporalFrequency().get();
    double maximumExcessDelay = desired.getSnapshot()->getMaximumExcessDelay().dbl();
    for (const auto& interferer : noise.getInterferers()) {
        maximumTemporalFrequency = std::max(maximumTemporalFrequency,
            interferer.snapshot->getActualMaximumTemporalFrequency().get());
        maximumExcessDelay = std::max(maximumExcessDelay,
            interferer.snapshot->getMaximumExcessDelay().dbl());
    }
    if (!std::isfinite(maximumTemporalFrequency) || maximumTemporalFrequency < 0 ||
        !std::isfinite(maximumExcessDelay) || maximumExcessDelay < 0)
        throw cRuntimeError("Channel-matrix snapshot sampling metadata is invalid");
    preserveDesiredSpaceTimeSlots(timeCuts, *desired.getSpatialTransmissionPlan(), startTime);
    refineTimeCuts(timeCuts, maximumTemporalFrequency,
        *desired.getSpatialTransmissionPlan(), startTime);
    refineFrequencyCuts(frequencyCuts, maximumExcessDelay);
    const size_t numberOfCells = checkedCellCount(timeCuts.size() - 1,
        frequencyCuts.size() - 1, maximumCellCount);
    std::vector<ChannelMatrixResourceGrid::Cell> cells;
    cells.reserve(numberOfCells);
    for (size_t timeIndex = 0; timeIndex + 1 < timeCuts.size(); timeIndex++) {
        const simtime_t centerTime = timeCuts[timeIndex] +
            (timeCuts[timeIndex + 1] - timeCuts[timeIndex]) / 2;
        const auto signalPart = getSignalPart(reception, centerTime);
        for (size_t frequencyIndex = 0; frequencyIndex + 1 < frequencyCuts.size(); frequencyIndex++)
            cells.emplace_back(timeCuts[timeIndex], timeCuts[timeIndex + 1],
                frequencyCuts[frequencyIndex], frequencyCuts[frequencyIndex + 1], signalPart);
    }
    return ChannelMatrixResourceGrid(cells, maximumCellCount);
}

ChannelMatrixResourceGrid buildTechnologyGrid(const IReception& reception,
    const ChannelMatrixReceptionAnalogModel& desired, const ChannelMatrixNoise& noise,
    const std::vector<ChannelMatrixResourceCell>& resources, size_t maximumCellCount)
{
    const simtime_t startTime = reception.getStartTime();
    const simtime_t duration = reception.getDuration();
    const Hz centerFrequency = desired.getCenterFrequency();
    const Hz lowerSignalFrequency = centerFrequency - desired.getBandwidth() / 2;
    const Hz upperSignalFrequency = centerFrequency + desired.getBandwidth() / 2;
    std::vector<ChannelMatrixResourceGrid::Cell> cells;
    for (const auto& resource : resources) {
        if (resource.getStartOffset() < SIMTIME_ZERO ||
            resource.getStartOffset() >= resource.getEndOffset() ||
            resource.getEndOffset() > duration)
            throw cRuntimeError("Technology-supplied channel-matrix resource has an invalid PPDU-relative time interval");
        const Hz lowerFrequency = centerFrequency + resource.getLowerBasebandFrequency();
        const Hz upperFrequency = centerFrequency + resource.getUpperBasebandFrequency();
        if (lowerFrequency < lowerSignalFrequency || upperFrequency > upperSignalFrequency ||
            lowerFrequency >= upperFrequency)
            throw cRuntimeError("Technology-supplied channel-matrix resource lies outside the desired signal band");

        const simtime_t absoluteStart = startTime + resource.getStartOffset();
        const simtime_t absoluteEnd = startTime + resource.getEndOffset();
        const auto& desiredSegment = desired.getSpatialTransmissionPlan()->getSegmentAt(
            resource.getStartOffset() +
            (resource.getEndOffset() - resource.getStartOffset()) / 2);
        std::vector<simtime_t> timeCuts = {absoluteStart, absoluteEnd};
        if (!desiredSegment.hasSpaceTimeCode()) {
            for (const auto& interferer : noise.getInterferers()) {
                addClipped(timeCuts, interferer.startTime, absoluteStart, absoluteEnd);
                addClipped(timeCuts, interferer.endTime, absoluteStart, absoluteEnd);
                for (const auto& segment : interferer.spatialTransmissionPlan->getSegments()) {
                    addClipped(timeCuts, interferer.startTime + segment.getStartOffset(), absoluteStart, absoluteEnd);
                    addClipped(timeCuts, interferer.startTime + segment.getEndOffset(), absoluteStart, absoluteEnd);
                    if (segment.hasSpaceTimeCode() && segment.getSpaceTimeCodeSlotDuration() > SIMTIME_ZERO)
                        for (simtime_t offset = segment.getStartOffset() + segment.getSpaceTimeCodeSlotDuration();
                             offset < segment.getEndOffset(); offset += segment.getSpaceTimeCodeSlotDuration())
                            addClipped(timeCuts, interferer.startTime + offset, absoluteStart, absoluteEnd);
                }
            }
        }
        sortAndUnique(timeCuts);
        std::vector<Hz> frequencyCuts = {lowerFrequency, upperFrequency};
        for (const auto& interferer : noise.getInterferers()) {
            addClipped(frequencyCuts, interferer.centerFrequency - interferer.bandwidth / 2,
                lowerFrequency, upperFrequency);
            addClipped(frequencyCuts, interferer.centerFrequency + interferer.bandwidth / 2,
                lowerFrequency, upperFrequency);
        }
        sortAndUnique(frequencyCuts);
        if (maximumCellCount == 0 || cells.size() >= maximumCellCount)
            throw cRuntimeError("Technology-supplied channel-matrix resource grid exceeds configured limit %zu",
                maximumCellCount);
        checkedCellCount(timeCuts.size() - 1, frequencyCuts.size() - 1,
            maximumCellCount - cells.size());
        for (size_t timeIndex = 0; timeIndex + 1 < timeCuts.size(); timeIndex++)
            for (size_t frequencyIndex = 0; frequencyIndex + 1 < frequencyCuts.size(); frequencyIndex++)
                cells.emplace_back(timeCuts[timeIndex], timeCuts[timeIndex + 1],
                    frequencyCuts[frequencyIndex], frequencyCuts[frequencyIndex + 1],
                    resource.getSignalPart(), resource.getPowerSpectralDensityScale());
    }
    std::sort(cells.begin(), cells.end(), [] (const auto& left, const auto& right) {
        return left.getStartTime() < right.getStartTime() ||
            (left.getStartTime() == right.getStartTime() &&
             left.getLowerFrequency() < right.getLowerFrequency());
    });
    return ChannelMatrixResourceGrid(cells, maximumCellCount);
}

ChannelMatrixResourceGrid buildGrid(const IReception& reception,
    const ChannelMatrixReceptionAnalogModel& desired, const ChannelMatrixNoise& noise,
    const IChannelMatrixReceiver& receiver)
{
    const auto resources = receiver.getChannelMatrixResourceCells(reception);
    return resources.empty() ?
        buildContinuumGrid(reception, desired, noise,
            receiver.getMaximumMaterializedResourceCells()) :
        buildTechnologyGrid(reception, desired, noise, resources,
            receiver.getMaximumMaterializedResourceCells());
}

ChannelMatrixReceptionContext::Signal resolveDesiredSignal(
    const ChannelMatrixReceptionAnalogModel& desired, simtime_t receptionStartTime,
    simtime_t time, Hz frequency, double powerSpectralDensityScale)
{
    const auto& segment = desired.getSpatialTransmissionPlan()->getSegmentAt(
        time - receptionStartTime, SpatialTransmissionPlan::BoundarySide::RIGHT_LIMIT);
    const auto response = desired.getSnapshot()->getResponse(time, frequency);
    const auto fullBandPower = desired.getDeterministicLargeScalePower()->getValue(
        Point<simsec, Hz>(simsec(time), frequency));
    const WpHz occupiedResourcePower(fullBandPower.get<WpHz>() * powerSpectralDensityScale);
    return ChannelMatrixReceptionContext::Signal(response, segment, occupiedResourcePower,
        frequency - desired.getCenterFrequency());
}

std::optional<double> getActivePowerSpectralDensityScale(
    const ChannelMatrixNoise::Interferer& interferer, simtime_t time, Hz frequency)
{
    if (time < interferer.startTime || time >= interferer.endTime ||
        frequency < interferer.centerFrequency - interferer.bandwidth / 2 ||
        frequency >= interferer.centerFrequency + interferer.bandwidth / 2)
        return std::nullopt;
    if (interferer.resourceCells.empty())
        return 1;
    const simtime_t offset = time - interferer.startTime;
    const Hz basebandFrequency = frequency - interferer.centerFrequency;
    for (const auto& resource : interferer.resourceCells)
        if (resource.contains(offset, basebandFrequency))
            return resource.getPowerSpectralDensityScale();
    return std::nullopt;
}

ChannelMatrixReceptionContext::Signal resolveInterfererSignal(
    const ChannelMatrixNoise::Interferer& interferer, simtime_t time, Hz frequency,
    double powerSpectralDensityScale)
{
    const simtime_t relativeTime = time - interferer.startTime;
    const auto& segment = interferer.spatialTransmissionPlan->getSegmentAt(
        relativeTime, SpatialTransmissionPlan::BoundarySide::RIGHT_LIMIT);
    const auto response = interferer.snapshot->getResponse(time, frequency);
    const auto fullBandPower = interferer.deterministicLargeScalePower->getValue(
        Point<simsec, Hz>(simsec(time), frequency));
    const WpHz occupiedResourcePower(fullBandPower.get<WpHz>() * powerSpectralDensityScale);
    if (segment.hasSpaceTimeCode() && segment.getSpaceTimeCodeSlotDuration() > SIMTIME_ZERO) {
        const simtime_t codedOffset = relativeTime - segment.getStartOffset();
        const int64_t slotOrdinal = codedOffset.raw() / segment.getSpaceTimeCodeSlotDuration().raw();
        const int numberOfSlots = segment.getSpaceTimeCodeDescriptor()->getNumberOfSlots();
        const int slotIndex = slotOrdinal % numberOfSlots;
        const int64_t blockStartOffset = segment.getStartOffset().raw() +
            (slotOrdinal - slotIndex) * segment.getSpaceTimeCodeSlotDuration().raw();
        return ChannelMatrixReceptionContext::Signal(response, segment, occupiedResourcePower,
            frequency - interferer.centerFrequency, interferer.transmissionId,
            blockStartOffset, slotIndex);
    }
    return ChannelMatrixReceptionContext::Signal(response, segment, occupiedResourcePower,
        frequency - interferer.centerFrequency);
}

} // namespace

std::shared_ptr<const MaterializedSpatialReception> ChannelMatrixReceptionMaterializer::materialize(
    const IReception& reception, const ChannelMatrixNoise& noise,
    const IChannelMatrixReceiver& receiver)
{
    const auto desired = check_and_cast<const ChannelMatrixReceptionAnalogModel *>(reception.getAnalogModel());
    const auto processor = receiver.getChannelMatrixReceptionProcessor();
    if (processor == nullptr)
        throw cRuntimeError("Channel-matrix materialization requires a configured reception processor");
    const auto grid = buildGrid(reception, *desired, noise, receiver);
    auto makeContext = [&] (const ChannelMatrixResourceGrid::Cell& cell) {
        const simtime_t time = cell.getCenterTime();
        const Hz frequency = cell.getCenterFrequency();
        auto desiredSignal = resolveDesiredSignal(*desired, reception.getStartTime(), time, frequency,
            cell.getPowerSpectralDensityScale());
        std::vector<ChannelMatrixReceptionContext::Signal> interferingSignals;
        for (const auto& interferer : noise.getInterferers()) {
            const auto powerSpectralDensityScale =
                getActivePowerSpectralDensityScale(interferer, time, frequency);
            if (powerSpectralDensityScale.has_value())
                interferingSignals.push_back(resolveInterfererSignal(interferer, time, frequency,
                    *powerSpectralDensityScale));
        }
        const double backgroundPower = noise.getCompatibilityBackgroundPower()->getValue(
            Point<simsec, Hz>(simsec(time), frequency)).get<WpHz>();
        if (!std::isfinite(backgroundPower) || backgroundPower <= 0)
            throw cRuntimeError("Matrix receiver background PSD must be finite and positive, got %g", backgroundPower);
        const auto backgroundCovariance = ChannelMatrixAlgebra::scale(
            ChannelMatrixAlgebra::identity(desiredSignal.getResponse().getNumRows()),
            std::complex<double>(backgroundPower, 0));
        const ChannelMatrixReceptionContext context(desiredSignal, interferingSignals,
            backgroundCovariance, time, frequency, cell.getSignalPart());
        return context;
    };

    const auto& resourceCells = grid.getCells();
    std::vector<std::optional<ChannelMatrixDetectionResult>> results(resourceCells.size());
    for (size_t index = 0; index < resourceCells.size(); index++) {
        if (results[index].has_value())
            continue;
        const auto context = makeContext(resourceCells[index]);
        const auto& segment = context.getDesiredSignal().getSpatialTransmissionSegment();
        if (!segment.hasSpaceTimeCode() || segment.getSpaceTimeCodeSlotDuration() == SIMTIME_ZERO) {
            results[index] = processor->compute(context);
            continue;
        }
        const auto& descriptor = *segment.getSpaceTimeCodeDescriptor();
        const simtime_t relativeTime = resourceCells[index].getCenterTime() - reception.getStartTime() -
            segment.getStartOffset();
        const int64_t slotOrdinal = relativeTime.raw() / segment.getSpaceTimeCodeSlotDuration().raw();
        const int slotIndex = slotOrdinal % descriptor.getNumberOfSlots();
        if (slotIndex != 0)
            throw cRuntimeError("Space-time resource grid does not begin at a complete code block");
        if (resourceCells[index].getEndTime() - resourceCells[index].getStartTime() !=
            segment.getSpaceTimeCodeSlotDuration())
            throw cRuntimeError("Space-time resource cell does not cover exactly one code slot");
        std::vector<ChannelMatrixReceptionContext> slotContexts;
        std::vector<size_t> slotCellIndices;
        for (int slot = 0; slot < descriptor.getNumberOfSlots(); slot++) {
            const simtime_t expectedStart = resourceCells[index].getStartTime() +
                slot * segment.getSpaceTimeCodeSlotDuration();
            const simtime_t expectedEnd = expectedStart + segment.getSpaceTimeCodeSlotDuration();
            const auto iterator = std::lower_bound(resourceCells.begin(), resourceCells.end(), expectedStart,
                [] (const ChannelMatrixResourceGrid::Cell& cell, simtime_t time) {
                    return cell.getStartTime() < time;
                });
            auto matching = iterator;
            while (matching != resourceCells.end() && matching->getStartTime() == expectedStart &&
                matching->getLowerFrequency() < resourceCells[index].getLowerFrequency())
                matching++;
            if (matching == resourceCells.end() || matching->getStartTime() != expectedStart ||
                matching->getEndTime() != expectedEnd ||
                matching->getLowerFrequency() != resourceCells[index].getLowerFrequency() ||
                matching->getUpperFrequency() != resourceCells[index].getUpperFrequency())
                throw cRuntimeError("Space-time resource grid ends inside a code block or changes frequency resources");
            const size_t slotCellIndex = matching - resourceCells.begin();
            const auto& slotCell = resourceCells[slotCellIndex];
            slotContexts.push_back(makeContext(slotCell));
            slotCellIndices.push_back(slotCellIndex);
        }
        const auto detection = processor->computeSpaceTimeBlock(slotContexts);
        for (const auto slotCellIndex : slotCellIndices)
            results[slotCellIndex] = detection;
    }
    std::vector<MaterializedSpatialReception::Cell> cells;
    cells.reserve(resourceCells.size());
    for (size_t index = 0; index < resourceCells.size(); index++) {
        if (!results[index].has_value())
            throw cRuntimeError("Channel-matrix materialization left an empty resource cell");
        cells.emplace_back(resourceCells[index], *results[index]);
    }
    return std::make_shared<const MaterializedSpatialReception>(cells);
}

} // namespace physicallayer
} // namespace inet
