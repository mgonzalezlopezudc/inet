//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxUmiPathLoss.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace inet {
namespace physicallayer {

Define_Module(TgaxUmiPathLoss);

void TgaxUmiPathLoss::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL)
        configure(par("propagationCondition"), m(par("baseStationHeight")),
                m(par("userTerminalHeight")), par("enforceApplicabilityRange"));
}

void TgaxUmiPathLoss::configure(PropagationCondition propagationCondition,
        m baseStationHeight, m userTerminalHeight, bool enforceApplicabilityRange)
{
    if (!std::isfinite(baseStationHeight.get<m>()) || baseStationHeight <= m(1))
        throw cRuntimeError("UMi base-station height must be finite and greater than 1 m");
    if (!std::isfinite(userTerminalHeight.get<m>()) || userTerminalHeight <= m(1))
        throw cRuntimeError("UMi user-terminal height must be finite and greater than 1 m");
    this->propagationCondition = propagationCondition;
    this->baseStationHeight = baseStationHeight;
    this->userTerminalHeight = userTerminalHeight;
    this->enforceApplicabilityRange = enforceApplicabilityRange;
}

void TgaxUmiPathLoss::configure(const char *propagationCondition,
        m baseStationHeight, m userTerminalHeight, bool enforceApplicabilityRange)
{
    if (!strcmp(propagationCondition, "LOS"))
        configure(PropagationCondition::LOS, baseStationHeight, userTerminalHeight,
                enforceApplicabilityRange);
    else if (!strcmp(propagationCondition, "NLOS"))
        configure(PropagationCondition::NLOS, baseStationHeight, userTerminalHeight,
                enforceApplicabilityRange);
    else
        throw cRuntimeError("Unknown UMi propagation condition: '%s' (expected 'LOS' or 'NLOS')",
                propagationCondition);
}

void TgaxUmiPathLoss::validatePropagationParameters(mps propagationSpeed, Hz frequency) const
{
    if (!std::isfinite(propagationSpeed.get<mps>()) || propagationSpeed <= mps(0))
        throw cRuntimeError("Propagation speed must be finite and positive");
    if (!std::isfinite(frequency.get<Hz>()) || frequency <= Hz(0))
        throw cRuntimeError("Carrier frequency must be finite and positive");
    if (enforceApplicabilityRange && (frequency < GHz(2) || frequency > GHz(6)))
        throw cRuntimeError("The TGax UMi model is applicable from 2 GHz through 6 GHz");
}

void TgaxUmiPathLoss::validateDistance(m distance) const
{
    if (!std::isfinite(distance.get<m>()) || distance <= m(0))
        throw cRuntimeError("UMi propagation distance must be finite and positive");
    if (enforceApplicabilityRange) {
        auto maximumDistance = propagationCondition == PropagationCondition::LOS ? m(5000) : m(2000);
        if (distance < m(10) || distance > maximumDistance)
            throw cRuntimeError("UMi propagation distance is outside the condition's applicability range");
    }
}

m TgaxUmiPathLoss::computeBreakpointDistance(mps propagationSpeed, Hz frequency) const
{
    auto effectiveBaseStationHeight = baseStationHeight - m(1);
    auto effectiveUserTerminalHeight = userTerminalHeight - m(1);
    return 4.0 * effectiveBaseStationHeight * effectiveUserTerminalHeight * frequency / propagationSpeed;
}

double TgaxUmiPathLoss::computePathLossDb(mps propagationSpeed, Hz frequency, m distance) const
{
    double distanceMeters = distance.get<m>();
    double frequencyGhz = frequency.get<GHz>();
    if (propagationCondition == PropagationCondition::NLOS)
        return 36.7 * std::log10(distanceMeters) + 22.7 + 26 * std::log10(frequencyGhz);

    auto breakpointDistance = computeBreakpointDistance(propagationSpeed, frequency);
    if (distance < breakpointDistance)
        return 22 * std::log10(distanceMeters) + 28 + 20 * std::log10(frequencyGhz);
    else {
        double effectiveBaseStationHeight = (baseStationHeight - m(1)).get<m>();
        double effectiveUserTerminalHeight = (userTerminalHeight - m(1)).get<m>();
        // ITU-R M.2135-1, Table A1-2 is primary for the +2 log10(fc)
        // coefficient. IEEE 802.11-14/0882r4 reproduces it as +20, a known
        // transcription discrepancy in the TGax contribution.
        return 40 * std::log10(distanceMeters) + 7.8
                - 18 * std::log10(effectiveBaseStationHeight)
                - 18 * std::log10(effectiveUserTerminalHeight)
                + 2 * std::log10(frequencyGhz);
    }
}

double TgaxUmiPathLoss::computePathLoss(mps propagationSpeed, Hz frequency, m distance) const
{
    validatePropagationParameters(propagationSpeed, frequency);
    validateDistance(distance);
    return std::min(1.0, std::pow(10.0, -computePathLossDb(propagationSpeed, frequency, distance) / 10));
}

m TgaxUmiPathLoss::computeRange(mps propagationSpeed, Hz frequency, double loss) const
{
    validatePropagationParameters(propagationSpeed, frequency);
    if (!std::isfinite(loss) || loss < 0 || loss > 1)
        throw cRuntimeError("Path loss factor must be finite and in the range [0, 1]");
    if (loss == 0)
        return m(INFINITY);

    double pathLossDb = -10 * std::log10(loss);
    double frequencyGhz = frequency.get<GHz>();
    double distanceMeters;
    if (propagationCondition == PropagationCondition::NLOS)
        distanceMeters = std::pow(10.0,
                (pathLossDb - 22.7 - 26 * std::log10(frequencyGhz)) / 36.7);
    else {
        auto breakpointDistance = computeBreakpointDistance(propagationSpeed, frequency);
        double breakpointLoss = std::min(1.0, std::pow(10.0,
                -computePathLossDb(propagationSpeed, frequency, breakpointDistance) / 10));
        if (loss >= breakpointLoss)
            distanceMeters = std::pow(10.0,
                    (pathLossDb - 28 - 20 * std::log10(frequencyGhz)) / 22);
        else {
            double effectiveBaseStationHeight = (baseStationHeight - m(1)).get<m>();
            double effectiveUserTerminalHeight = (userTerminalHeight - m(1)).get<m>();
            distanceMeters = std::pow(10.0,
                    (pathLossDb - 7.8
                            + 18 * std::log10(effectiveBaseStationHeight)
                            + 18 * std::log10(effectiveUserTerminalHeight)
                            - 2 * std::log10(frequencyGhz)) / 40);
        }
    }
    m distance(distanceMeters);
    validateDistance(distance);
    return distance;
}

std::ostream& TgaxUmiPathLoss::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "TgaxUmiPathLoss";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(propagationCondition,
                propagationCondition == PropagationCondition::LOS ? "LOS" : "NLOS")
               << EV_FIELD(baseStationHeight)
               << EV_FIELD(userTerminalHeight)
               << EV_FIELD(enforceApplicabilityRange);
    return stream;
}

} // namespace physicallayer
} // namespace inet
