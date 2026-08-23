//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MaximumRatioCombiner.h"

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

ChannelMatrixDetectionResult MaximumRatioCombiner::compute(const ComplexMatrix& effectiveChannel,
    const ComplexMatrix& projectedCovariance)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "MRC effective channel");
    return compute(effectiveChannel, projectedCovariance, allRows(effectiveChannel.getNumRows()));
}

ChannelMatrixDetectionResult MaximumRatioCombiner::compute(const ComplexMatrix& effectiveChannel,
    const ComplexMatrix& projectedCovariance, const std::vector<int>& selectedReceiveRows)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "MRC effective channel");
    if (effectiveChannel.getNumColumns() != 1)
        throw cRuntimeError("Maximum-ratio combining requires a one-column effective channel");
    ChannelMatrixAlgebra::validateDimensions(projectedCovariance, effectiveChannel.getNumRows(),
        effectiveChannel.getNumRows(), "MRC covariance");
    ReceiveAntennaSelection::validateRowSet(selectedReceiveRows, effectiveChannel.getNumRows());
    const auto selectedChannel = ChannelMatrixAlgebra::selectRows(effectiveChannel, selectedReceiveRows);
    const auto selectedCovariance = ChannelMatrixAlgebra::selectRowsAndColumns(
        projectedCovariance, selectedReceiveRows);
    ChannelMatrixAlgebra::validatePositiveDefinite(selectedCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "MRC covariance");
    ComplexMatrix weights(1, selectedChannel.getNumRows());
    bool zeroChannel = true;
    for (int row = 0; row < selectedChannel.getNumRows(); row++) {
        weights.get(0, row) = std::conj(selectedChannel.get(row, 0));
        zeroChannel = zeroChannel && selectedChannel.get(row, 0) == std::complex<double>(0, 0);
    }
    if (zeroChannel)
        weights.get(0, 0) = 1;
    return ChannelMatrixDetectionResult::fromCanonicalWeights(
        ChannelMatrixDetectionStatus::SUCCESS, selectedReceiveRows,
        selectedChannel, selectedCovariance, weights);
}

} // namespace physicallayer
} // namespace inet
