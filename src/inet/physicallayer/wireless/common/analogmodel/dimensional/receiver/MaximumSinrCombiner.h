//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MAXIMUMSINRCOMBINER_H
#define __INET_MAXIMUMSINRCOMBINER_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

namespace inet {
namespace physicallayer {

/** Full-covariance maximum-SINR combiner using an HPD solve, never an inverse. */
class INET_API MaximumSinrCombiner final
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
