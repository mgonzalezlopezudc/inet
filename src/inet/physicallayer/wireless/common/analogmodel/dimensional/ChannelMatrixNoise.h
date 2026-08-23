//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXNOISE_H
#define __INET_CHANNELMATRIXNOISE_H

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixReceiver.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReception.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixNoise : public DimensionalNoise
{
  public:
    /** Owned, receiver-independent snapshot of an interfering matrix signal. */
    struct INET_API Interferer final
    {
        int transmissionId;
        simtime_t startTime;
        simtime_t endTime;
        Hz centerFrequency;
        Hz bandwidth;
        std::shared_ptr<const IChannelMatrixSnapshot> snapshot;
        std::shared_ptr<const SpatialTransmissionPlan> spatialTransmissionPlan;
        Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> deterministicLargeScalePower;
        Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> physicalAggregatePower;
        std::vector<ChannelMatrixResourceCell> resourceCells;

        Interferer(int transmissionId, simtime_t startTime, simtime_t endTime,
            Hz centerFrequency, Hz bandwidth,
            const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
            const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& deterministicLargeScalePower,
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& physicalAggregatePower,
            const std::vector<ChannelMatrixResourceCell>& resourceCells = {});
    };

  protected:
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> ccaBackgroundPower;
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> compatibilityBackgroundPower;
    std::vector<Interferer> interferers;

  public:
    ChannelMatrixNoise(simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaAggregatePower,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaBackgroundPower,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& compatibilityBackgroundPower,
        const std::vector<Interferer>& interferers);

    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getCcaBackgroundPower() const { return ccaBackgroundPower; }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getCompatibilityBackgroundPower() const { return compatibilityBackgroundPower; }
    const std::vector<Interferer>& getInterferers() const { return interferers; }
};

} // namespace physicallayer
} // namespace inet

#endif
