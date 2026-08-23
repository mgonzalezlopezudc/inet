//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_SELECTIONCOMBINER_H
#define __INET_SELECTIONCOMBINER_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

namespace inet {
namespace physicallayer {

/** One-column branch selection combining with deterministic row ties. */
class INET_API SelectionCombiner final
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
