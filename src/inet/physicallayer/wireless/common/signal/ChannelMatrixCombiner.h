//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXCOMBINER_H
#define __INET_CHANNELMATRIXCOMBINER_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixResponse.h"

namespace inet {
namespace physicallayer {

/**
 * Single-stream MRC for spatially white, equal-variance, uncorrelated receive
 * noise. Multi-transmit-antenna use requires an explicit unit-norm precoder.
 */
class INET_API ChannelMatrixCombiner
{
  public:
    static double computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix);
    static double computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix,
            const std::vector<std::complex<double>>& transmitWeights);
};

} // namespace physicallayer
} // namespace inet

#endif
