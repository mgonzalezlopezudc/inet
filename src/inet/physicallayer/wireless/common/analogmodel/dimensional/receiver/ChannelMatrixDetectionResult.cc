//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

#include <algorithm>
#include <cmath>

namespace inet {
namespace physicallayer {

namespace {

constexpr double WEIGHT_NORM_TOLERANCE = 1e-12;

void validateStatus(ChannelMatrixDetectionStatus status)
{
    switch (status) {
        case ChannelMatrixDetectionStatus::SUCCESS:
        case ChannelMatrixDetectionStatus::UNDERDETERMINED:
        case ChannelMatrixDetectionStatus::RANK_DEFICIENT:
        case ChannelMatrixDetectionStatus::ILL_CONDITIONED:
        case ChannelMatrixDetectionStatus::UNSUPPORTED_LAYOUT:
            return;
    }
    throw cRuntimeError("Unknown channel-matrix detection status");
}

void validateSelectedRows(const std::vector<int>& rows)
{
    if (rows.empty())
        throw cRuntimeError("Detection result requires a nonempty receive-row set");
    int previous = -1;
    for (int row : rows) {
        if (row < 0)
            throw cRuntimeError("Detection result receive row %d is negative", row);
        if (row <= previous)
            throw cRuntimeError("Detection result receive rows must be strictly increasing and unique");
        previous = row;
    }
}

std::vector<ChannelMatrixObservationCoordinate> synthesizeObservationCoordinates(const std::vector<int>& rows)
{
    std::vector<ChannelMatrixObservationCoordinate> result;
    result.reserve(rows.size());
    for (int row : rows)
        result.emplace_back(0, row, false);
    return result;
}

std::vector<int> physicalRowsFromObservationCoordinates(
    const std::vector<ChannelMatrixObservationCoordinate>& coordinates)
{
    std::vector<int> result;
    result.reserve(coordinates.size());
    for (const auto& coordinate : coordinates)
        result.push_back(coordinate.getReceiveRowIndex());
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

void validateObservationCoordinates(const std::vector<ChannelMatrixObservationCoordinate>& coordinates,
    int expectedRows, const char *operation)
{
    if (coordinates.empty())
        throw cRuntimeError("%s requires a nonempty observation-coordinate set", operation);
    if ((int)coordinates.size() != expectedRows)
        throw cRuntimeError("%s coordinate count %zu does not match observation-row count %d",
            operation, coordinates.size(), expectedRows);
    for (size_t i = 0; i < coordinates.size(); i++) {
        const auto& coordinate = coordinates[i];
        if (coordinate.getSlotIndex() < 0 || coordinate.getReceiveRowIndex() < 0)
            throw cRuntimeError("%s coordinates must have nonnegative slot and receive-row indices", operation);
        for (size_t j = 0; j < i; j++)
            if (coordinates[j] == coordinate)
                throw cRuntimeError("%s contains duplicate observation coordinate", operation);
    }
}

void validateDetectionOrder(const std::optional<std::vector<int>>& order, int numberOfStreams)
{
    if (!order)
        return;
    if (order->size() != (size_t)numberOfStreams)
        throw cRuntimeError("Detection order must contain one entry per stream");
    std::vector<bool> seen(numberOfStreams, false);
    for (int stream : *order) {
        if (stream < 0 || stream >= numberOfStreams)
            throw cRuntimeError("Detection-order stream %d is outside [0,%d)", stream, numberOfStreams);
        if (seen[stream])
            throw cRuntimeError("Detection order must contain unique stream indices");
        seen[stream] = true;
    }
}

void validatePhysicalPowers(const std::vector<WpHz>& values, int expectedCount, const char *name)
{
    if ((int)values.size() != expectedCount)
        throw cRuntimeError("%s count %zu does not match stream count %d", name, values.size(), expectedCount);
    for (const auto& value : values) {
        const double numericValue = value.get<WpHz>();
        if (!std::isfinite(numericValue) || numericValue < 0)
            throw cRuntimeError("%s must contain finite nonnegative WpHz values", name);
    }
}

void validateSinrs(const std::vector<double>& values, int expectedCount)
{
    if ((int)values.size() != expectedCount)
        throw cRuntimeError("SINR count %zu does not match stream count %d", values.size(), expectedCount);
    for (double value : values)
        if (!std::isfinite(value) || value < 0)
            throw cRuntimeError("SINR must contain finite nonnegative values");
}

ComplexMatrix normalizeRowsWithDeterministicBasis(const ComplexMatrix& input)
{
    ComplexMatrix result(input.getNumRows(), input.getNumColumns());
    for (int row = 0; row < input.getNumRows(); row++) {
        double squaredNorm = 0;
        for (int column = 0; column < input.getNumColumns(); column++)
            squaredNorm += std::norm(input.get(row, column));
        if (!std::isfinite(squaredNorm))
            throw cRuntimeError("Detection weights contain a non-finite row norm");
        const double norm = std::sqrt(squaredNorm);
        if (norm > 0) {
            for (int column = 0; column < input.getNumColumns(); column++)
                result.get(row, column) = input.get(row, column) / norm;
        }
        else
            // A zero canonical row is still a valid successful detector row:
            // choose a deterministic unit basis vector without changing its
            // zero desired or cross-stream response.
            result.get(row, row % input.getNumColumns()) = 1;
    }
    return result;
}

void validateUnitRows(const ComplexMatrix& weights)
{
    for (int row = 0; row < weights.getNumRows(); row++) {
        double squaredNorm = 0;
        for (int column = 0; column < weights.getNumColumns(); column++)
            squaredNorm += std::norm(weights.get(row, column));
        const double norm = std::sqrt(squaredNorm);
        if (!std::isfinite(norm) || norm <= 0 || std::abs(norm - 1) > WEIGHT_NORM_TOLERANCE)
            throw cRuntimeError("Successful detection weights must have unit-norm rows");
    }
}

} // namespace

ChannelMatrixDetectionResult::ChannelMatrixDetectionResult(ChannelMatrixDetectionStatus status,
    const std::vector<int>& selectedReceiveRows, const ComplexMatrix& weights,
    const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates,
    const std::vector<WpHz>& desiredSignalPowerSpectralDensities,
    const std::vector<WpHz>& crossStreamResidualPowerSpectralDensities,
    const std::vector<WpHz>& projectedNoiseAndInterferencePowerSpectralDensities,
    const std::vector<double>& sinrs, const std::optional<std::vector<int>>& detectionOrder) :
    status(status), selectedReceiveRows(selectedReceiveRows), observationCoordinates(observationCoordinates), weights(weights),
    desiredSignalPowerSpectralDensities(desiredSignalPowerSpectralDensities),
    crossStreamResidualPowerSpectralDensities(crossStreamResidualPowerSpectralDensities),
    projectedNoiseAndInterferencePowerSpectralDensities(projectedNoiseAndInterferencePowerSpectralDensities),
    sinrs(sinrs), detectionOrder(detectionOrder)
{
}

ChannelMatrixObservationCoordinate::ChannelMatrixObservationCoordinate(int slotIndex, int receiveRowIndex, bool conjugated) :
    slotIndex(slotIndex), receiveRowIndex(receiveRowIndex), conjugated(conjugated)
{
    if (slotIndex < 0 || receiveRowIndex < 0)
        throw cRuntimeError("Channel-matrix observation coordinates require nonnegative slot and receive-row indices");
}

ChannelMatrixDetectionResult ChannelMatrixDetectionResult::fromCanonicalWeights(ChannelMatrixDetectionStatus status,
    const std::vector<int>& selectedReceiveRows, const ComplexMatrix& effectiveChannel,
    const ComplexMatrix& projectedCovariance, const ComplexMatrix& canonicalWeights,
    const std::optional<std::vector<int>>& detectionOrder)
{
    validateSelectedRows(selectedReceiveRows);
    if ((int)selectedReceiveRows.size() != effectiveChannel.getNumRows())
        throw cRuntimeError("Detection selected-row count must equal effective-channel row count");
    return fromCanonicalWeights(status, synthesizeObservationCoordinates(selectedReceiveRows), effectiveChannel,
        projectedCovariance, canonicalWeights, detectionOrder);
}

ChannelMatrixDetectionResult ChannelMatrixDetectionResult::fromCanonicalWeights(ChannelMatrixDetectionStatus status,
    const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates,
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance,
    const ComplexMatrix& canonicalWeights, const std::optional<std::vector<int>>& detectionOrder)
{
    validateStatus(status);
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "Detection effective channel");
    ChannelMatrixAlgebra::validatePositiveDefinite(projectedCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "Detection projected covariance");
    validateObservationCoordinates(observationCoordinates, effectiveChannel.getNumRows(), "Detection");
    if (projectedCovariance.getNumRows() != effectiveChannel.getNumRows() ||
        projectedCovariance.getNumColumns() != effectiveChannel.getNumRows())
        throw cRuntimeError("Detection projected covariance must match the effective-channel row count");
    const std::vector<int> selectedReceiveRows = physicalRowsFromObservationCoordinates(observationCoordinates);
    ChannelMatrixAlgebra::validateDimensions(canonicalWeights, effectiveChannel.getNumColumns(),
        effectiveChannel.getNumRows(), "Detection canonical weights");
    validateDetectionOrder(detectionOrder, effectiveChannel.getNumColumns());

    if (status != ChannelMatrixDetectionStatus::SUCCESS) {
        const int numberOfStreams = effectiveChannel.getNumColumns();
        const int numberOfRows = effectiveChannel.getNumRows();
        ComplexMatrix zeroWeights(numberOfStreams, numberOfRows);
        std::vector<WpHz> zeroPowers(numberOfStreams, WpHz(0));
        std::vector<double> zeroSinrs(numberOfStreams, 0);
        return ChannelMatrixDetectionResult(status, selectedReceiveRows, zeroWeights, observationCoordinates,
            zeroPowers, zeroPowers, zeroPowers, zeroSinrs, detectionOrder);
    }

    const ComplexMatrix weights = normalizeRowsWithDeterministicBasis(canonicalWeights);
    const ComplexMatrix response = ChannelMatrixAlgebra::multiply(weights, effectiveChannel);
    const ComplexMatrix projected = ChannelMatrixAlgebra::multiply(
        ChannelMatrixAlgebra::multiply(weights, projectedCovariance),
        ChannelMatrixAlgebra::conjugateTranspose(weights));
    std::vector<WpHz> desired(effectiveChannel.getNumColumns(), WpHz(0));
    std::vector<WpHz> residual(effectiveChannel.getNumColumns(), WpHz(0));
    std::vector<WpHz> noise(effectiveChannel.getNumColumns(), WpHz(0));
    std::vector<double> sinrs(effectiveChannel.getNumColumns(), 0);
    for (int stream = 0; stream < effectiveChannel.getNumColumns(); stream++) {
        double desiredValue = std::norm(response.get(stream, stream));
        double residualValue = 0;
        for (int otherStream = 0; otherStream < effectiveChannel.getNumColumns(); otherStream++)
            if (otherStream != stream)
                residualValue += std::norm(response.get(stream, otherStream));
        const std::complex<double> projectedValue = projected.get(stream, stream);
        const double scale = std::abs(projectedValue);
        if (std::abs(projectedValue.imag()) > 1e-9 * scale)
            throw cRuntimeError("Detection projected covariance has a non-real diagonal");
        double noiseValue = projectedValue.real();
        if (!std::isfinite(desiredValue) || !std::isfinite(residualValue) || !std::isfinite(noiseValue) ||
            desiredValue < 0 || residualValue < 0 || noiseValue < 0)
            throw cRuntimeError("Detection physical outputs are not finite and nonnegative");
        // A positive-definite covariance makes noiseValue positive for every
        // unit-norm row, including deterministic basis rows for zero channels.
        const double denominator = residualValue + noiseValue;
        double sinr = denominator > 0 ? desiredValue / denominator : 0;
        if (!std::isfinite(sinr) || sinr < 0)
            throw cRuntimeError("Detection SINR is not finite and nonnegative");
        desired[stream] = WpHz(desiredValue);
        residual[stream] = WpHz(residualValue);
        noise[stream] = WpHz(noiseValue);
        sinrs[stream] = sinr;
    }
    return ChannelMatrixDetectionResult(status, selectedReceiveRows, weights, observationCoordinates,
        desired, residual, noise, sinrs, detectionOrder);
}

ChannelMatrixDetectionResult ChannelMatrixDetectionResult::fromPhysicalOutputs(ChannelMatrixDetectionStatus status,
    const std::vector<int>& selectedReceiveRows, const ComplexMatrix& inputWeights,
    const std::vector<WpHz>& desiredSignalPowerSpectralDensities,
    const std::vector<WpHz>& crossStreamResidualPowerSpectralDensities,
    const std::vector<WpHz>& projectedNoiseAndInterferencePowerSpectralDensities,
    const std::vector<double>& sinrs, const std::optional<std::vector<int>>& detectionOrder)
{
    validateSelectedRows(selectedReceiveRows);
    if ((int)selectedReceiveRows.size() != inputWeights.getNumColumns())
        throw cRuntimeError("Detection selected-row count must equal weight-column count");
    return fromPhysicalOutputs(status, synthesizeObservationCoordinates(selectedReceiveRows), inputWeights,
        desiredSignalPowerSpectralDensities, crossStreamResidualPowerSpectralDensities,
        projectedNoiseAndInterferencePowerSpectralDensities, sinrs, detectionOrder);
}

ChannelMatrixDetectionResult ChannelMatrixDetectionResult::fromPhysicalOutputs(ChannelMatrixDetectionStatus status,
    const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates, const ComplexMatrix& inputWeights,
    const std::vector<WpHz>& desiredSignalPowerSpectralDensities,
    const std::vector<WpHz>& crossStreamResidualPowerSpectralDensities,
    const std::vector<WpHz>& projectedNoiseAndInterferencePowerSpectralDensities,
    const std::vector<double>& sinrs, const std::optional<std::vector<int>>& detectionOrder)
{
    validateStatus(status);
    ChannelMatrixAlgebra::validateFinite(inputWeights, "Detection reported weights");
    validateObservationCoordinates(observationCoordinates, inputWeights.getNumColumns(), "Detection");
    const std::vector<int> selectedReceiveRows = physicalRowsFromObservationCoordinates(observationCoordinates);
    validateDetectionOrder(detectionOrder, inputWeights.getNumRows());
    validatePhysicalPowers(desiredSignalPowerSpectralDensities, inputWeights.getNumRows(), "Desired PSD");
    validatePhysicalPowers(crossStreamResidualPowerSpectralDensities, inputWeights.getNumRows(), "Cross-stream residual PSD");
    validatePhysicalPowers(projectedNoiseAndInterferencePowerSpectralDensities, inputWeights.getNumRows(), "Projected noise/interference PSD");
    validateSinrs(sinrs, inputWeights.getNumRows());

    if (status != ChannelMatrixDetectionStatus::SUCCESS) {
        std::vector<WpHz> zeroPowers(inputWeights.getNumRows(), WpHz(0));
        std::vector<double> zeroSinrs(inputWeights.getNumRows(), 0);
        ComplexMatrix zeroWeights(inputWeights.getNumRows(), inputWeights.getNumColumns());
        return ChannelMatrixDetectionResult(status, selectedReceiveRows, zeroWeights, observationCoordinates,
            zeroPowers, zeroPowers, zeroPowers, zeroSinrs, detectionOrder);
    }

    validateUnitRows(inputWeights);
    return ChannelMatrixDetectionResult(status, selectedReceiveRows, inputWeights, observationCoordinates,
        desiredSignalPowerSpectralDensities, crossStreamResidualPowerSpectralDensities,
        projectedNoiseAndInterferencePowerSpectralDensities, sinrs, detectionOrder);
}

} // namespace physicallayer
} // namespace inet
