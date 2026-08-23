//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXRECEPTIONANALOGMODEL_H
#define __INET_CHANNELMATRIXRECEPTIONANALOGMODEL_H

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixReceptionAnalogModel : public DimensionalReceptionAnalogModel
{
  protected:
    std::shared_ptr<const IChannelMatrixSnapshot> snapshot;
    std::shared_ptr<const SpatialTransmissionPlan> spatialTransmissionPlan;
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> deterministicLargeScalePower;

  public:
    ChannelMatrixReceptionAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration,
        Hz centerFrequency, Hz bandwidth, const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
        const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& deterministicLargeScalePower,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& physicalPower);

    const std::shared_ptr<const IChannelMatrixSnapshot>& getSnapshot() const { return snapshot; }
    const std::shared_ptr<const SpatialTransmissionPlan>& getSpatialTransmissionPlan() const { return spatialTransmissionPlan; }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getDeterministicLargeScalePower() const { return deterministicLargeScalePower; }
    /** CCA uses the same aggregate physical power until spatial SNIR exists. */
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getCcaPower() const { return getPower(); }
};

} // namespace physicallayer
} // namespace inet

#endif
