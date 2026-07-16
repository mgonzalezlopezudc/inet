//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXCOMBINER_H
#define __INET_CHANNELMATRIXCOMBINER_H

#include <memory>

#include "inet/common/math/IFunction.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixResponse.h"

namespace inet {
namespace physicallayer {

using namespace inet::math;

class ChannelMatrixSignal;

/**
 * Single-stream MRC for spatially white, equal-variance, uncorrelated receive
 * noise. Multi-transmit-antenna use requires an explicit unit-norm precoder.
 */
class INET_API ChannelMatrixCombiner
{
  public:
    static std::vector<std::complex<double>> computeEffectiveChannel(const ChannelMatrix& channelMatrix,
            const std::vector<std::complex<double>>& transmitWeights);
    static double computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix);
    static double computeSingleStreamMrcPowerGain(const ChannelMatrix& channelMatrix,
            const std::vector<std::complex<double>>& transmitWeights);
    static double computeSingleStreamLmmseSinr(const std::vector<std::complex<double>>& desiredChannel,
            double desiredPower, const std::vector<std::vector<std::complex<double>>>& interferingChannels,
            const std::vector<double>& interferingPowers, double noisePower);
    static Ptr<const IFunction<double, Domain<simsec, Hz>>> createStaticSingleStreamMrcPowerGain(
            const std::shared_ptr<const IChannelMatrixResponse>& channelMatrixResponse,
            int selectedTransmitAntenna, simtime_t startTime, simtime_t endTime,
            Hz centerFrequency, Hz bandwidth, Hz frequencyResolution);
    static Ptr<const IFunction<double, Domain<simsec, Hz>>> createStaticSingleStreamLmmseSnir(
            const std::shared_ptr<const ChannelMatrixSignal>& desiredSignal,
            const std::vector<std::shared_ptr<const ChannelMatrixSignal>>& interferingSignals,
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& backgroundNoisePower,
            simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth,
            simtime_t timeResolution, Hz frequencyResolution);
};

} // namespace physicallayer
} // namespace inet

#endif
