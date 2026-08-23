//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/PerfectCancellationSuccessiveInterferenceCancellationSpatialStreamDetector.h"

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MinimumMeanSquareErrorSpatialStreamDetector.h"
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

ChannelMatrixDetectionResult PerfectCancellationSuccessiveInterferenceCancellationSpatialStreamDetector::compute(
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "MMSE-SIC effective channel");
    return compute(effectiveChannel, projectedCovariance, allRows(effectiveChannel.getNumRows()));
}

ChannelMatrixDetectionResult PerfectCancellationSuccessiveInterferenceCancellationSpatialStreamDetector::compute(
    const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance,
    const std::vector<int>& selectedReceiveRows)
{
    ChannelMatrixAlgebra::validateFinite(effectiveChannel, "MMSE-SIC effective channel");
    if (effectiveChannel.getNumColumns() <= 0)
        throw cRuntimeError("MMSE-SIC effective channel must have at least one stream");
    ChannelMatrixAlgebra::validateDimensions(projectedCovariance, effectiveChannel.getNumRows(),
        effectiveChannel.getNumRows(), "MMSE-SIC covariance");
    ReceiveAntennaSelection::validateRowSet(selectedReceiveRows, effectiveChannel.getNumRows());
    const ComplexMatrix selectedChannel = ChannelMatrixAlgebra::selectRows(effectiveChannel, selectedReceiveRows);
    const ComplexMatrix selectedCovariance = ChannelMatrixAlgebra::selectRowsAndColumns(
        projectedCovariance, selectedReceiveRows);
    ChannelMatrixAlgebra::validatePositiveDefinite(selectedCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "MMSE-SIC covariance");

    const int numberOfStreams = selectedChannel.getNumColumns();
    const int numberOfRows = selectedChannel.getNumRows();
    ComplexMatrix weights(numberOfStreams, numberOfRows);
    std::vector<WpHz> desired(numberOfStreams, WpHz(0));
    std::vector<WpHz> residual(numberOfStreams, WpHz(0));
    std::vector<WpHz> projected(numberOfStreams, WpHz(0));
    std::vector<double> sinrs(numberOfStreams, 0);
    std::vector<int> active(numberOfStreams);
    for (int stream = 0; stream < numberOfStreams; stream++)
        active[stream] = stream;
    std::vector<int> detectionOrder;
    detectionOrder.reserve(numberOfStreams);

    while (!active.empty()) {
        const ComplexMatrix remainingChannel = ChannelMatrixAlgebra::selectColumns(selectedChannel, active);
        const auto stage = MinimumMeanSquareErrorSpatialStreamDetector::compute(
            remainingChannel, selectedCovariance);
        int bestLocalIndex = 0;
        for (int localIndex = 1; localIndex < (int)active.size(); localIndex++)
            if (stage.getSinrs()[localIndex] > stage.getSinrs()[bestLocalIndex])
                bestLocalIndex = localIndex;
        const int originalStream = active[bestLocalIndex];
        detectionOrder.push_back(originalStream);
        for (int row = 0; row < numberOfRows; row++)
            weights.get(originalStream, row) = stage.getWeights().get(bestLocalIndex, row);
        desired[originalStream] = stage.getDesiredSignalPowerSpectralDensities()[bestLocalIndex];
        residual[originalStream] = stage.getCrossStreamResidualPowerSpectralDensities()[bestLocalIndex];
        projected[originalStream] = stage.getProjectedNoiseAndInterferencePowerSpectralDensities()[bestLocalIndex];
        sinrs[originalStream] = stage.getSinrs()[bestLocalIndex];
        active.erase(active.begin() + bestLocalIndex);
    }

    return ChannelMatrixDetectionResult::fromPhysicalOutputs(
        ChannelMatrixDetectionStatus::SUCCESS, selectedReceiveRows, weights,
        desired, residual, projected, sinrs, detectionOrder);
}

} // namespace physicallayer
} // namespace inet
