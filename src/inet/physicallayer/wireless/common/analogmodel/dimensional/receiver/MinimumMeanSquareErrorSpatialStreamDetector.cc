//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MinimumMeanSquareErrorSpatialStreamDetector.h"

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ReceiveAntennaSelection.h"

namespace inet {
namespace physicallayer {

namespace {

std::vector<ChannelMatrixObservationCoordinate> allObservationCoordinates(int numberOfRows)
{
    std::vector<ChannelMatrixObservationCoordinate> result;
    result.reserve(numberOfRows);
    for (int row = 0; row < numberOfRows; row++)
        result.emplace_back(0, row, false);
    return result;
}

} // namespace

ChannelMatrixDetectionResult MinimumMeanSquareErrorSpatialStreamDetector::compute(
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "MMSE effective channel");
    return compute(effectiveChannel, projectedCovariance, allObservationCoordinates(effectiveChannel.getNumRows()));
}

ChannelMatrixDetectionResult MinimumMeanSquareErrorSpatialStreamDetector::compute(
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance,
    const std::vector<int>& selectedReceiveRows)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "MMSE effective channel");
    if (effectiveChannel.getNumColumns() <= 0)
        throw cRuntimeError("MMSE effective channel must have at least one stream");
    ChannelMatrixAlgebra::validateDimensions(projectedCovariance, effectiveChannel.getNumRows(),
        effectiveChannel.getNumRows(), "MMSE covariance");
    ReceiveAntennaSelection::validateRowSet(selectedReceiveRows, effectiveChannel.getNumRows());
    const ComplexMatrix selectedChannel = ChannelMatrixAlgebra::selectRows(effectiveChannel, selectedReceiveRows);
    const ComplexMatrix selectedCovariance = ChannelMatrixAlgebra::selectRowsAndColumns(
        projectedCovariance, selectedReceiveRows);
    std::vector<ChannelMatrixObservationCoordinate> selectedCoordinates;
    selectedCoordinates.reserve(selectedReceiveRows.size());
    for (int row : selectedReceiveRows)
        selectedCoordinates.emplace_back(0, row, false);
    return compute(selectedChannel, selectedCovariance, selectedCoordinates);
}

ChannelMatrixDetectionResult MinimumMeanSquareErrorSpatialStreamDetector::compute(
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance,
    const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "MMSE effective channel");
    if (effectiveChannel.getNumColumns() <= 0)
        throw cRuntimeError("MMSE effective channel must have at least one stream");
    ChannelMatrixAlgebra::validateDimensions(projectedCovariance, effectiveChannel.getNumRows(),
        effectiveChannel.getNumRows(), "MMSE covariance");
    if (observationCoordinates.empty() || (int)observationCoordinates.size() != effectiveChannel.getNumRows())
        throw cRuntimeError("MMSE observation coordinates must match the nonempty effective-channel rows");
    for (size_t i = 0; i < observationCoordinates.size(); i++) {
        const auto& coordinate = observationCoordinates[i];
        for (size_t j = 0; j < i; j++)
            if (observationCoordinates[j] == coordinate)
                throw cRuntimeError("MMSE observation coordinates must be unique");
    }
    ChannelMatrixAlgebra::validatePositiveDefinite(projectedCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "MMSE covariance");

    const ComplexMatrix channelHermitian = ChannelMatrixAlgebra::conjugateTranspose(effectiveChannel);
    const ComplexMatrix regularized = ChannelMatrixAlgebra::add(
        ChannelMatrixAlgebra::multiply(effectiveChannel, channelHermitian), projectedCovariance);
    const ComplexMatrix factorizedSolution = ChannelMatrixAlgebra::solveHermitianPositiveDefinite(
        regularized, effectiveChannel);
    const ComplexMatrix weights = ChannelMatrixAlgebra::conjugateTranspose(factorizedSolution);
    return ChannelMatrixDetectionResult::fromCanonicalWeights(
        ChannelMatrixDetectionStatus::SUCCESS, observationCoordinates,
        effectiveChannel, projectedCovariance, weights);
}

} // namespace physicallayer
} // namespace inet
