//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include <algorithm>
#include <cmath>
#include <cstring>

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxIndoorPathLoss.h"

namespace inet {

namespace physicallayer {

Define_Module(TgaxIndoorPathLoss);

TgaxIndoorPathLoss::TgaxIndoorPathLoss()
{
    configureChannelModel("B");
}

void TgaxIndoorPathLoss::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL)
        configureChannelModel(par("channelModel"));
}

void TgaxIndoorPathLoss::configureChannelModel(const char *channelModel)
{
    if (!strcmp(channelModel, "B")) {
        this->channelModel = "B";
        breakpointDistance = m(5);
    }
    else if (!strcmp(channelModel, "D")) {
        this->channelModel = "D";
        breakpointDistance = m(10);
    }
    else
        throw cRuntimeError("Unknown TGax indoor channel model: '%s' (expected 'B' or 'D')", channelModel);
}

void TgaxIndoorPathLoss::validatePropagationParameters(mps propagationSpeed, Hz frequency) const
{
    if (!std::isfinite(propagationSpeed.get<mps>()) || propagationSpeed <= mps(0))
        throw cRuntimeError("Propagation speed must be finite and positive");
    if (!std::isfinite(frequency.get<Hz>()) || frequency <= Hz(0))
        throw cRuntimeError("Carrier frequency must be finite and positive");
}

std::ostream& TgaxIndoorPathLoss::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "TgaxIndoorPathLoss";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(channelModel)
               << EV_FIELD(breakpointDistance);
    return stream;
}

double TgaxIndoorPathLoss::computeFreeSpacePathLoss(mps propagationSpeed, Hz frequency, m distance) const
{
    if (distance == m(0))
        return 1;
    auto waveLength = propagationSpeed / frequency;
    auto ratio = (waveLength / (4 * M_PI * distance)).get<unit>();
    return ratio * ratio;
}

double TgaxIndoorPathLoss::computePathLoss(mps propagationSpeed, Hz frequency, m distance) const
{
    validatePropagationParameters(propagationSpeed, frequency);
    if (std::isnan(distance.get<m>()) || distance < m(0))
        throw cRuntimeError("Propagation distance must be non-negative and not NaN");
    if (distance == m(0))
        return 1;
    if (std::isinf(distance.get<m>()))
        return 0;

    // IEEE 802.11-14/0882r4, Table III:
    // free-space propagation up to dBP, exponent 3.5 after dBP.
    if (distance <= breakpointDistance)
        return std::min(1.0, computeFreeSpacePathLoss(propagationSpeed, frequency, distance));
    else {
        auto breakpointPathLoss = std::min(1.0, computeFreeSpacePathLoss(propagationSpeed, frequency, breakpointDistance));
        return breakpointPathLoss * pow((breakpointDistance / distance).get<unit>(), 3.5);
    }
}

m TgaxIndoorPathLoss::computeRange(mps propagationSpeed, Hz frequency, double loss) const
{
    validatePropagationParameters(propagationSpeed, frequency);
    if (!std::isfinite(loss) || loss < 0 || loss > 1)
        throw cRuntimeError("Path loss factor must be finite and in the range [0, 1]");
    if (loss == 0)
        return m(INFINITY);

    auto waveLength = propagationSpeed / frequency;
    m unityGainDistance = waveLength / (4 * M_PI);
    auto breakpointPathLoss = std::min(1.0, computeFreeSpacePathLoss(propagationSpeed, frequency, breakpointDistance));
    if (loss == 1)
        return std::min(unityGainDistance, breakpointDistance);
    if (loss >= breakpointPathLoss)
        return waveLength / (4 * M_PI * sqrt(loss));
    else
        return breakpointDistance * pow(breakpointPathLoss / loss, 1.0 / 3.5);
}

} // namespace physicallayer

} // namespace inet
