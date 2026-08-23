//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/pathloss/TgnIndoorPathLoss.h"

#include <algorithm>
#include <cmath>

namespace inet {
namespace physicallayer {

Define_Module(TgnIndoorPathLoss);

namespace {

constexpr double PI = 3.141592653589793238462643383279502884;

}

void TgnIndoorPathLoss::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        model = TgnChannelProfile::parseModel(par("profile"));
        TgnChannelProfile profile = TgnChannelProfile::create(model);
        breakpointDistance = m(profile.getBreakpointDistanceMeters());
        referenceDistance = m(par("referenceDistance"));
        if (!std::isfinite(referenceDistance.get()) || referenceDistance <= m(0))
            throw cRuntimeError("TGn reference distance must be finite and positive");
        if (referenceDistance > breakpointDistance)
            throw cRuntimeError("TGn reference distance %s exceeds profile %s breakpoint distance %s",
                referenceDistance.str().c_str(), TgnChannelProfile::getModelName(model), breakpointDistance.str().c_str());
    }
}

double TgnIndoorPathLoss::computeFreeSpacePowerGain(mps propagationSpeed, Hz frequency, m distance) const
{
    if (!std::isfinite(propagationSpeed.get()) || propagationSpeed <= mps(0) ||
        !std::isfinite(frequency.get()) || frequency <= Hz(0) ||
        !std::isfinite(distance.get()) || distance <= m(0))
        throw cRuntimeError("TGn path loss requires finite positive propagation speed, frequency, and distance");
    const double amplitude = propagationSpeed.get() / (4 * PI * frequency.get() * distance.get());
    return amplitude * amplitude;
}

double TgnIndoorPathLoss::computePathLoss(mps propagationSpeed, Hz frequency, m distance) const
{
    if (!std::isfinite(distance.get()) || distance < m(0))
        throw cRuntimeError("TGn path loss distance must be finite and nonnegative");
    // INET policy: distances below one configurable reference distance are clamped.
    const m effectiveDistance = std::max(distance, referenceDistance);
    if (effectiveDistance <= breakpointDistance)
        return computeFreeSpacePowerGain(propagationSpeed, frequency, effectiveDistance);
    const double breakpointGain = computeFreeSpacePowerGain(propagationSpeed, frequency, breakpointDistance);
    return breakpointGain * std::pow((effectiveDistance / breakpointDistance).get<unit>(), -3.5);
}

m TgnIndoorPathLoss::computeRange(mps propagationSpeed, Hz frequency, double loss) const
{
    if (!std::isfinite(loss) || loss <= 0 || loss > 1)
        throw cRuntimeError("TGn path-loss range requires a finite gain in (0, 1]");
    const double maximumModeledGain = computePathLoss(propagationSpeed, frequency, referenceDistance);
    if (loss > maximumModeledGain)
        return m(NaN);
    const double breakpointGain = computePathLoss(propagationSpeed, frequency, breakpointDistance);
    m distance;
    if (loss >= breakpointGain)
        distance = m(propagationSpeed.get() / (4 * PI * frequency.get() * std::sqrt(loss)));
    else
        distance = breakpointDistance * std::pow(breakpointGain / loss, 1.0 / 3.5);
    distance = std::max(distance, referenceDistance);
    const double reconstructed = computePathLoss(propagationSpeed, frequency, distance);
    if (std::abs(reconstructed - loss) > 1e-10 * std::max(loss, reconstructed))
        throw cRuntimeError("TGn path-loss inverse failed its forward consistency check");
    return distance;
}

std::ostream& TgnIndoorPathLoss::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "TgnIndoorPathLoss";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(model, TgnChannelProfile::getModelName(model))
               << EV_FIELD(breakpointDistance)
               << EV_FIELD(referenceDistance);
    return stream;
}

} // namespace physicallayer
} // namespace inet
