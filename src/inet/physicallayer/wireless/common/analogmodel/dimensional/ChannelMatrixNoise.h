//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXNOISE_H
#define __INET_CHANNELMATRIXNOISE_H

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReception.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixNoise : public DimensionalNoise
{
  protected:
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> ccaBackgroundPower;
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> postCombinerBackgroundPower;
    std::vector<const IReception *> interferingMatrixReceptions;

  public:
    ChannelMatrixNoise(simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaAggregatePower,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaBackgroundPower,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& postCombinerBackgroundPower,
        const std::vector<const IReception *>& interferingMatrixReceptions);

    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getCcaBackgroundPower() const { return ccaBackgroundPower; }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getPostCombinerBackgroundPower() const { return postCombinerBackgroundPower; }
    const std::vector<const IReception *>& getInterferingMatrixReceptions() const { return interferingMatrixReceptions; }
};

} // namespace physicallayer
} // namespace inet

#endif
