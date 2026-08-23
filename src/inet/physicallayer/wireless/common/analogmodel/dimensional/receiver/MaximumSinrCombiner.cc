//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MaximumSinrCombiner.h"

#include <cmath>

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

ChannelMatrixDetectionResult MaximumSinrCombiner::compute(const ComplexMatrix& effectiveChannel,
    const ComplexMatrix& projectedCovariance)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "Maximum-SINR effective channel");
    return compute(effectiveChannel, projectedCovariance, allRows(effectiveChannel.getNumRows()));
}

ChannelMatrixDetectionResult MaximumSinrCombiner::compute(const ComplexMatrix& effectiveChannel,
    const ComplexMatrix& projectedCovariance, const std::vector<int>& selectedReceiveRows)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "Maximum-SINR effective channel");
    if (effectiveChannel.getNumColumns() != 1)
        throw cRuntimeError("Maximum-SINR combining requires a one-column effective channel");
    ChannelMatrixAlgebra::validateDimensions(projectedCovariance, effectiveChannel.getNumRows(),
        effectiveChannel.getNumRows(), "Maximum-SINR covariance");
    ReceiveAntennaSelection::validateRowSet(selectedReceiveRows, effectiveChannel.getNumRows());
    const ComplexMatrix selectedChannel = ChannelMatrixAlgebra::selectRows(effectiveChannel, selectedReceiveRows);
    const ComplexMatrix selectedCovariance = ChannelMatrixAlgebra::selectRowsAndColumns(
        projectedCovariance, selectedReceiveRows);
    ChannelMatrixAlgebra::validatePositiveDefinite(selectedCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "Maximum-SINR covariance");

    ComplexMatrix weights(1, selectedChannel.getNumRows());
    bool zeroChannel = true;
    for (int row = 0; row < selectedChannel.getNumRows(); row++)
        zeroChannel = zeroChannel && selectedChannel.get(row, 0) == std::complex<double>(0, 0);
    if (zeroChannel)
        weights.get(0, 0) = 1; // deterministic lowest-row basis for a zero channel
    else {
        const ComplexMatrix solution = ChannelMatrixAlgebra::solveHermitianPositiveDefinite(
            selectedCovariance, selectedChannel);
        for (int row = 0; row < selectedChannel.getNumRows(); row++)
            weights.get(0, row) = std::conj(solution.get(row, 0));
    }
    return ChannelMatrixDetectionResult::fromCanonicalWeights(
        ChannelMatrixDetectionStatus::SUCCESS, selectedReceiveRows, selectedChannel,
        selectedCovariance, weights);
}

} // namespace physicallayer
} // namespace inet
