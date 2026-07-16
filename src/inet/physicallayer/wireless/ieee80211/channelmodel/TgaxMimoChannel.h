//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGAXMIMOCHANNEL_H
#define __INET_TGAXMIMOCHANNEL_H

#include <cstdint>
#include <memory>
#include <vector>

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixResponse.h"
#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxChannelProfile.h"

namespace inet {
namespace physicallayer {

class INET_API TgaxMimoChannel : public IChannelMatrixResponse
{
  public:
    struct Tap {
        simsec excessDelay;
        ChannelMatrix coefficient;
    };

  protected:
    Hz referenceFrequency;
    int numReceiveAntennas;
    int numTransmitAntennas;
    std::vector<Tap> taps;

  public:
    TgaxMimoChannel(Hz referenceFrequency, int numReceiveAntennas, int numTransmitAntennas,
            std::vector<Tap> taps);

    static std::shared_ptr<const TgaxMimoChannel> create(const TgaxChannelProfile& profile,
            Hz referenceFrequency, int numReceiveAntennas, int numTransmitAntennas, uint64_t seed);
    static std::complex<double> computeHalfWavelengthUlaCorrelation(double meanAngleDegrees,
            double angularSpreadDegrees);

    const std::vector<Tap>& getTaps() const { return taps; }
    std::shared_ptr<const TgaxMimoChannel> transposed() const;

    virtual int getNumReceiveAntennas() const override { return numReceiveAntennas; }
    virtual int getNumTransmitAntennas() const override { return numTransmitAntennas; }
    virtual ChannelMatrix getChannelMatrix(simsec time, Hz frequency) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
