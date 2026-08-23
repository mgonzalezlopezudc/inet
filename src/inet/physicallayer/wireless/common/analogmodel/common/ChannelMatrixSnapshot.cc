//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixSnapshot.h"

#include <cmath>

namespace inet {
namespace physicallayer {

ChannelMatrixSnapshot::ChannelMatrixSnapshot(int numReceiveAntennas, int numTransmitAntennas, Hz referenceFrequency,
    simtime_t startTime, simtime_t endTime, double shadowingPowerGain,
    Hz actualMaximumTemporalFrequency, simtime_t maximumExcessDelay,
    const std::shared_ptr<const ChannelMatrixResponse>& response) :
    numReceiveAntennas(numReceiveAntennas), numTransmitAntennas(numTransmitAntennas), referenceFrequency(referenceFrequency),
    startTime(startTime), endTime(endTime), shadowingPowerGain(shadowingPowerGain),
    actualMaximumTemporalFrequency(actualMaximumTemporalFrequency), maximumExcessDelay(maximumExcessDelay), response(response)
{
    if (numReceiveAntennas <= 0 || numTransmitAntennas <= 0)
        throw cRuntimeError("Channel snapshot dimensions must be positive");
    if (!std::isfinite(referenceFrequency.get()) || referenceFrequency <= Hz(0))
        throw cRuntimeError("Channel snapshot reference frequency must be finite and positive");
    if (endTime < startTime)
        throw cRuntimeError("Channel snapshot interval is reversed");
    if (!std::isfinite(shadowingPowerGain) || shadowingPowerGain <= 0)
        throw cRuntimeError("Channel snapshot shadowing power gain must be finite and positive");
    if (!std::isfinite(actualMaximumTemporalFrequency.get()) || actualMaximumTemporalFrequency < Hz(0) || maximumExcessDelay < SIMTIME_ZERO)
        throw cRuntimeError("Channel snapshot temporal metadata is invalid");
    if (!response)
        throw cRuntimeError("Channel snapshot response is null");
    if (response->getNumReceiveAntennas() != numReceiveAntennas || response->getNumTransmitAntennas() != numTransmitAntennas)
        throw cRuntimeError("Channel snapshot dimensions %d x %d do not match response dimensions %d x %d",
            numReceiveAntennas, numTransmitAntennas, response->getNumReceiveAntennas(), response->getNumTransmitAntennas());
}

ComplexMatrix ChannelMatrixSnapshot::getResponse(simtime_t absoluteTime, Hz frequency) const
{
    return response->getValue(absoluteTime, frequency);
}

std::shared_ptr<const IChannelMatrixSnapshot> ChannelMatrixSnapshot::transpose() const
{
    return std::make_shared<const ChannelMatrixSnapshot>(numTransmitAntennas, numReceiveAntennas, referenceFrequency,
        startTime, endTime, shadowingPowerGain, actualMaximumTemporalFrequency, maximumExcessDelay, response->transpose());
}

std::ostream& ChannelMatrixSnapshot::printToStream(std::ostream& stream, int level, int evFlags) const
{
    return stream << "ChannelMatrixSnapshot, dimensions = " << numReceiveAntennas << " x " << numTransmitAntennas
                  << ", reference frequency = " << referenceFrequency << ", interval = [" << startTime << ", " << endTime << "]";
}

} // namespace physicallayer
} // namespace inet
