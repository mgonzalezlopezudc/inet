//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MINIMUMMEANSQUAREERRORSPATIALSTREAMDETECTOR_H
#define __INET_MINIMUMMEANSQUAREERRORSPATIALSTREAMDETECTOR_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

namespace inet {
namespace physicallayer {

/** Factorized linear MMSE detector with finite outputs for rank-deficient H. */
class INET_API MinimumMeanSquareErrorSpatialStreamDetector final
{
  public:
    static ChannelMatrixDetectionResult compute(const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance);
    static ChannelMatrixDetectionResult compute(const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance, const std::vector<int>& selectedReceiveRows);
    static ChannelMatrixDetectionResult compute(const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance,
        const std::vector<ChannelMatrixObservationCoordinate>& observationCoordinates);
};

} // namespace physicallayer
} // namespace inet

#endif
