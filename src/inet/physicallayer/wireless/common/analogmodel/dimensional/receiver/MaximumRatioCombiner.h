//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MAXIMUMRATIOCOMBINER_H
#define __INET_MAXIMUMRATIOCOMBINER_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

namespace inet {
namespace physicallayer {

/** Compatibility MRC using the conjugated desired-channel direction. */
class INET_API MaximumRatioCombiner final
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
