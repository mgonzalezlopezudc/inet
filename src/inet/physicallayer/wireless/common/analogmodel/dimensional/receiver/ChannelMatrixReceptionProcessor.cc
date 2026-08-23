//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixReceptionProcessor.h"

#include <algorithm>
#include <limits>

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MaximumSinrCombiner.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MaximumRatioCombiner.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MinimumMeanSquareErrorSpatialStreamDetector.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/PerfectCancellationSuccessiveInterferenceCancellationSpatialStreamDetector.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ReceiveAntennaSelection.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/SelectionCombiner.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ZeroForcingSpatialStreamDetector.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"

namespace inet {
namespace physicallayer {

Define_Module(ChannelMatrixReceptionProcessor);

namespace {

std::vector<int> allRows(int numberOfRows)
{
    std::vector<int> result(numberOfRows);
    for (int row = 0; row < numberOfRows; row++)
        result[row] = row;
    return result;
}

ChannelMatrixReceptionProcessor::AntennaSelection parseAntennaSelection(const char *value)
{
    if (!strcmp(value, "all"))
        return ChannelMatrixReceptionProcessor::AntennaSelection::ALL;
    if (!strcmp(value, "fixed"))
        return ChannelMatrixReceptionProcessor::AntennaSelection::FIXED;
    if (!strcmp(value, "optimal"))
        return ChannelMatrixReceptionProcessor::AntennaSelection::OPTIMAL;
    throw cRuntimeError("Unknown receive antenna selection policy '%s'", value);
}

ChannelMatrixReceptionProcessor::OneStreamCombiner parseOneStreamCombiner(const char *value)
{
    if (!strcmp(value, "mrc"))
        return ChannelMatrixReceptionProcessor::OneStreamCombiner::MAXIMUM_RATIO;
    if (!strcmp(value, "selection"))
        return ChannelMatrixReceptionProcessor::OneStreamCombiner::SELECTION;
    if (!strcmp(value, "maximumSinr") || !strcmp(value, "mmse"))
        return ChannelMatrixReceptionProcessor::OneStreamCombiner::MAXIMUM_SINR;
    throw cRuntimeError("Unknown one-stream combiner '%s'", value);
}

ChannelMatrixReceptionProcessor::SpatialStreamDetector parseSpatialStreamDetector(const char *value)
{
    if (!strcmp(value, "zf"))
        return ChannelMatrixReceptionProcessor::SpatialStreamDetector::ZERO_FORCING;
    if (!strcmp(value, "mmse"))
        return ChannelMatrixReceptionProcessor::SpatialStreamDetector::MMSE;
    if (!strcmp(value, "mmseSic"))
        return ChannelMatrixReceptionProcessor::SpatialStreamDetector::MMSE_SIC;
    throw cRuntimeError("Unknown spatial-stream detector '%s'", value);
}

std::vector<int> parseRows(const char *value)
{
    std::vector<int> result;
    for (const auto& token : cStringTokenizer(value).asVector()) {
        char *end = nullptr;
        const long row = strtol(token.c_str(), &end, 10);
        if (*token.c_str() == '\0' || *end != '\0' || row < 0 || row > std::numeric_limits<int>::max())
            throw cRuntimeError("Invalid fixed receive antenna index '%s'", token.c_str());
        result.push_back((int)row);
    }
    return result;
}

double score(const ChannelMatrixDetectionResult& result)
{
    if (result.getStatus() != ChannelMatrixDetectionStatus::SUCCESS)
        return 0;
    if (result.getSinrs().empty())
        throw cRuntimeError("Successful channel-matrix detection produced no stream SINR");
    return *std::min_element(result.getSinrs().begin(), result.getSinrs().end());
}

ComplexMatrix transformObservationCovariance(const ComplexMatrix& covariance,
    bool conjugateObservation)
{
    if (!conjugateObservation)
        return covariance;
    ComplexMatrix result(covariance.getNumRows(), covariance.getNumColumns());
    for (int row = 0; row < covariance.getNumRows(); row++)
        for (int column = 0; column < covariance.getNumColumns(); column++)
            result.get(row, column) = std::conj(covariance.get(row, column));
    return result;
}

void addBlock(ComplexMatrix& target, int blockRow, int blockColumn,
    const ComplexMatrix& block)
{
    const int blockSize = block.getNumRows();
    if (block.getNumColumns() != blockSize)
        throw cRuntimeError("Augmented covariance block must be square");
    for (int row = 0; row < blockSize; row++)
        for (int column = 0; column < blockSize; column++)
            target.get(blockRow * blockSize + row, blockColumn * blockSize + column) +=
                block.get(row, column);
}

struct CanonicalInterfererCoefficients final
{
    ComplexMatrix direct;
    ComplexMatrix conjugate;
};

CanonicalInterfererCoefficients buildCanonicalInterfererCoefficients(
    const ChannelMatrixReceptionContext::Signal& signal,
    const std::vector<int>& selectedReceiveRows, bool conjugateObservation)
{
    const auto& segment = signal.getSpatialTransmissionSegment();
    const auto& descriptor = *segment.getSpaceTimeCodeDescriptor();
    const auto& slot = descriptor.getSlot(signal.getSpaceTimeCodeSlotIndex());
    const auto selectedChannel = ChannelMatrixAlgebra::selectRows(
        signal.getEffectiveSpaceTimeStreamChannel(), selectedReceiveRows);
    ComplexMatrix direct = ChannelMatrixAlgebra::scale(
        ChannelMatrixAlgebra::multiply(selectedChannel, slot.getDirectCoefficients()),
        std::complex<double>(descriptor.getAmplitudeScale(), 0));
    ComplexMatrix conjugate = ChannelMatrixAlgebra::scale(
        ChannelMatrixAlgebra::multiply(selectedChannel, slot.getConjugateCoefficients()),
        std::complex<double>(descriptor.getAmplitudeScale(), 0));
    if (conjugateObservation) {
        ComplexMatrix transformedDirect(conjugate.getNumRows(), conjugate.getNumColumns());
        ComplexMatrix transformedConjugate(direct.getNumRows(), direct.getNumColumns());
        for (int row = 0; row < direct.getNumRows(); row++)
            for (int column = 0; column < direct.getNumColumns(); column++) {
                transformedDirect.get(row, column) = std::conj(conjugate.get(row, column));
                transformedConjugate.get(row, column) = std::conj(direct.get(row, column));
            }
        direct = std::move(transformedDirect);
        conjugate = std::move(transformedConjugate);
    }
    return {direct, conjugate};
}

const ChannelMatrixReceptionContext::Signal *findCorrelatedSignal(
    const ChannelMatrixReceptionContext& context,
    const ChannelMatrixReceptionContext::Signal& reference)
{
    for (const auto& signal : context.getInterferingSignals())
        if (signal.hasCorrelatedSpaceTimeCodeIdentity() &&
            signal.getTransmissionId() == reference.getTransmissionId() &&
            signal.getSpaceTimeCodeBlockId() == reference.getSpaceTimeCodeBlockId())
            return &signal;
    return nullptr;
}

ComplexMatrix buildFullAugmentedCovariance(
    const std::vector<ChannelMatrixReceptionContext>& slotContexts,
    const SpaceTimeCodeDescriptor& desiredDescriptor,
    const std::vector<int>& selectedReceiveRows)
{
    const int numberOfSlots = slotContexts.size();
    const int numberOfRows = selectedReceiveRows.size();
    ComplexMatrix result(numberOfSlots * numberOfRows, numberOfSlots * numberOfRows);

    for (int slotIndex = 0; slotIndex < numberOfSlots; slotIndex++) {
        const auto& context = slotContexts[slotIndex];
        const bool conjugateObservation = desiredDescriptor.getSlot(slotIndex).isConjugateObservation();
        addBlock(result, slotIndex, slotIndex, transformObservationCovariance(
            ChannelMatrixAlgebra::selectRowsAndColumns(
                context.getBackgroundCovariance(), selectedReceiveRows),
            conjugateObservation));
        for (const auto& signal : context.getInterferingSignals())
            if (!signal.hasCorrelatedSpaceTimeCodeIdentity())
                addBlock(result, slotIndex, slotIndex, transformObservationCovariance(
                    ChannelMatrixAlgebra::selectRowsAndColumns(
                        signal.getReceiveCovariance(), selectedReceiveRows),
                    conjugateObservation));
    }

    for (int firstSlot = 0; firstSlot < numberOfSlots; firstSlot++) {
        const bool firstConjugated = desiredDescriptor.getSlot(firstSlot).isConjugateObservation();
        for (const auto& firstSignal : slotContexts[firstSlot].getInterferingSignals()) {
            if (!firstSignal.hasCorrelatedSpaceTimeCodeIdentity())
                continue;
            const auto first = buildCanonicalInterfererCoefficients(firstSignal,
                selectedReceiveRows, firstConjugated);
            for (int secondSlot = 0; secondSlot < numberOfSlots; secondSlot++) {
                const auto secondSignal = findCorrelatedSignal(slotContexts[secondSlot], firstSignal);
                if (secondSignal == nullptr)
                    continue;
                const bool secondConjugated = desiredDescriptor.getSlot(secondSlot).isConjugateObservation();
                const auto second = buildCanonicalInterfererCoefficients(*secondSignal,
                    selectedReceiveRows, secondConjugated);
                const auto directCovariance = ChannelMatrixAlgebra::multiply(first.direct,
                    ChannelMatrixAlgebra::conjugateTranspose(second.direct));
                const auto conjugateCovariance = ChannelMatrixAlgebra::multiply(first.conjugate,
                    ChannelMatrixAlgebra::conjugateTranspose(second.conjugate));
                addBlock(result, firstSlot, secondSlot,
                    ChannelMatrixAlgebra::add(directCovariance, conjugateCovariance));
            }
        }
    }
    ChannelMatrixAlgebra::validatePositiveDefinite(result,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE,
        "Full augmented interference-plus-noise covariance");
    return result;
}

} // namespace

void ChannelMatrixReceptionProcessor::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        configuration.antennaSelection = parseAntennaSelection(par("antennaSelection"));
        configuration.activeReceiveAntennaCount = par("activeReceiveAntennaCount");
        configuration.fixedReceiveRows = parseRows(par("fixedReceiveAntennaIndices"));
        configuration.oneStreamCombiner = parseOneStreamCombiner(par("oneStreamCombiner"));
        configuration.spatialStreamDetector = parseSpatialStreamDetector(par("spatialStreamDetector"));
        if (configuration.activeReceiveAntennaCount < -1 || configuration.activeReceiveAntennaCount == 0)
            throw cRuntimeError("Active receive antenna count must be -1 (all) or positive");
        if (configuration.antennaSelection == AntennaSelection::FIXED && configuration.fixedReceiveRows.empty())
            throw cRuntimeError("Fixed receive antenna selection requires at least one index");
        if (configuration.antennaSelection == AntennaSelection::OPTIMAL && configuration.activeReceiveAntennaCount <= 0)
            throw cRuntimeError("Optimal receive antenna selection requires a positive active count");
    }
}

void ChannelMatrixReceptionProcessor::handleMessage(cMessage *message)
{
    throw cRuntimeError("ChannelMatrixReceptionProcessor is stateless and does not process messages");
}

ChannelMatrixDetectionResult ChannelMatrixReceptionProcessor::computeForRows(
    const ChannelMatrixReceptionContext& context, const Configuration& configuration,
    const std::vector<int>& selectedReceiveRows)
{
    const auto& desiredSignal = context.getDesiredSignal();
    const auto& segment = desiredSignal.getSpatialTransmissionSegment();
    if (segment.hasSpaceTimeCode()) {
        const auto& descriptor = *segment.getSpaceTimeCodeDescriptor();
        const auto selectedStsChannel = ChannelMatrixAlgebra::selectRows(
            desiredSignal.getEffectiveSpaceTimeStreamChannel(), selectedReceiveRows);
        const auto selectedCovariance = context.getSelectedInterferencePlusNoiseCovariance(selectedReceiveRows);
        const auto augmentedChannel = descriptor.buildCanonicalAugmentedChannel(selectedStsChannel);
        const auto augmentedCovariance = descriptor.buildCanonicalAugmentedCovariance(selectedCovariance);
        std::vector<ChannelMatrixObservationCoordinate> coordinates;
        for (int slot = 0; slot < descriptor.getNumberOfSlots(); slot++)
            for (int row : selectedReceiveRows)
                coordinates.emplace_back(slot, row, descriptor.getSlot(slot).isConjugateObservation());
        return MinimumMeanSquareErrorSpatialStreamDetector::compute(
            augmentedChannel, augmentedCovariance, coordinates);
    }
    const auto effectiveChannel = context.getEffectiveDesiredChannel();
    const auto covariance = context.getInterferencePlusNoiseCovariance();
    if (effectiveChannel.getNumColumns() == 1) {
        switch (configuration.oneStreamCombiner) {
            case OneStreamCombiner::MAXIMUM_RATIO:
                return MaximumRatioCombiner::compute(effectiveChannel, covariance, selectedReceiveRows);
            case OneStreamCombiner::SELECTION:
                return SelectionCombiner::compute(effectiveChannel, covariance, selectedReceiveRows);
            case OneStreamCombiner::MAXIMUM_SINR:
                return MaximumSinrCombiner::compute(effectiveChannel, covariance, selectedReceiveRows);
        }
    }
    switch (configuration.spatialStreamDetector) {
        case SpatialStreamDetector::ZERO_FORCING:
            return ZeroForcingSpatialStreamDetector::compute(effectiveChannel, covariance, selectedReceiveRows);
        case SpatialStreamDetector::MMSE:
            return MinimumMeanSquareErrorSpatialStreamDetector::compute(effectiveChannel, covariance, selectedReceiveRows);
        case SpatialStreamDetector::MMSE_SIC:
            return PerfectCancellationSuccessiveInterferenceCancellationSpatialStreamDetector::compute(
                effectiveChannel, covariance, selectedReceiveRows);
    }
    throw cRuntimeError("Invalid channel-matrix receiver strategy configuration");
}

ChannelMatrixDetectionResult ChannelMatrixReceptionProcessor::compute(
    const ChannelMatrixReceptionContext& context, const Configuration& configuration)
{
    const int numberOfRows = context.getDesiredSignal().getResponse().getNumRows();
    std::vector<int> selectedReceiveRows;
    switch (configuration.antennaSelection) {
        case AntennaSelection::ALL:
            if (configuration.activeReceiveAntennaCount != -1 &&
                configuration.activeReceiveAntennaCount != numberOfRows)
                throw cRuntimeError("All-antenna selection requires activeReceiveAntennaCount=-1 or the physical receive-row count");
            selectedReceiveRows = allRows(numberOfRows);
            break;
        case AntennaSelection::FIXED:
            selectedReceiveRows = ReceiveAntennaSelection::validateRowSet(
                configuration.fixedReceiveRows, numberOfRows);
            if (configuration.activeReceiveAntennaCount != -1 &&
                configuration.activeReceiveAntennaCount != (int)selectedReceiveRows.size())
                throw cRuntimeError("Fixed receive antenna count does not match activeReceiveAntennaCount");
            break;
        case AntennaSelection::OPTIMAL:
            selectedReceiveRows = ReceiveAntennaSelection::selectBestSubset(numberOfRows,
                configuration.activeReceiveAntennaCount,
                [&] (const std::vector<int>& candidate) {
                    return score(computeForRows(context, configuration, candidate));
                });
            break;
    }
    return computeForRows(context, configuration, selectedReceiveRows);
}

ChannelMatrixDetectionResult ChannelMatrixReceptionProcessor::computeSpaceTimeBlockForRows(
    const std::vector<ChannelMatrixReceptionContext>& slotContexts,
    const Configuration& configuration, const std::vector<int>& selectedReceiveRows)
{
    if (slotContexts.empty())
        throw cRuntimeError("Space-time block processing requires at least one slot context");
    const auto& firstSegment = slotContexts.front().getDesiredSignal().getSpatialTransmissionSegment();
    if (!firstSegment.hasSpaceTimeCode())
        throw cRuntimeError("Space-time block processing requires a descriptor-owned desired segment");
    const auto& descriptor = *firstSegment.getSpaceTimeCodeDescriptor();
    if ((int)slotContexts.size() != descriptor.getNumberOfSlots())
        throw cRuntimeError("Space-time block has %zu contexts instead of %d slots",
            slotContexts.size(), descriptor.getNumberOfSlots());
    std::vector<ComplexMatrix> selectedChannels;
    selectedChannels.reserve(slotContexts.size());
    for (const auto& context : slotContexts) {
        const auto& segment = context.getDesiredSignal().getSpatialTransmissionSegment();
        if (!segment.hasSpaceTimeCode() ||
            segment.getSpaceTimeCodeDescriptor().get() != firstSegment.getSpaceTimeCodeDescriptor().get())
            throw cRuntimeError("All desired space-time slot contexts must share one immutable descriptor");
        selectedChannels.push_back(ChannelMatrixAlgebra::selectRows(
            context.getDesiredSignal().getEffectiveSpaceTimeStreamChannel(), selectedReceiveRows));
    }
    const auto augmentedChannel = descriptor.buildCanonicalAugmentedChannel(selectedChannels);
    const auto augmentedCovariance = buildFullAugmentedCovariance(
        slotContexts, descriptor, selectedReceiveRows);
    std::vector<ChannelMatrixObservationCoordinate> coordinates;
    for (int slot = 0; slot < descriptor.getNumberOfSlots(); slot++)
        for (int row : selectedReceiveRows)
            coordinates.emplace_back(slot, row, descriptor.getSlot(slot).isConjugateObservation());
    return MinimumMeanSquareErrorSpatialStreamDetector::compute(
        augmentedChannel, augmentedCovariance, coordinates);
}

ChannelMatrixDetectionResult ChannelMatrixReceptionProcessor::computeSpaceTimeBlock(
    const std::vector<ChannelMatrixReceptionContext>& slotContexts,
    const Configuration& configuration)
{
    if (slotContexts.empty())
        throw cRuntimeError("Space-time block processing requires at least one slot context");
    const int numberOfRows = slotContexts.front().getDesiredSignal().getResponse().getNumRows();
    std::vector<int> selectedReceiveRows;
    switch (configuration.antennaSelection) {
        case AntennaSelection::ALL:
            if (configuration.activeReceiveAntennaCount != -1 &&
                configuration.activeReceiveAntennaCount != numberOfRows)
                throw cRuntimeError("All-antenna selection requires activeReceiveAntennaCount=-1 or the physical receive-row count");
            selectedReceiveRows = allRows(numberOfRows);
            break;
        case AntennaSelection::FIXED:
            selectedReceiveRows = ReceiveAntennaSelection::validateRowSet(
                configuration.fixedReceiveRows, numberOfRows);
            if (configuration.activeReceiveAntennaCount != -1 &&
                configuration.activeReceiveAntennaCount != (int)selectedReceiveRows.size())
                throw cRuntimeError("Fixed receive antenna count does not match activeReceiveAntennaCount");
            break;
        case AntennaSelection::OPTIMAL:
            selectedReceiveRows = ReceiveAntennaSelection::selectBestSubset(numberOfRows,
                configuration.activeReceiveAntennaCount,
                [&] (const std::vector<int>& candidate) {
                    return score(computeSpaceTimeBlockForRows(slotContexts, configuration, candidate));
                });
            break;
    }
    return computeSpaceTimeBlockForRows(slotContexts, configuration, selectedReceiveRows);
}

} // namespace physicallayer
} // namespace inet
