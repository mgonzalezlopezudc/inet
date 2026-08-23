//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ZeroForcingSpatialStreamDetector.h"

#include <cmath>
#include <limits>

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ReceiveAntennaSelection.h"

namespace inet {
namespace physicallayer {

namespace {

std::vector<int> allRows(int numberOfRows)
{
    std::vector<int> result(numberOfRows);
    for (int row = 0; row < numberOfRows; row++)
        result[row] = row;
    return result;
}

} // namespace

ChannelMatrixDetectionResult ZeroForcingSpatialStreamDetector::compute(
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "Zero-forcing effective channel");
    return compute(effectiveChannel, projectedCovariance, allRows(effectiveChannel.getNumRows()));
}

ChannelMatrixDetectionResult ZeroForcingSpatialStreamDetector::compute(
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance,
    const std::vector<int>& selectedReceiveRows)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "Zero-forcing effective channel");
    if (effectiveChannel.getNumColumns() <= 0)
        throw cRuntimeError("Zero-forcing effective channel must have at least one stream");
    ChannelMatrixAlgebra::validateDimensions(projectedCovariance, effectiveChannel.getNumRows(),
        effectiveChannel.getNumRows(), "Zero-forcing covariance");
    ReceiveAntennaSelection::validateRowSet(selectedReceiveRows, effectiveChannel.getNumRows());
    const ComplexMatrix selectedChannel = ChannelMatrixAlgebra::selectRows(effectiveChannel, selectedReceiveRows);
    const ComplexMatrix selectedCovariance = ChannelMatrixAlgebra::selectRowsAndColumns(
        projectedCovariance, selectedReceiveRows);
    ChannelMatrixAlgebra::validatePositiveDefinite(selectedCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "Zero-forcing covariance");
    const int numberOfReceiveRows = selectedChannel.getNumRows();
    const int numberOfStreams = selectedChannel.getNumColumns();
    ComplexMatrix zeroWeights(numberOfStreams, numberOfReceiveRows);

    if (numberOfReceiveRows < numberOfStreams)
        return ChannelMatrixDetectionResult::fromCanonicalWeights(
            ChannelMatrixDetectionStatus::UNDERDETERMINED, selectedReceiveRows,
            selectedChannel, selectedCovariance, zeroWeights);
    if (ChannelMatrixAlgebra::computeRank(selectedChannel) < numberOfStreams)
        return ChannelMatrixDetectionResult::fromCanonicalWeights(
            ChannelMatrixDetectionStatus::RANK_DEFICIENT, selectedReceiveRows,
            selectedChannel, selectedCovariance, zeroWeights);

    const ComplexMatrix x = ChannelMatrixAlgebra::solveHermitianPositiveDefinite(
        selectedCovariance, selectedChannel);
    const ComplexMatrix gram = ChannelMatrixAlgebra::multiply(
        ChannelMatrixAlgebra::conjugateTranspose(selectedChannel), x);
    double conditionNumber = std::numeric_limits<double>::infinity();
    try {
        conditionNumber = ChannelMatrixAlgebra::computeConditionNumber(gram);
    }
    catch (const cRuntimeError&) {
        conditionNumber = std::numeric_limits<double>::infinity();
    }
    if (!std::isfinite(conditionNumber) || conditionNumber > MAX_GRAM_CONDITION_NUMBER)
        return ChannelMatrixDetectionResult::fromCanonicalWeights(
            ChannelMatrixDetectionStatus::ILL_CONDITIONED, selectedReceiveRows,
            selectedChannel, selectedCovariance, zeroWeights);

    ComplexMatrix weights;
    try {
        // G W = X^H; this is the covariance-whitened ZF equation and does
        // not materialize an inverse of either Rz or G.
        weights = ChannelMatrixAlgebra::solveHermitianPositiveDefinite(
            gram, ChannelMatrixAlgebra::conjugateTranspose(x));
    }
    catch (const cRuntimeError&) {
        return ChannelMatrixDetectionResult::fromCanonicalWeights(
            ChannelMatrixDetectionStatus::ILL_CONDITIONED, selectedReceiveRows,
            selectedChannel, selectedCovariance, zeroWeights);
    }
    return ChannelMatrixDetectionResult::fromCanonicalWeights(
        ChannelMatrixDetectionStatus::SUCCESS, selectedReceiveRows,
        selectedChannel, selectedCovariance, weights);
}

} // namespace physicallayer
} // namespace inet
