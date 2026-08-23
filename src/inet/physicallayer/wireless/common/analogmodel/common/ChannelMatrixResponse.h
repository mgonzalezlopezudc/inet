//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXRESPONSE_H
#define __INET_CHANNELMATRIXRESPONSE_H

#include <functional>

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixResponse
{
  public:
    using Evaluator = std::function<ComplexMatrix(simtime_t, Hz)>;

  protected:
    int numReceiveAntennas;
    int numTransmitAntennas;
    Evaluator evaluator;

  public:
    ChannelMatrixResponse(int numReceiveAntennas, int numTransmitAntennas, const Evaluator& evaluator);
    ComplexMatrix getValue(simtime_t absoluteTime, Hz frequency) const;
    std::shared_ptr<const ChannelMatrixResponse> transpose() const;
};

} // namespace physicallayer
} // namespace inet

#endif
