//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ZEROFORCINGSPATIALSTREAMDETECTOR_H
#define __INET_ZEROFORCINGSPATIALSTREAMDETECTOR_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

namespace inet {
namespace physicallayer {

/** Covariance-whitened zero-forcing detector for multiple spatial streams. */
class INET_API ZeroForcingSpatialStreamDetector final
{
  public:
    // This detector-specific bound is intentionally independent of the
    // algebra's pivot/rank tolerance.
    static constexpr double MAX_GRAM_CONDITION_NUMBER = 1e8;

    static ChannelMatrixDetectionResult compute(const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance);
    static ChannelMatrixDetectionResult compute(const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance, const std::vector<int>& selectedReceiveRows);
};

} // namespace physicallayer
} // namespace inet

#endif
