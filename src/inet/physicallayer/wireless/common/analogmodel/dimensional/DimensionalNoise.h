//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_DIMENSIONALNOISE_H
#define __INET_DIMENSIONALNOISE_H

#include "inet/common/math/IFunction.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandNoiseBase.h"
#include "inet/physicallayer/wireless/common/signal/ChannelMatrixSignal.h"

namespace inet {

namespace physicallayer {

using namespace inet::math;

class INET_API DimensionalNoise : public NarrowbandNoiseBase
{
  protected:
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> power;
    const bool containsInterferingReceptions;
    // Spatially white, equal power spectral density on every receive branch.
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> backgroundNoisePower;
    const std::vector<std::shared_ptr<const ChannelMatrixSignal>> channelMatrixInterferers;
    const bool containsNonChannelMatrixInterference;

  public:
    DimensionalNoise(simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth,
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power, bool hasInterferingReceptions = false,
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& backgroundNoisePower = nullptr,
            std::vector<std::shared_ptr<const ChannelMatrixSignal>> channelMatrixInterferers = {},
            bool containsNonChannelMatrixInterference = false);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
    virtual const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getPower() const { return power; }
    bool hasInterferingReceptions() const { return containsInterferingReceptions; }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getBackgroundNoisePower() const { return backgroundNoisePower; }
    const std::vector<std::shared_ptr<const ChannelMatrixSignal>>& getChannelMatrixInterferers() const { return channelMatrixInterferers; }
    bool hasNonChannelMatrixInterference() const { return containsNonChannelMatrixInterference; }

    virtual W computeMinPower(simtime_t startTime, simtime_t endTime) const override;
    virtual W computeMaxPower(simtime_t startTime, simtime_t endTime) const override;
};

} // namespace physicallayer

} // namespace inet

#endif
