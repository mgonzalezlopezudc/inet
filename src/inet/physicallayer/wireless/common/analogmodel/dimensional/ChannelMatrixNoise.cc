//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixNoise.h"

namespace inet {
namespace physicallayer {

ChannelMatrixNoise::ChannelMatrixNoise(simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaAggregatePower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaBackgroundPower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& postCombinerBackgroundPower,
    const std::vector<const IReception *>& interferingMatrixReceptions) :
    DimensionalNoise(startTime, endTime, centerFrequency, bandwidth, ccaAggregatePower),
    ccaBackgroundPower(ccaBackgroundPower), postCombinerBackgroundPower(postCombinerBackgroundPower),
    interferingMatrixReceptions(interferingMatrixReceptions)
{
    if (!ccaAggregatePower || !ccaBackgroundPower || !postCombinerBackgroundPower)
        throw cRuntimeError("Channel-matrix noise requires non-null power functions");
}

} // namespace physicallayer
} // namespace inet
