//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXDETECTIONRESULT_H
#define __INET_CHANNELMATRIXDETECTIONRESULT_H

#include <optional>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixAlgebra.h"

namespace inet {
namespace physicallayer {

enum class ChannelMatrixDetectionStatus {
    SUCCESS,
    UNDERDETERMINED,
    RANK_DEFICIENT,
    ILL_CONDITIONED,
    UNSUPPORTED_LAYOUT
};

/**
 * Identifies one row in an observation vector before spatial projection.
 * The slot and physical receive-row fields are deliberately kept separate:
 * space-time processing may observe the same antenna more than once, while
 * the conjugation flag records the canonical augmented-observation transform.
 */
class INET_API ChannelMatrixObservationCoordinate final
{
  private:
    int slotIndex;
    int receiveRowIndex;
    bool conjugated;

  public:
    ChannelMatrixObservationCoordinate(int slotIndex, int receiveRowIndex, bool conjugated);

    int getSlotIndex() const { return slotIndex; }
    int getReceiveRowIndex() const { return receiveRowIndex; }
    bool isConjugated() const { return conjugated; }

    bool operator==(const ChannelMatrixObservationCoordinate& other) const {
        return slotIndex == other.slotIndex && receiveRowIndex == other.receiveRowIndex && conjugated == other.conjugated;
    }
    bool operator!=(const ChannelMatrixObservationCoordinate& other) const { return !(*this == other); }
};

/**
 * Immutable value object containing the physical outputs of a receiver
 * detector.  Weights are detector rows: the output for stream k is
 * weights[k] * y, and covariance matrices in this API contain numeric W/Hz
 * values.  The PSD vectors retain WpHz units at the value-type boundary.
 */
class INET_API ChannelMatrixDetectionResult final
{
  private:
    ChannelMatrixDetectionStatus status;
    std::vector<int> selectedReceiveRows;
    std::vector<ChannelMatrixObservationCoordinate> observationCoordinates;
    ComplexMatrix weights;
    std::vector<WpHz> desiredSignalPowerSpectralDensities;
    std::vector<WpHz> crossStreamResidualPowerSpectralDensities;
    std::vector<WpHz> projectedNoiseAndInterferencePowerSpectralDensities;
    std::vector<double> sinrs;
    std::optional<std::vector<int>> detectionOrder;

    ChannelMatrixDetectionResult(ChannelMatrixDetectionStatus status,
        const std::vector<int>& selectedReceiveRows, const ComplexMatrix& weights,
        const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates,
        const std::vector<WpHz>& desiredSignalPowerSpectralDensities,
        const std::vector<WpHz>& crossStreamResidualPowerSpectralDensities,
        const std::vector<WpHz>& projectedNoiseAndInterferencePowerSpectralDensities,
        const std::vector<double>& sinrs, const std::optional<std::vector<int>>& detectionOrder);

  public:
    using Status = ChannelMatrixDetectionStatus;
    /**
     * Creates a result from canonical detector rows and computes all physical
     * outputs.  A non-success status deliberately produces finite zero-valued
     * outputs while preserving the requested dimensions and status.
     */
    static ChannelMatrixDetectionResult fromCanonicalWeights(ChannelMatrixDetectionStatus status,
        const std::vector<int>& selectedReceiveRows, const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance, const ComplexMatrix& canonicalWeights,
        const std::optional<std::vector<int>>& detectionOrder = std::nullopt);

    static ChannelMatrixDetectionResult fromCanonicalWeights(ChannelMatrixDetectionStatus status,
        const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates,
        const ComplexMatrix& effectiveChannel, const ComplexMatrix& projectedCovariance,
        const ComplexMatrix& canonicalWeights,
        const std::optional<std::vector<int>>& detectionOrder = std::nullopt);

    /** Creates a result from already computed physical values (used by SIC). */
    static ChannelMatrixDetectionResult fromPhysicalOutputs(ChannelMatrixDetectionStatus status,
        const std::vector<int>& selectedReceiveRows, const ComplexMatrix& weights,
        const std::vector<WpHz>& desiredSignalPowerSpectralDensities,
        const std::vector<WpHz>& crossStreamResidualPowerSpectralDensities,
        const std::vector<WpHz>& projectedNoiseAndInterferencePowerSpectralDensities,
        const std::vector<double>& sinrs,
        const std::optional<std::vector<int>>& detectionOrder = std::nullopt);

    static ChannelMatrixDetectionResult fromPhysicalOutputs(ChannelMatrixDetectionStatus status,
        const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates,
        const ComplexMatrix& weights,
        const std::vector<WpHz>& desiredSignalPowerSpectralDensities,
        const std::vector<WpHz>& crossStreamResidualPowerSpectralDensities,
        const std::vector<WpHz>& projectedNoiseAndInterferencePowerSpectralDensities,
        const std::vector<double>& sinrs,
        const std::optional<std::vector<int>>& detectionOrder = std::nullopt);

    ChannelMatrixDetectionStatus getStatus() const { return status; }
    const std::vector<int>& getSelectedReceiveRows() const { return selectedReceiveRows; }
    const std::vector<ChannelMatrixObservationCoordinate>& getObservationCoordinates() const { return observationCoordinates; }
    const ComplexMatrix& getWeights() const { return weights; }
    const std::vector<WpHz>& getDesiredSignalPowerSpectralDensities() const { return desiredSignalPowerSpectralDensities; }
    const std::vector<WpHz>& getCrossStreamResidualPowerSpectralDensities() const { return crossStreamResidualPowerSpectralDensities; }
    const std::vector<WpHz>& getProjectedNoiseAndInterferencePowerSpectralDensities() const { return projectedNoiseAndInterferencePowerSpectralDensities; }
    const std::vector<double>& getSinrs() const { return sinrs; }
    const std::optional<std::vector<int>>& getDetectionOrder() const { return detectionOrder; }
    bool hasDetectionOrder() const { return detectionOrder.has_value(); }
};

} // namespace physicallayer
} // namespace inet

#endif
