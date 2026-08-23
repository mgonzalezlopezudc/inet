//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ICHANNELMATRIXRECEPTIONPROCESSOR_H
#define __INET_ICHANNELMATRIXRECEPTIONPROCESSOR_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixReceptionContext.h"

namespace inet {
namespace physicallayer {

/**
 * Stateless receiver-local policy for reducing one exact matrix/covariance
 * sample to decoded spatial-stream results.
 */
class INET_API IChannelMatrixReceptionProcessor
{
  public:
    virtual ~IChannelMatrixReceptionProcessor() = default;

    virtual ChannelMatrixDetectionResult compute(const ChannelMatrixReceptionContext& context) const = 0;
    virtual ChannelMatrixDetectionResult computeSpaceTimeBlock(
        const std::vector<ChannelMatrixReceptionContext>& slotContexts) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
