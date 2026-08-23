//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/SelectionCombiner.h"

#include <algorithm>
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

ChannelMatrixDetectionResult SelectionCombiner::compute(const ComplexMatrix& effectiveChannel,
    const ComplexMatrix& projectedCovariance)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "Selection-combiner effective channel");
    return compute(effectiveChannel, projectedCovariance, allRows(effectiveChannel.getNumRows()));
}

ChannelMatrixDetectionResult SelectionCombiner::compute(const ComplexMatrix& effectiveChannel,
    const ComplexMatrix& projectedCovariance, const std::vector<int>& selectedReceiveRows)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "Selection-combiner effective channel");
    if (effectiveChannel.getNumColumns() != 1)
        throw cRuntimeError("Selection combining requires a one-column effective channel");
    ChannelMatrixAlgebra::validateDimensions(projectedCovariance, effectiveChannel.getNumRows(),
        effectiveChannel.getNumRows(), "Selection-combiner covariance");
    ReceiveAntennaSelection::validateRowSet(selectedReceiveRows, effectiveChannel.getNumRows());
    const ComplexMatrix selectedChannel = ChannelMatrixAlgebra::selectRows(effectiveChannel, selectedReceiveRows);
    const ComplexMatrix selectedCovariance = ChannelMatrixAlgebra::selectRowsAndColumns(
        projectedCovariance, selectedReceiveRows);
    ChannelMatrixAlgebra::validatePositiveDefinite(selectedCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "Selection-combiner covariance");

    int bestBranch = 0;
    double bestScore = -1;
    for (int branch = 0; branch < selectedChannel.getNumRows(); branch++) {
        const std::complex<double> diagonal = selectedCovariance.get(branch, branch);
        const double scale = std::abs(diagonal);
        if (std::abs(diagonal.imag()) > 1e-9 * scale || diagonal.real() <= 0)
            throw cRuntimeError("Selection-combiner covariance diagonal must be real and positive");
        const double score = std::norm(selectedChannel.get(branch, 0)) / diagonal.real();
        if (!std::isfinite(score))
            throw cRuntimeError("Selection-combiner branch SINR is not finite");
        if (score > bestScore) {
            bestScore = score;
            bestBranch = branch;
        }
    }
    ComplexMatrix weights(1, selectedChannel.getNumRows());
    weights.get(0, bestBranch) = 1;
    return ChannelMatrixDetectionResult::fromCanonicalWeights(
        ChannelMatrixDetectionStatus::SUCCESS, selectedReceiveRows, selectedChannel,
        selectedCovariance, weights);
}

} // namespace physicallayer
} // namespace inet
