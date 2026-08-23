//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixReceptionAnalogModel.h"

namespace inet {
namespace physicallayer {

ChannelMatrixReceptionAnalogModel::ChannelMatrixReceptionAnalogModel(simtime_t preambleDuration,
    simtime_t headerDuration, simtime_t dataDuration, Hz centerFrequency, Hz bandwidth,
    const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot, int selectedTransmitAntenna,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& deterministicLargeScalePower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& decodedPower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaPower) :
    DimensionalReceptionAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, decodedPower),
    snapshot(snapshot), selectedTransmitAntenna(selectedTransmitAntenna),
    deterministicLargeScalePower(deterministicLargeScalePower), ccaPower(ccaPower)
{
    if (!snapshot || !deterministicLargeScalePower || !decodedPower || !ccaPower)
        throw cRuntimeError("Channel-matrix reception requires non-null snapshot and power functions");
    if (selectedTransmitAntenna < 0 || selectedTransmitAntenna >= snapshot->getNumTransmitAntennas())
        throw cRuntimeError("Selected transmit antenna %d is outside a %d-column channel snapshot",
            selectedTransmitAntenna, snapshot->getNumTransmitAntennas());
}

} // namespace physicallayer
} // namespace inet
