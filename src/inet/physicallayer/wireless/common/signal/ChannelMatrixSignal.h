//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXSIGNAL_H
#define __INET_CHANNELMATRIXSIGNAL_H

#include <complex>
#include <memory>
#include <vector>

#include "inet/common/math/IFunction.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixResponse.h"

namespace inet {
namespace physicallayer {

using namespace inet::math;

/**
 * Immutable single-stream spatial signal metadata. The input power is the
 * propagated transmit PSD after large-scale attenuation and before applying
 * the phase-preserving channel matrix response.
 */
class INET_API ChannelMatrixSignal
{
  protected:
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> inputPower;
    const std::shared_ptr<const IChannelMatrixResponse> channelMatrixResponse;
    const std::vector<std::complex<double>> transmitWeights;

  public:
    ChannelMatrixSignal(const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& inputPower,
            const std::shared_ptr<const IChannelMatrixResponse>& channelMatrixResponse,
            std::vector<std::complex<double>> transmitWeights);

    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getInputPower() const { return inputPower; }
    const std::shared_ptr<const IChannelMatrixResponse>& getChannelMatrixResponse() const { return channelMatrixResponse; }
    const std::vector<std::complex<double>>& getTransmitWeights() const { return transmitWeights; }
    int getNumReceiveAntennas() const { return channelMatrixResponse->getNumReceiveAntennas(); }

    std::vector<std::complex<double>> computeEffectiveChannel(simsec time, Hz frequency) const;
    std::shared_ptr<const ChannelMatrixSignal> withInputPower(
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& inputPower) const;
};

} // namespace physicallayer
} // namespace inet

#endif
