//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_PERFECTCANCELLATIONSUCCESSIVEINTERFERENCECANCELLATIONSPATIALSTREAMDETECTOR_H
#define __INET_PERFECTCANCELLATIONSUCCESSIVEINTERFERENCECANCELLATIONSPATIALSTREAMDETECTOR_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

namespace inet {
namespace physicallayer {

/**
 * Upper-bound SIC detector: MMSE is recomputed on remaining columns, the
 * selected column is perfectly cancelled, and no symbol decisions or error
 * propagation are modeled.
 */
class INET_API PerfectCancellationSuccessiveInterferenceCancellationSpatialStreamDetector final
{
  public:
    static ChannelMatrixDetectionResult compute(const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance);
    static ChannelMatrixDetectionResult compute(const ComplexMatrix& effectiveChannel,
        const ComplexMatrix& projectedCovariance, const std::vector<int>& selectedReceiveRows);
};

} // namespace physicallayer
} // namespace inet

#endif
