//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixResponse.h"

#include <cmath>

namespace inet {
namespace physicallayer {

ChannelMatrixResponse::ChannelMatrixResponse(int numReceiveAntennas, int numTransmitAntennas, const Evaluator& evaluator) :
    numReceiveAntennas(numReceiveAntennas), numTransmitAntennas(numTransmitAntennas), evaluator(evaluator)
{
    if (numReceiveAntennas <= 0 || numTransmitAntennas <= 0)
        throw cRuntimeError("Channel response dimensions must be positive, got %d x %d", numReceiveAntennas, numTransmitAntennas);
    if (!evaluator)
        throw cRuntimeError("Channel response evaluator is null");
}

ComplexMatrix ChannelMatrixResponse::getValue(simtime_t absoluteTime, Hz frequency) const
{
    if (!std::isfinite(frequency.get()))
        throw cRuntimeError("Channel response frequency is non-finite");
    auto result = evaluator(absoluteTime, frequency);
    if (result.getNumRows() != numReceiveAntennas || result.getNumColumns() != numTransmitAntennas)
        throw cRuntimeError("Channel response returned %d x %d instead of %d x %d", result.getNumRows(), result.getNumColumns(), numReceiveAntennas, numTransmitAntennas);
    if (!result.isFinite())
        throw cRuntimeError("Channel response contains a non-finite coefficient");
    return result;
}

std::shared_ptr<const ChannelMatrixResponse> ChannelMatrixResponse::transpose() const
{
    auto original = std::make_shared<const ChannelMatrixResponse>(*this);
    return std::make_shared<const ChannelMatrixResponse>(numTransmitAntennas, numReceiveAntennas,
        [original](simtime_t time, Hz frequency) { return original->getValue(time, frequency).transpose(); });
}

} // namespace physicallayer
} // namespace inet
