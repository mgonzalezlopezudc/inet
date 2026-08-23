//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixReceptionAnalogModel.h"

namespace inet {
namespace physicallayer {

ChannelMatrixReceptionAnalogModel::ChannelMatrixReceptionAnalogModel(simtime_t preambleDuration,
    simtime_t headerDuration, simtime_t dataDuration, Hz centerFrequency, Hz bandwidth,
    const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
    const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& deterministicLargeScalePower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& physicalPower) :
    DimensionalReceptionAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, physicalPower),
    snapshot(snapshot), spatialTransmissionPlan(spatialTransmissionPlan),
    deterministicLargeScalePower(deterministicLargeScalePower)
{
    if (!snapshot || !spatialTransmissionPlan || !deterministicLargeScalePower || !physicalPower)
        throw cRuntimeError("Channel-matrix reception requires non-null snapshot, spatial plan, and power functions");
    if (spatialTransmissionPlan->getNumberOfTransmitAntennas() != snapshot->getNumTransmitAntennas())
        throw cRuntimeError("Channel-matrix reception spatial plan has %d antennas instead of snapshot dimension %d",
            spatialTransmissionPlan->getNumberOfTransmitAntennas(), snapshot->getNumTransmitAntennas());
}

} // namespace physicallayer
} // namespace inet
