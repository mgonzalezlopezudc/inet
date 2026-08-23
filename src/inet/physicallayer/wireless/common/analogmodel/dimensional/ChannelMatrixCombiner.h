//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXCOMBINER_H
#define __INET_CHANNELMATRIXCOMBINER_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/ComplexMatrix.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixCombiner
{
  public:
    static double computeSelectedColumnGain(const ComplexMatrix& response, int selectedTransmitAntenna);
    static double computeSelectedColumnGain(const IChannelMatrixSnapshot& snapshot, int selectedTransmitAntenna, simtime_t time, Hz frequency);
};

} // namespace physicallayer
} // namespace inet

#endif
