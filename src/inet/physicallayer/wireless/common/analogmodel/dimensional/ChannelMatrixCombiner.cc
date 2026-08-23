//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixCombiner.h"

#include <cmath>

namespace inet {
namespace physicallayer {

double ChannelMatrixCombiner::computeSelectedColumnGain(const ComplexMatrix& response, int selectedTransmitAntenna)
{
    if (selectedTransmitAntenna < 0 || selectedTransmitAntenna >= response.getNumColumns())
        throw cRuntimeError("Selected transmit antenna %d is outside matrix with %d columns", selectedTransmitAntenna, response.getNumColumns());
    double gain = 0;
    for (int row = 0; row < response.getNumRows(); row++)
        gain += std::norm(response.get(row, selectedTransmitAntenna));
    if (!std::isfinite(gain))
        throw cRuntimeError("Selected-column channel gain is non-finite");
    return gain;
}

double ChannelMatrixCombiner::computeSelectedColumnGain(const IChannelMatrixSnapshot& snapshot, int selectedTransmitAntenna, simtime_t time, Hz frequency)
{
    return computeSelectedColumnGain(snapshot.getResponse(time, frequency), selectedTransmitAntenna);
}

} // namespace physicallayer
} // namespace inet
