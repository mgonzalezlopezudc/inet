//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixNoise.h"

#include <algorithm>
#include <cmath>

namespace inet {
namespace physicallayer {

ChannelMatrixNoise::Interferer::Interferer(int transmissionId, simtime_t startTime, simtime_t endTime,
    Hz centerFrequency, Hz bandwidth,
    const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
    const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& deterministicLargeScalePower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& physicalAggregatePower,
    const std::vector<ChannelMatrixResourceCell>& resourceCells) :
    transmissionId(transmissionId), startTime(startTime), endTime(endTime),
    centerFrequency(centerFrequency), bandwidth(bandwidth), snapshot(snapshot),
    spatialTransmissionPlan(spatialTransmissionPlan), deterministicLargeScalePower(deterministicLargeScalePower),
    physicalAggregatePower(physicalAggregatePower), resourceCells(resourceCells)
{
    if (startTime >= endTime || !std::isfinite(centerFrequency.get()) || centerFrequency <= Hz(0) ||
        !std::isfinite(bandwidth.get()) || bandwidth <= Hz(0))
        throw cRuntimeError("Channel-matrix interferer descriptor has an invalid interval or band");
    if (!snapshot || !spatialTransmissionPlan || !deterministicLargeScalePower || !physicalAggregatePower)
        throw cRuntimeError("Channel-matrix interferer descriptor requires non-null channel and power data");
    if (spatialTransmissionPlan->getNumberOfTransmitAntennas() != snapshot->getNumTransmitAntennas())
        throw cRuntimeError("Channel-matrix interferer spatial plan and snapshot dimensions disagree");
    const simtime_t duration = endTime - startTime;
    for (size_t index = 0; index < resourceCells.size(); index++) {
        const auto& resource = resourceCells[index];
        if (resource.getStartOffset() < SIMTIME_ZERO ||
            resource.getStartOffset() >= resource.getEndOffset() ||
            resource.getEndOffset() > duration ||
            resource.getLowerBasebandFrequency() < -bandwidth / 2 ||
            resource.getUpperBasebandFrequency() > bandwidth / 2 ||
            resource.getLowerBasebandFrequency() >= resource.getUpperBasebandFrequency() ||
            !std::isfinite(resource.getPowerSpectralDensityScale()) ||
            resource.getPowerSpectralDensityScale() <= 0)
            throw cRuntimeError("Channel-matrix interferer has an invalid technology resource");
        if (index != 0) {
            const auto& previous = resourceCells[index - 1];
            if (resource.getStartOffset() < previous.getStartOffset() ||
                (resource.getStartOffset() == previous.getStartOffset() &&
                 resource.getLowerBasebandFrequency() < previous.getLowerBasebandFrequency()))
                throw cRuntimeError("Channel-matrix interferer technology resources are not time/frequency ordered");
        }
    }
}

ChannelMatrixNoise::ChannelMatrixNoise(simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaAggregatePower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaBackgroundPower,
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& compatibilityBackgroundPower,
    const std::vector<Interferer>& interferers) :
    DimensionalNoise(startTime, endTime, centerFrequency, bandwidth, ccaAggregatePower),
    ccaBackgroundPower(ccaBackgroundPower), compatibilityBackgroundPower(compatibilityBackgroundPower),
    interferers(interferers)
{
    if (!ccaAggregatePower || !ccaBackgroundPower || !compatibilityBackgroundPower)
        throw cRuntimeError("Channel-matrix noise requires non-null power functions");
    std::sort(this->interferers.begin(), this->interferers.end(), [] (const Interferer& left, const Interferer& right) {
        return left.transmissionId < right.transmissionId;
    });
}

} // namespace physicallayer
} // namespace inet
